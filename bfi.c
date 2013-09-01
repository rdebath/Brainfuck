/*
This is a BF, compiler, interpreter and converter.
It can convert to C or x86 assembler or it can interpret the BF itself.
In addition, if configured it will use TCCLIB to run the C program.

In the simple case the C conversions are:

    >   becomes   ++p;			<   becomes   --p;	    
    +   becomes   ++*p;			-   becomes   --*p;	    
    .   becomes   putchar(*p);		,   becomes   *p = getchar();
    [   becomes   while (*p) {		]   becomes   }

Or
    [   becomes   if(!*p) goto match;	]   becomes   if(*p) goto match;

For best speed you should use an optimising C compiler like GCC, the 
interpreter loop especially requires perfect code generation for best speed.

The generated C code isn't quite so sensitve to the compiler used to
run it as some of the simple optimisations have already been done.
So reasonable performance can be got with TCC. GCC does, however, still
compile some constructs better.

Note: Telling this program the Cell size you will be using allows it 
to generate better code.

TODO:
    Note: With T_IF loops the condition may depend on the cell size.
	If so, within the loop the cell size may be known.

    UTF-8 output (just use putwchar() and getwchar() ?).

    New C function on Level 2 end while.

    #	Comment if at SOL
    %	Comment character til EOL
    //  Comment til EOL.

    [.] Comment at start of file.
*/ /*	C comments or REVERSED C comments ?

    ;	Input cell as a decimal NUMBER, trim initial whitespace.
    :	Output cell as a decimal NUMBER
    =	Set memory cell to zero. (macro for [-] )

    #	Debugging flag.

    !	On stdin for end of program / start of data.
    @	On stdin for end of program / start of data.

    \	Input next character as a literal. ie *m = 'next';
    "..."   Input characters from string on succesive calls. (pointer unchanged!)


    'Easy' mode, read from stdin single BF commands.
	"+-<>,." are executed immediatly.
	"]" is ignored.
	"[" reads source until matching "]" and optimises and executes loop.
	    Or discards it if it's a comment.

    Hard 'Easy' mode. Do initial loop read character by character.

*/
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <signal.h>
#include <locale.h>
#include <langinfo.h>

#ifdef MAP_NORESERVE
#define USEHUGERAM
#endif

#define USEPRINTF

#ifndef NOPCOUNT
/* 80=30, 0=30, 112=34, 24  */
/* #define NOPCOUNT 0 */
#endif

#ifdef USEHUGERAM
#define MEMSIZE	    2UL*1024*1024*1024
#define MEMGUARD    16UL*1024*1024
#define MEMSKIP	    1UL*1024*1024
#else
#define MEMSIZE 256*1024
#endif

char * program = "C";
int verbose = 0;
int noheader = 0;
int do_run = 0;
int do_C = 0;
int do_asm = 0;
int do_adder = 0;
int do_bf = 0;
int opt_level = 3;  /* 4 => Allow unsafe optimisations */
int output_limit = 0;
int enable_trace = 0;
int iostyle = 0;

int cell_size = 0;  /* 0=> 8,16,32 or more. 7 and up are number of bits */
int cell_mask = 0;
char *cell_type = "C";

FILE * ifd;
char * curfile = 0;
int curr_line = 0, curr_col = 0;
int cmd_line = 0, cmd_col = 0;

/* Note: Must match order of tokens below */
int cmd[] = {'>','+','.',',','[',']','<','-',EOF,0};

char *tokennames[] = {
#define T_MOV	0
    "T_MOV",
#define T_ADD	1
    "T_ADD",
#define T_PRT	2
    "T_PRT",
#define T_INP	3
    "T_INP",
#define T_WHL	4
    "T_WHL",
#define T_END	5
    "T_END",

#define T_LEFT	6   /* RLE use only, converted to T_MOV */
    /* SKIPPED */
#define T_DOWN	7   /* RLE use only, converted to T_ADD */
    /* SKIPPED */

/* The interpreter's switch likes a filled list of cases so start at 6. */
#define T_ZFIND	6  
    "T_ZFIND",
#define T_ADDWZ	7
    "T_ADDWZ",
#define T_STOP	8
    "T_STOP",
#define T_EQU	9
    "T_EQU",
#define T_IF	10
    "T_IF",
#define T_MULT	11
    "T_MULT",
#define T_CMULT	12
    "T_CMULT",
#define T_FOR	13
    "T_FOR",
#define T_SET	14
    "T_SET",
#define T_NOP	15
    "T_NOP",
#define T_DEAD	16
    "T_DEAD",
#define TCOUNT	17
    "T_?"
};

struct bfi
{
    struct bfi *next;
    int type;
    int count;
    int offset;
    int profile;
    struct bfi *last;

    int count2;
    int offset2;
    int count3;
    int offset3;

    int line, col;

    int orgtype;
    struct bfi *parent;
    struct bfi *prev;
} *bfprog = 0;

/* Stats */
double run_time = 0;
int loaded_nodes = 0;
int total_nodes = 0;
int node_type_counts[TCOUNT+1];
int min_pointer = 0, max_pointer = 0;
int most_negative_mov = 0, most_positive_mov = 0;
double profile_hits = 0.0;

#ifdef USEHUGERAM
/* Huge tape location */
void * cell_array_pointer = 0;
void * cell_array_low_addr = 0;
size_t cell_array_alloc_len = 0;
#endif

void open_file(char * fname);
void process_file(void);
void print_tree_stats(void);
void run_tree(void);
void run_ccode(void);
void print_asm();
void printccode(FILE * ofd);
void calculate_stats(void);
void pointer_scan(void);
void quick_scan(void);
void invariants_scan(void);
void trim_trailing_sets(void);
int find_known_state(struct bfi * v);
int flatten_loop(struct bfi * v, int constant_count);
int classify_loop(struct bfi * v);
int flatten_multiplier(struct bfi * v);
void * tcalloc(size_t nmemb, size_t size);
void print_adder(void);
void print_bf(void);
void * map_hugeram(void);
void trap_sigsegv(void);

void Usage(void)
{
    fprintf(stderr, "%s: Version 0.1.0\n", program);
    fprintf(stderr, "Usage: %s [options] [files]\n", program);
    fprintf(stderr, "   -h   This message.\n");
    fprintf(stderr, "   -v   Verbose, repeat for more.\n");
    fprintf(stderr, "   -r   Run in interpreter.\n");
    fprintf(stderr, "   -c   Create C code.\n");
    fprintf(stderr, "   -s   Create NASM code.\n");
    fprintf(stderr, "        Default is to generate & run the C code.\n");
    fprintf(stderr, "   -T   Create trace statements in output C code.\n");
    fprintf(stderr, "   -H   Remove headers in output code; optimiser cannot see start either.\n");
    fprintf(stderr, "   -L n Allow at most 'n' lines of output. (Interpreter)\n");
    fprintf(stderr, "   -u   Unicode I/O%s\n", iostyle?" (default)":"");
    fprintf(stderr, "   -a   Ascii I/O%s\n", iostyle?"":" (default)");
    fprintf(stderr, "   -m   Minimal processing, use RLE only.\n");
    fprintf(stderr, "   -On  'Optimisation level' -O0 is -m\n");
    fprintf(stderr, "   -O1      Only pointer motion removal.\n");
    fprintf(stderr, "   -O2      A few simple changes.\n");
    fprintf(stderr, "   -O3      Maximum normal level, default.\n");
    fprintf(stderr, "   -O4      Allow some 'unsafe' transformations.\n");
    fprintf(stderr, "   -B8  Use 8 bit cells.\n");
    fprintf(stderr, "   -B16 Use 16 bit cells.\n");
    fprintf(stderr, "   -B32 Use 32 bit cells.\n");
    fprintf(stderr, "        Default for C is 'unknown', ASM can only be 8bit.\n");
    fprintf(stderr, "        Other bitwidths work for the interpreter and C.\n");
    fprintf(stderr, "        Unicode characters need at least 16 bits.\n");
    exit(1);
}

void
UsageInt64()
{
    fprintf(stderr,
	"You cannot *specify* a cell size > %d bits on this machine.\n"
	"Compiled C code can use larger cells using -DC=intmax_t, but beware the\n"
	"optimiser may make changes that are unsafe for this code.\n",
	sizeof(int)*8);
    exit(1);
}

int
main(int argc, char ** argv)
{
    int ar, opton=1;
    char *p;
    char * filename = 0;
    program = argv[0];
    if (isatty(STDOUT_FILENO)) setbuf(stdout, NULL);
    setlocale(LC_ALL, "");
    if (!strcmp("UTF-8", nl_langinfo(CODESET)))
	iostyle = 1;

    /* Traditional option processing. */
    for(ar=1; ar<argc; ar++) 
        if(opton && argv[ar][0] == '-' && argv[ar][1] != 0) 
            for(p=argv[ar]+1;*p;p++)
                switch(*p) {
                    char ch, * ap;
                    case '-': opton = 0; break;
                    case 'v': verbose++; break;
                    case 'H': noheader=1; break;
                    case 'r': do_run=1; break;
                    case 'c': do_C=1; break;
                    case 's': do_asm=1; break;
                    case 'A': do_adder=1; break;
                    case 'F': do_bf=1; break;
                    case 'm': opt_level=0; break;
                    case 'T': enable_trace=1; break;
                    case 'u': iostyle=1; break;
                    case 'a': iostyle=0; break;

                    default: 
                        ch = *p;
#ifdef TAIL_ALWAYS_ARG
                        if (p[1]) { ap = p+1; p=" "; }
#else
                        if (p==argv[ar]+1 && p[1]) { ap = p+1; p=" "; }
#endif
                        else {
                            if (ar+1>=argc) Usage();
                            ap = argv[++ar];
                        }
                        switch(ch) {
                            case 'O': opt_level = strtol(ap,0,10); break;
                            case 'L': output_limit=strtol(ap,0,10); break;
                            case 'B':
				cell_size = strtol(ap,0,10);
				cell_mask = (1 << cell_size) - 1;
				if (cell_size < 7) {
				    fprintf(stderr, "Sorry bitfuck not implemented\n");
				    exit(1);
				} else if (cell_size <= 8)
				    cell_type = "char";
				else if (cell_size <= 16)
				    cell_type = "short";
				else if (cell_size > sizeof(int)*8) {
				    UsageInt64();
				} else
				    cell_type = "int";
				break;
                            default:  Usage();
                        }
                        break;
                }
        else {
	    curfile = argv[ar];

	    if (filename) {
		fprintf(stderr, "Only one file can be processed.\n");
		exit(1);
	    }

            if (opton && strcmp(argv[ar], "-") == 0)
                filename = "";
            else
                filename = argv[ar];
        }

    if (do_C + do_asm + do_bf + do_adder > 1) {
	fprintf(stderr, "The flags -c, -s, -A, and -F may not be combined.\n");
	exit(255);
    }

    if (do_asm && (do_C || do_run || cell_size) ) {
	fprintf(stderr, "The -s flag cannot be combined with -r, -c or -B\n");
	exit(255);
    }

    if (!do_run && !do_asm && !do_C && !do_adder && !do_bf)
	do_run = do_C = 1;

    if ((do_bf || do_adder) && opt_level > 2)
	opt_level = 2;

    if (do_asm) { cell_size = 8; cell_mask = 0xFF; cell_type = "char"; }

    if (cell_size == 0 && do_run) {
	if (iostyle == 1) {
	    cell_size = 16; cell_mask = 0xFFFF; cell_type = "short";
	} else {
	    cell_size = 8; cell_mask = 0xFF; cell_type = "char";
	}
    }

#ifdef USEHUGERAM
    trap_sigsegv();
#endif

    if( !filename || !*filename )
      open_file(0);
    else
      open_file(filename);

    exit(0);
}

void
open_file(char * fname)
{
    curr_line = curr_col = 1;
    if (fname == 0)
	ifd = stdin;
    else {
	ifd = fopen(fname, "r");
	if (!ifd) {
	    perror(fname);
	    exit(1);
	}
    }

    process_file();

    if (fname)
	fclose(ifd);
}

void *
tcalloc(size_t nmemb, size_t size)
{
    void * m;
    m = calloc(nmemb, size);
    if (m) return m;

    fprintf(stderr, "Allocate of %d*%d bytes failed, ABORT\n", nmemb, size);
    exit(42);
}

void 
printtreecell(FILE * efd, int indent, struct bfi * n)
{
#define CN(pv) (((int)(pv) - (int)bfprog) / sizeof(struct bfi))
    while(indent-->0) 
	fprintf(efd, " ");
    if (n == 0 ) {fprintf(efd, "NULL Cell"); return; }
    fprintf(efd, "Cell 0x%04x: ", CN(n));
    switch(n->type)
    {
    case T_MOV:
	if(n->offset == 0)
	    fprintf(efd, "T_MOV(%d), ", n->count);
	else
	    fprintf(efd, "T_MOV(%d,off=%d), ", n->count, n->offset);
	break;
    case T_ADD:
	fprintf(efd, "T_ADD[%d]+%d, ", n->offset, n->count);
	break;
    case T_EQU:
	fprintf(efd, "T_EQU[%d]=%d, ", n->offset, n->count);
	break;
    case T_SET:
	fprintf(efd, "T_SET[%d]=%d+[%d]*%d+[%d]*%d, ",
	    n->offset, n->count, n->offset2, n->count2, n->offset3, n->count3);
	break;
    case T_PRT:
	if (n->count == -1 )
	    fprintf(efd, "putch([%d]), ", n->offset);
	else {
	    if (n->count >= ' ' && n->count <= '~' && n->count != '"')
		fprintf(efd, "putch('%c'), ", n->count);
	    else
		fprintf(efd, "putch(0x%02x), ", n->count);
	}
	break;
    case T_INP: fprintf(efd, "[%d]=getch(), ", n->offset); break;
    case T_DEAD: fprintf(efd, "if(0) id=%d, ", n->count); break;
    case T_END: fprintf(efd, "end[%d] id=%d, ", n->offset, n->parent->count); break;
    case T_STOP: fprintf(efd, "stop[%d], ", n->offset); break;

    case T_ZFIND:
	fprintf(efd, "begin_zfind[%d],id=%d, ", n->offset, n->count);
	break;
    case T_ADDWZ:
	fprintf(efd, "begin_addw[%d],id=%d, ", n->offset, n->count);
	break;
    case T_WHL:
	fprintf(efd, "begin_std[%d],id=%d, ", n->offset, n->count);
	break;
    case T_FOR:
	fprintf(efd, "begin_for[%d],id=%d, ", n->offset, n->count);
	break;
    case T_IF:
	fprintf(efd, "begin_if[%d],id=%d, ", n->offset, n->count);
	break;
    case T_MULT:
	fprintf(efd, "begin_mult[%d],id=%d, ", n->offset, n->count);
	break;
    case T_CMULT:
	fprintf(efd, "begin_ifmult[%d],id=%d, ", n->offset, n->count);
	break;
    default:
	fprintf(efd, "type %d:", n->type);
	if(n->offset) 
	    fprintf(efd, "ptr+%d, ", n->offset);
	if(n->count != 1)
	    fprintf(efd, "*%d, ", n->count);
    }
    if(n->next == 0) 
	fprintf(efd, "next 0, ");
    else if(n->next->prev != n) 
	fprintf(efd, "next 0x%04x, ", CN(n->next));
    if(n->prev == 0)
	fprintf(efd, "prev 0, ");
    else if(n->prev->next != n) 
	fprintf(efd, "prev 0x%04x, ", CN(n->prev));
    if(n->parent) 
	fprintf(efd, "parent 0x%04x, ", CN(n->parent));
    if(n->last) 
	fprintf(efd, "last 0x%04x, ", CN(n->last));
    if(n->profile) 
	fprintf(efd, "prof %d, ", n->profile);
    if(n->line || n->col)
	fprintf(efd, "@(%d,%d)", n->line, n->col);
    else
	fprintf(efd, "@new");
#undef CN
}

void
printtree(void)
{
    int indent = 0;
    struct bfi * n = bfprog;
    fprintf(stderr, "Whole tree dump...\n");
    while(n)
    {
	if (indent>0 && n->orgtype == T_END) indent--;
	printtreecell(stderr, indent, n);
	fprintf(stderr, "\n");
	if (n->orgtype == T_WHL) indent++;
	n=n->next;
    }
}

void
pt(FILE* ofd, int indent, struct bfi * n)
{
    int i, j;
    for(j=(n==0 || !enable_trace); j<2; j++) {
	if (indent == 0)
	    fprintf(ofd, "  ");
	else for(i=0; i<indent; i++)
	    fprintf(ofd, "\t");

	if (j == 0) {
	    fprintf(ofd, "t(%d,%d,\"", n->line, n->col);
	    printtreecell(ofd, 0, n);
	    fprintf(ofd, "\",m+ %d)\n", n->offset);
	}
    }
}

void
print_c_header(FILE * ofd)
{
    char *main_name = "main";
    int use_full = 0;
    int memsize = 60000;
    int memoffset = 0;

    calculate_stats();

    if (enable_trace || do_run || iostyle) use_full = 1;

    if (total_nodes == node_type_counts[T_PRT] && !use_full) {
	fprintf(ofd, "#include <stdio.h>\n\n");
	fprintf(ofd, "int main(void)\n{\n");
	return ;
    }

    if (iostyle == 1) {
	fprintf(ofd, "#include <locale.h>\n");
	fprintf(ofd, "#include <wchar.h>\n");
    }

    fprintf(ofd, "#include <stdio.h>\n");
    fprintf(ofd, "#include <stdlib.h>\n");
    if (cell_size == 0) {
	fprintf(ofd, "# ifndef C\n");
	if (iostyle == 1)
	    fprintf(ofd, "# define C short\n");
	else
	    fprintf(ofd, "# define C char\n");
	fprintf(ofd, "# endif\n");
    }

    if (enable_trace)
	fputs(	"#define t(p1,p2,p3,p4) fprintf(stderr, \"P(%d,%d)=%s"
		" -> %d%s\\n\", p1, p2, p3,"
		" (p4>=mem?*(p4):0), p4>mem?\"\":\" SIGSEG\");\n",
	    ofd);

    if (node_type_counts[T_MOV] == 0) {
	if (min_pointer >= 0)
	    memsize = max_pointer+1;
	else {
	    memoffset = -min_pointer;
	    memsize = max_pointer-min_pointer+1;
	}
    }
    if (opt_level > 3) {
	if (memoffset == 0 && min_pointer < 0) {
	    memsize -= min_pointer;
	    memoffset -= min_pointer;
	}
	memsize -= most_negative_mov;
	memoffset -= most_negative_mov;
    }

    if (do_run) main_name = "run_bf";

    if (node_type_counts[T_MOV] == 0) {
	if (memoffset == 0)
	    fprintf(ofd, "%s m[%d];\n", cell_type, memsize);
	else {
	    fprintf(ofd, "%s mem[%d];\n", cell_type, memsize);
	    fprintf(ofd, "#define m (mem+%d)\n", memoffset);
	}
    } else
	fprintf(ofd, "%s *mem;\n", cell_type);
    fprintf(ofd, "int %s(){\n", main_name);

    if (node_type_counts[T_MOV] != 0) {

#if 0
	fprintf(ofd, "#if defined(__GNUC__) && defined(__i386__)\n");
	fprintf(ofd, "static %s * m asm (\"edx\");\n", cell_type);
	fprintf(ofd, "  m = mem = calloc(%d,sizeof*m);\n", memsize);
	fprintf(ofd, "#else\n");
	fprintf(ofd, "#endif\n");
#endif
	fprintf(ofd, "  %s*m = mem = calloc(%d,sizeof*m);\n",
			cell_type, memsize);
	if (memoffset > 0)
	    fprintf(ofd, "  m += %d;\n", memoffset);
    }

    if (node_type_counts[T_INP] != 0)
	fprintf(ofd, "  setbuf(stdout, 0);\n");

    if (iostyle == 1)
	fprintf(ofd, "  setlocale(LC_ALL, \"\");\n");
}

void 
printccode(FILE * ofd)
{
    int indent = 0;
    struct bfi * n = bfprog;
    char string_buffer[180], *sp = string_buffer, lastspch = 0;
    int ok_for_printf = 0, spc = 0;
    int multiply_mode = 0, multiply_cell = 0;
    int add_mask = 0;
    if (cell_size > 0 &&
	cell_size != sizeof(int)*8 &&
	cell_size != sizeof(short)*8 &&
	cell_size != sizeof(char)*8)
	add_mask = (1 << cell_size) - 1;

    if (verbose)
	fprintf(stderr, "Generating C Code.\n");

    if (!noheader)
	print_c_header(ofd);

    while(n)
    {
	if (n->type == T_PRT) 
	    ok_for_printf = (
		(
		    n->count >= ' ' && n->count <= '~'
#ifdef USEPRINTF
		    && n->count != '%'
#endif
		) || (n->count == '\n') || (n->count == '\033'));

	if (sp != string_buffer) {
	    if ((n->type != T_EQU && n->type != T_ADD && n->type != T_PRT) ||
	        (n->type == T_PRT && !ok_for_printf) ||
		(sp >= string_buffer + sizeof(string_buffer) - 8) ||
		(sp > string_buffer+1 && lastspch == '\n')
	       ) {
		*sp++ = 0;
		pt(ofd, indent,0);
#ifdef USEPRINTF
		fprintf(ofd, "printf(\"%s\");\n", string_buffer);
#else
		fprintf(ofd, "fwrite(\"%s\", 1, %d, stdout);\n", string_buffer, spc);
#endif
		sp = string_buffer; spc = 0;
	    }
	}

	if (n->orgtype == T_END) indent--;
	if (add_mask > 0) {
	    switch(n->type)
	    {
	    case T_WHL: case T_ZFIND: case T_IF:
	    case T_ADDWZ: case T_FOR: case T_CMULT:
		pt(ofd, indent, 0);
		fprintf(ofd, "m[%d] &= %d;\n", n->offset, add_mask);
	    }
	}
	switch(n->type)
	{
	case T_MOV:
	    pt(ofd, indent,n);
	    if (n->count < 0)
		fprintf(ofd, "m -= %d;\n", -n->count);
	    else if (n->count > 0)
		fprintf(ofd, "m += %d;\n", n->count);
	    if(n->offset != 0 || n->count == 0)
		fprintf(stderr, "WARN: T_MOV found with offset=%d, count=%d !\n",
			n->offset, n->count);
	    break;
	case T_ADD:
	    pt(ofd, indent-(multiply_mode ==2),n);
	    if(multiply_mode) 
	    {
		fprintf(ofd, "/*MM: m[%d] += m[%d] * %d; */ ",
			n->offset, multiply_cell, n->count);

		if (n->offset == multiply_cell && n->count == -1 )
		    fprintf(ofd, "m[%d] = 0;\n", n->offset);
		else if (n->count == 1)
		    fprintf(ofd, "m[%d] += m[%d];\n", n->offset, multiply_cell);
		else if (n->count == -1)
		    fprintf(ofd, "m[%d] -= m[%d];\n", n->offset, multiply_cell);
		else if (n->count > 0)
		    fprintf(ofd, "m[%d] += m[%d] * %d;\n",
			n->offset, multiply_cell, n->count);
		else if (n->count < 0)
		    fprintf(ofd, "m[%d] -= m[%d] * %d;\n",
			n->offset, multiply_cell, -n->count);
		else
		    fprintf(ofd, ";\n");
	    } else if(n->offset == 0) {
		if (n->count < 0)
		    fprintf(ofd, "*m -= %d;\n", -n->count);
		else if (n->count > 0)
		    fprintf(ofd, "*m += %d;\n", n->count);
		else
		    fprintf(ofd, ";\n");
	    } else {
		if (n->count < 0)
		    fprintf(ofd, "m[%d] -= %d;\n", n->offset, -n->count);
		else if (n->count > 0)
		    fprintf(ofd, "m[%d] += %d;\n", n->offset, n->count);
		else
		    fprintf(ofd, ";\n");
	    }
	    if (enable_trace) {
		pt(ofd, indent,0);
		fprintf(ofd, "t(%d,%d,\"\",m+ %d)\n", n->line, n->col, n->offset);
	    }
	    break;
	case T_EQU:
	    pt(ofd, indent-(multiply_mode ==2),n);
	    if (multiply_mode) fprintf(ofd, "/*MM*/ ");
	    if(n->offset == 0)
		fprintf(ofd, "*m = %d;\n", n->count);
	    else
		fprintf(ofd, "m[%d] = %d;\n", n->offset, n->count);
	    if (enable_trace) {
		pt(ofd, indent,0);
		fprintf(ofd, "t(%d,%d,\"\",m+ %d)\n", n->line, n->col, n->offset);
	    }
	    break;
	case T_SET:
	    pt(ofd, indent, n);
	    {
		int ac = '=';
		fprintf(ofd, "m[%d]", n->offset);
		if (n->count) {
		    fprintf(ofd, " %c %d ", n->count, ac);
		    ac = '+';
		}

		if (n->count2) {
		    int c = n->count2;
		    if (ac == '+' && c < 0) { c = -c; ac = '-'; };
		    if (ac == '=' && n->offset == n->offset2 && c == 1) {
			fprintf(ofd, " += ", ac, n->offset2);
			ac = 0;
		    } else {
			fprintf(ofd, " %c m[%d]", ac, n->offset2);
			ac = '+';
			if (c!=1) fprintf(ofd, "*%d", c);
		    }
		}

		if (n->count3) {
		    int c = n->count3;
		    if (ac == '+' && c < 0) { c = -c; ac = '-'; };
		    if (ac == 0)
			fprintf(ofd, "m[%d]", n->offset3);
		    else
			fprintf(ofd, " %c m[%d]", ac, n->offset3);
		    ac = '+';
		    if (c!=1) fprintf(ofd, "*%d", c);
		}
		if (ac == '=' || ac == 0)
		    fprintf(ofd, " = 0 /*T_SET zero!*/");

		if (enable_trace) {
		    fprintf(ofd, " /* ");
		    fprintf(ofd, "m[%d] = %d", n->offset, n->count);
		    fprintf(ofd, " + m[%d]*%d + m[%d]*%d",
			n->offset2, n->count2, n->offset3, n->count3);
		    fprintf(ofd, "; */");
		}

		fprintf(ofd, ";\n");
	    }

	    if (enable_trace) {
		pt(ofd, indent,0);
		fprintf(ofd, "t(%d,%d,\"\",m+ %d)\n", n->line, n->col, n->offset);
	    }
	    break;
	case T_PRT:
	    spc++;
	    if (ok_for_printf || (sp != string_buffer && n->count > 0)) {
		lastspch = n->count;
		if (add_mask>0)
		    lastspch &= add_mask;
		if (lastspch == '\n') {
		    *sp++ = '\\'; *sp++ = 'n';
		} else if (lastspch == '%') {
		    *sp++ = '%'; *sp++ = '%';
		} else if (lastspch == '\\' || lastspch == '"') {
		    *sp++ = '\\'; *sp++ = lastspch;
		} else if (lastspch >= ' ' && lastspch <= '~') {
		    *sp++ = lastspch;
		} else {
		    sp += sprintf(sp, "\\%03o", lastspch & 0xFF);
		}
		break;
	    }
	    spc--;
	    if (sp != string_buffer)
		fprintf(stderr, "Cannot add char %d to string\n", n->count);
	    pt(ofd, indent,n);
	    if (n->count == -1 ) {
		if (add_mask > 0 && add_mask < 0xFF) {
		    fprintf(ofd, "m[%d] &= %d;\n", n->offset, add_mask);
		    pt(ofd, indent, 0);
		}
		if (iostyle == 1)
		    fprintf(ofd, "printf(\"%%lc\",m[%d]);\n",
					    n->offset);
		else
		    fprintf(ofd, "putchar(m[%d]);\n", n->offset);
	    } else {
		int ch = n->count;
		if (add_mask>0) ch &= add_mask;
		if (ch >= ' ' && ch <= '~' &&
		    ch != '\'' && ch != '\\')
		    fprintf(ofd, "putchar('%c');\n", ch);
		else if (ch == '\n')
		    fprintf(ofd, "putchar('\\n');\n");
		else if (ch == '\t')
		    fprintf(ofd, "putchar('\\t');\n");
		else if (ch == '\\')
		    fprintf(ofd, "putchar('\\\\');\n");
		else if (iostyle == 1 && ch > 127)
		    fprintf(ofd, "printf(\"%%lc\",0x%02x);\n", ch);
		else
		    fprintf(ofd, "putchar(0x%02x);\n", ch);
	    }
	    break;
	case T_INP:
	    pt(ofd, indent,n);
	    fprintf(ofd, "{int c=get%schar();if(c!=EOF)m[%d]=c;}\n",
		(iostyle==1?"w":""), n->offset);
	    if (enable_trace) {
		pt(ofd, indent,0);
		fprintf(ofd, "t(%d,%d,\"\",m+ %d)\n", n->line, n->col, n->offset);
	    }
	    break;
	case T_IF:
	    pt(ofd, indent,n);
	    fprintf(ofd, "if(m[%d]) {\n", n->offset);
	    break;
	case T_ZFIND:
#if 0
	    if (n->next->count == 1 && add_mask <= 0) {
		pt(ofd, indent,n);
		fprintf(ofd, "if (sizeof*m == 1) {\n");
		pt(ofd, indent+1,n);
		fprintf(ofd, "m += strlen((char*)m + %d);\n", n->offset);
		pt(ofd, indent,n);
		fprintf(ofd, "} else\n");
	    }
#endif
	    pt(ofd, indent,n);
	    if (n->next->next != n->last) {
		fprintf(ofd, "while(m[%d]) { /*railrunner?*/\n", n->offset);
	    } else {
		fprintf(ofd, "while(m[%d]) { m += %d; }",
		    n->offset, n->next->count);
		n=n->last;
		fprintf(ofd, " /*railrunner*/\n", n->offset);
	    }
	    break;
	case T_WHL:
	    pt(ofd, indent,n);
	    fprintf(ofd, "while(m[%d]) {\n", n->offset);
	    break;
	case T_ADDWZ:
	    pt(ofd, indent,n);
	    fprintf(ofd, "while(m[%d]) { /*railcleaner*/\n", n->offset);
	    break;
	case T_FOR:
	    pt(ofd, indent,n);
	    fprintf(ofd, "for(;m[%d];) {\n", n->offset);
	    break;
	case T_CMULT:
	    multiply_mode = 1;
	    multiply_cell = n->offset;
	    pt(ofd, indent,n);
	    fprintf(ofd, "if(m[%d]) {\n", n->offset);
	    break;
	case T_MULT:
	    multiply_mode = 2;
	    multiply_cell = n->offset;
	    break;
	case T_END: 
	    if (multiply_mode != 2 ) {
		pt(ofd, indent,n);
		fprintf(ofd, "}\n");
	    }
	    multiply_mode = 0;
	    break;
	case T_STOP: 
	    pt(ofd, indent,n);
	    fprintf(ofd, "if(m[%d]) return 1;\n", n->offset);
	    break;
	case T_NOP: 
	    fprintf(stderr, "Warning on code generation: "
	           "NOP node: ptr+%d, cnt=%d, @(%d,%d).\n",
		    n->offset, n->count, n->line, n->col);
	    break;
	default:
	    pt(ofd, indent,n);
	    fprintf(ofd, "/* Bad node: type %d: ptr+%d, cnt=%d. */\n",
		    n->type, n->offset, n->count);
	    fprintf(stderr, "Error on code generation:\n"
	           "Bad node: type %d: ptr+%d, cnt=%d.\n",
		    n->type, n->offset, n->count);
	    break;
	}
	if (n->orgtype == T_WHL) indent++;
	n=n->next;
    }

    if(sp != string_buffer) {
	*sp++ = 0;
#ifdef USEPRINTF
	fprintf(ofd, "  printf(\"%s\");\n", string_buffer);
#else
	fprintf(ofd, "  fwrite(\"%s\", 1, %d, stdout);\n", string_buffer, spc);
#endif
    }
    if (!noheader)
	fprintf(ofd, "  return 0;\n}\n");
}

void
process_file(void)
{
    int c=0, count, loopid = 0, loops=0;
    int nextch, newcount = 0, newch = 0, newc = 0;
    struct bfi *n, *p;
    struct timeval tv_start, tv_end, tv_step;

    bfprog = n = p = 0;

    while((nextch = getc(ifd)) != EOF || newcount>0 || loops>0) {
	if (nextch == '\n') {
	    curr_line++; curr_col=0;
	} else 
	    curr_col++;
	if (newcount == 0) {
	    cmd_line = curr_line; cmd_col = curr_col;
	}

	/* RLE processing; this looks for runs of BF instructions.
	 * characters that aren't instructions are completely ignored,
	 * they don't even break a run. */
	for(c=0; cmd[c]; c++) { if(nextch == cmd[c]) break; }
	if (nextch != cmd[c]) continue;
	if (nextch == EOF && newcount == 0 && loops>0) {
	    /* Close any open loops at EOF. */
	    if (n->type != T_STOP) { newch = '#'; newc = T_STOP; newcount = 1;
	    } else { newch = ']'; newc = T_END; newcount = loops; }
	}
	if (newcount == 0) { newch = nextch; newc = c; newcount = 1; continue; }
	if (nextch == newch) { newcount++; continue; }
	ungetc(nextch, ifd); curr_col--;
	count = newcount; c = newc; 
	newch = newcount = newc = 0;
	/* c is the instruction (as 0-7) count is how many */

	while(count>0) {
	    loaded_nodes++;
	    if (n) {
		n->next = tcalloc(1, sizeof*n);
		p = n; n = n->next;
		n->parent = p->parent;
		n->prev = p;

		if (p->type == T_WHL) {
		    n->parent = p;
		} else if (p->type == T_END) {
		    if (p->parent) {
			n->parent = p->parent->parent;
			p->parent->parent = 0;
		    }
		} else
		    p->parent = 0;

		if (n->parent) n->parent->last = n;
	    } else {
		n = bfprog = tcalloc(1, sizeof*n);
		p = 0;
	    }
	    n->type = c;
	    n->orgtype = c;
	    n->line = cmd_line;
	    n->col = cmd_col;
	    if (c < 2) {
		n->count = count;
		count = 0;
	    } else if (c==T_LEFT || c==T_DOWN) {
		n->type = c-6;
		n->count = -count;
		count = 0;
	    } else {
		count--;
		if (c == T_WHL) { n->count = ++loopid; loops++; }
		if (c == T_PRT) n->count = -1;
	    }
	    if (c == T_END) {
		if (loops>0) loops--;
		else {
		    /* fprintf(stderr, "Warning: Too many end loops.\n"); */
		    n->type = T_STOP;
		}
	    }
	}
    }
    total_nodes = loaded_nodes;

    gettimeofday(&tv_start, 0);
    if (verbose>5) printtree();
    if (opt_level>=1) {
	if (verbose>2) gettimeofday(&tv_step, 0);
	pointer_scan(); /**/
	if (verbose>2) {
	    gettimeofday(&tv_end, 0);
	    fprintf(stderr, "Time for pointer scan %.3f\n", 
		(tv_end.tv_sec - tv_step.tv_sec) +
		(tv_end.tv_usec - tv_step.tv_usec)/1000000.0);
	}
	if (opt_level>=2) {
	    calculate_stats();
	    if (verbose>5) printtree();
	    if (verbose>2) gettimeofday(&tv_step, 0);
	    invariants_scan(); /**/
	    if (verbose>2) {
		gettimeofday(&tv_end, 0);
		fprintf(stderr, "Time for invariant scan %.3f\n", 
		    (tv_end.tv_sec - tv_step.tv_sec) +
		    (tv_end.tv_usec - tv_step.tv_usec)/1000000.0);
	    }
	    if (verbose>2) gettimeofday(&tv_step, 0);
	    if (!noheader)
		trim_trailing_sets(); /**/
	    if (verbose>2) {
		gettimeofday(&tv_end, 0);
		fprintf(stderr, "Time for trailing scan %.3f\n", 
		    (tv_end.tv_sec - tv_step.tv_sec) +
		    (tv_end.tv_usec - tv_step.tv_usec)/1000000.0);
	    }
	}
	if (!do_adder && !do_bf) {
	    if (verbose>2) gettimeofday(&tv_step, 0);
	    quick_scan(); /**/
	    if (verbose>2) {
		gettimeofday(&tv_end, 0);
		fprintf(stderr, "Time for quick scan %.3f\n", 
		    (tv_end.tv_sec - tv_step.tv_sec) +
		    (tv_end.tv_usec - tv_step.tv_usec)/1000000.0);
	    }
	}
	if (verbose>5) printtree();
    }
    if (verbose && !do_run)
	print_tree_stats();

    if (do_asm) print_asm();
    if (do_C && do_run) {
#ifdef TCC_OUTPUT_MEMORY
	run_ccode();
#else
	run_tree();
#endif
    } else {
	if (do_C) printccode(stdout);
	if (do_run) run_tree();
	if (do_adder) print_adder();
	if (do_bf) print_bf();
    }

    if (verbose && do_run)
	print_tree_stats();
    if (verbose>2 && (do_run || verbose<6))
	printtree();
}

void
calculate_stats(void)
{
    struct bfi * n = bfprog;
    int i;

    total_nodes = 0;
    min_pointer = 0;
    max_pointer = 0;
    most_negative_mov = 0;
    most_positive_mov = 0;
    profile_hits = 0;

    for(i=0; i<TCOUNT+1; i++)
	node_type_counts[i] = 0;

    while(n)
    {
	int t = n->type;
	total_nodes ++;
	if (t < 0 || t >= TCOUNT) 
	    node_type_counts[TCOUNT] ++;
	else
	    node_type_counts[t] ++;
	if (t == T_MOV) {
	    if (n->count < most_negative_mov) 
		most_negative_mov = n->count;
	    if (n->count > most_positive_mov) 
		most_positive_mov = n->count;
	}
	if (min_pointer > n->offset) min_pointer = n->offset;
	if (max_pointer < n->offset) max_pointer = n->offset;
	if (n->profile)
	    profile_hits += n->profile;
	n=n->next;
    }
}

void
print_tree_stats(void)
{
    int i;
    calculate_stats();

    fprintf(stderr, "Total nodes %d, loaded %d", total_nodes, loaded_nodes);
    fprintf(stderr, " (%dk)", (loaded_nodes * sizeof(struct bfi) +1023) / 1024);
    fprintf(stderr, ", Offset range %d..%d\n", min_pointer, max_pointer);
    if (profile_hits > 0)
	fprintf(stderr, "Run time %.4fs, cycles %.0f, %.3fns/cycle\n",
	    run_time, profile_hits, 1000000000.0*run_time/profile_hits);
    fprintf(stderr, "Tokens");
    for(i=0; i<TCOUNT; i++)
	if (node_type_counts[i])
	    fprintf(stderr, ", %s=%d", tokennames[i], node_type_counts[i]);
    fprintf(stderr, "\n");
}

#if MASK == 0xFF
#define icell	unsigned char
#define M(x) x
#elif MASK == 0xFFFF
#define icell	unsigned short
#define M(x) x
#else
#define icell	int
#endif
void
run_tree(void)
{
    icell *p, *oldp;
    int use_free = 0;
    struct bfi * n = bfprog;
    int c;
    struct timeval tv_start, tv_end, tv_pause;
#ifdef M
    if (sizeof(*p)*8 != cell_size && cell_size>0) {
	fprintf(stderr, "Sorry, the interpreter has been configured with a fixed cell size of %d bits\n", sizeof(icell)*8);
	exit(1);
    }
#else
    int icell_mask;
    if (cell_size == 0) icell_mask = 0xFF;
    else if (cell_size >= sizeof(*p)*8) 
	icell_mask = -1;
    else
	icell_mask = (1 << cell_size) - 1;
#define M(x) ((x)&icell_mask)
#endif

#ifdef USEHUGERAM
    oldp = p = map_hugeram();
#else
    p = oldp = tcalloc(MEMSIZE, sizeof*p);
    if (p == 0) {
	fprintf(stderr, "Cannot allocate memory for cell array\n");
	exit(1);
    }
    use_free = 1;
#endif

    if (verbose)
	fprintf(stderr, "Starting interpreter\n");
    gettimeofday(&tv_start, 0);

#include "nops.h"

    while(n){
	n->profile++;
	switch(n->type)
	{
	    case T_MOV: p += n->count; break;
	    case T_ADD: p[n->offset] += n->count; break;
	    case T_EQU: p[n->offset] = n->count; break;
	    case T_SET:
		p[n->offset] = n->count
			    + n->count2 * p[n->offset2]
			    + n->count3 * p[n->offset3];
		break;

	    case T_MULT: case T_CMULT:
	    case T_IF: case T_FOR:

	    case T_WHL: if(M(p[n->offset]) == 0) n=n->last;
		break;

	    case T_END: if(M(p[n->offset]) != 0) n=n->parent;
		break;

	    case T_PRT:
	    {
		int ch = M(n->count == -1?p[n->offset]:n->count);
		if (ch == '\n' && output_limit && --output_limit == 0) {
		    fprintf(stderr, "Output limit reached\n");
		    goto break_break;
		}

		/*TODO: stop the clock ? */

		if (ch > 127 && iostyle == 1)
		    printf("%lc", ch);
		else
		    putchar(ch);
		break;
	    }

	    case T_INP:
		gettimeofday(&tv_pause, 0); /* Stop the clock. */

		do {
		    if (iostyle == 1) {
			wchar_t wch = EOF;
			if (scanf("%lc", &wch))
			    c = wch;
			else
			    c = EOF;
		    } else
			c=getchar();
		} while(c == '\r');
		if (c != EOF) p[n->offset] = c;

		gettimeofday(&tv_end, 0);
		tv_start.tv_usec = tv_start.tv_usec +
				(tv_end.tv_usec - tv_pause.tv_usec);
		tv_start.tv_sec = tv_start.tv_sec +
				(tv_end.tv_sec - tv_pause.tv_sec);
		while (tv_start.tv_usec < 0) {
		    tv_start.tv_usec += 1000000;
		    tv_start.tv_sec -= 1;
		}
		break;

	    case T_ZFIND:
		/* Found a zip whizz */
		if(M(p[n->offset]) == 0) { n=n->last; break; }
#if 0
		/* Too much overhead with strlen */
		if (n->next->count == 1 && sizeof*p == 1)
		    /* Cast is to stop the whinge, if p isn't a char* this
		     * is dead code. */
		    p += strlen((char*)p + n->offset);
		else
#endif
		    while(M(p[n->offset]))
			p += n->next->count;
		n=n->last;
		break;

	    case T_ADDWZ:
		if(M(p[n->offset]) == 0) { n=n->last; break; }
		{
		    int noff, n2off, n2count, n3count;
		    noff = n->offset;
		    n2off = n->next->offset;
		    n2count = n->next->count;
		    n3count = n->next->next->count;
		    /* This is a running add. */
		    while(p[noff]) {
			p[n2off] += n2count;
			p += n3count;
		    }
		}
		n=n->last;
		break;

	    case T_STOP:
		if(M(p[n->offset]) != 0) {
		    if (verbose)
			fprintf(stderr, "STOP Command executed.\n");
		    return;
		}
		break;

	    case T_NOP:
		break;

	    default:
		fprintf(stderr, "Execution error:\n"
		       "Bad node: type %d: ptr+%d, cnt=%d.\n",
			n->type, n->offset, n->count);
		exit(1);
	}
	n = n->next;
    }

break_break:;
    gettimeofday(&tv_end, 0);
    run_time = (tv_end.tv_sec + tv_end.tv_usec/1000000.0);
    run_time = run_time - (tv_start.tv_sec + tv_start.tv_usec/1000000.0);
    if (verbose)
	fprintf(stderr, "Run time (excluding inputs) %.4fs\n", run_time);

    if (use_free) free(oldp);

#undef icell
}

void
pointer_scan(void)
{
    struct bfi * n = bfprog, *n2, *n3, *n4;

    if (verbose>1)
	fprintf(stderr, "Merging pointer movements.\n");

    while(n){

	if (verbose>4 && n->type == T_MOV) {
	    fprintf(stderr, "Checking pointer ...\n");
	    printtreecell(stderr, 1, n);
	    fprintf(stderr,  " prev ...\n");
	    printtreecell(stderr, 2, n->prev);
	    fprintf(stderr,  " next ...\n");
	    printtreecell(stderr, 2, n->next);
	    if (n->next) {
		printtreecell(stderr, 4, n->next->next);
		if (n->next && n->next->last) {
		    printtreecell(stderr, 4, n->next->last->prev);
		    printtreecell(stderr, 4, n->next->last);
		}
	    }
	    fprintf(stderr, "...\n");
	}

	if (n->type == T_MOV && n->next) {
	    if (n->next->type == T_MOV) {
		if(verbose>4)
		    fprintf(stderr, "Merge movements.\n");
		/* Merge movments */
		n2 = n->next;
		n->count += n2->count;
		n->next = n2->next;
		if (n2->next) n2->next->prev = n;
		free(n2);
		if (n->count == 0 && n->prev) {
		    n2 = n->prev;
		    n2->next = n->next;
		    if (n->next) n->next->prev = n2;
		    free(n);
		    n = n2;
		}
		continue;
	    } else if (n->next->type != T_WHL && n->next->type != T_END) {
		if(verbose>4)
		    fprintf(stderr, "Push past command.\n");
		/* Put movement after a normal cmd. */
		n2 = n->next;
		n2->offset += n->count;
		n3 = n->prev;
		n4 = n2->next;
		if(n3) n3->next = n2; else bfprog = n2;
		n2->next = n;
		n->next = n4;
		n2->prev = n3;
		n->prev = n2;
		if(n4) n4->prev = n;
		continue;
	    } else if (n->next->type == T_WHL) {
		if(verbose>4)
		    fprintf(stderr, "Push round a loop.\n");
		/* Push this record past the entire loop */
		/* Add two balancing records inside the loop at the start
		 * and end. Hopefully they'll cancel. */

		/* Insert record at loop start */
		n2 = n->next;
		n2->offset += n->count;
		n3 = n2->next;
		n4 = tcalloc(1, sizeof*n);
		n4->line = n2->line;
		n4->col = n2->col;
		n2->next = n4;
		n3->prev = n4;
		n4->next = n3;
		n4->prev = n2;
		n4->type = n->type;
		n4->count = n->count;

		/* Insert record at loop end */
		n2 = n->next->last->prev;
		n3 = n2->next;
		n4 = tcalloc(1, sizeof*n);
		n4->line = n3->line;
		n4->col = n3->col;
		n2->next = n4;
		n3->prev = n4;
		n4->next = n3;
		n4->prev = n2;
		n4->type = n->type;
		n4->count = -n->count;

		/* Move this record past loop */
		n2 = n->prev;
		if(n2) n2->next = n->next; else bfprog = n->next;
		if (n->next) n->next->prev = n2;

		n4 = n;
		n2 = n->next->last;
		n3 = n2->next;
		n = n->next;
		n2->offset += n4->count;

		n2->next = n4;
		if(n3) n3->prev = n4;
		n4->next = n3;
		n4->prev = n2;

	    }
	}
	/* Push it off the end of the program */
	if (n->type == T_MOV && n->next == 0 && n->prev) {
	    if(verbose>4)
		fprintf(stderr, "Pushed to end of program.\n");
	    if (!noheader) {
		n->prev->next = 0;
		free(n);
		n = 0;
		continue;
	    }
	}
	n=n->next;
    }
    if(verbose>4)
	fprintf(stderr, "Pointer scan complete.\n");
}

void 
quick_scan(void)
{
    struct bfi * n = bfprog, *n2;

    if (verbose>1)
	fprintf(stderr, "Finding 'quick' commands.\n");

    while(n){
	/* Special tight loops for the interpreter; note, the compilers
	   shouldn't need them because the code will already do a tight loop. */
	if (n->type == T_WHL) {
	    if ( n->next->type == T_MOV &&
		 n->next->next->type == T_END)
		n->type = T_ZFIND;
	    if ( n->next->type == T_ADD &&
		 n->next->next->type == T_MOV &&
		 n->next->next->next->type == T_END)
		n->type = T_ADDWZ;

	    if(verbose>4 && n->type != T_WHL) {
		fprintf(stderr, "Replaced loop type.\n");
		printtreecell(stderr, 1, n);
		fprintf(stderr, "\n");
	    }
	}

	/* Looking for "[-]" or "[+]" */
	if( n->type == T_WHL && n->next && n->next->next &&
	    n->next->type == T_ADD &&
	    n->next->next->type == T_END &&
	    ( n->next->count == 1 || n->next->count == -1 )
	    ) {
	    /* Change to T_EQU */
	    n->next->type = T_EQU; 
	    n->next->count = 0;

	    /* Delete the loop. */
	    n->type = T_NOP;
	    n->next->next->type = T_NOP;

	    if(verbose>4 && n->type == T_NOP) {
		fprintf(stderr, "Replaced loop with set.\n");
		printtreecell(stderr, 1, n->next);
		fprintf(stderr, "\n");
	    }
	}

	if (n && n->type == T_NOP) {
	    n2 = n; n = n->next;
	    if(n2->prev) n2->prev->next = n; else bfprog = n;
	    if(n) n->prev = n2->prev;
	    free(n2);
	    continue;
	}
	n=n->next;
    }
}

void
invariants_scan(void)
{
    struct bfi * n = bfprog, *n2, *n3, *n4;
    int node_changed;

    if (verbose>1)
	fprintf(stderr, "Scanning for invariant code.\n");

    while(n){
	node_changed = 0;

	switch(n->type) {
	case T_PRT:
	    if (n->count == -1)
		node_changed = find_known_state(n);
	    break;

	case T_EQU:
	case T_ADD:
	case T_WHL:
	    node_changed = find_known_state(n);
	    break;

	case T_END:
	    n2 = n->parent;
	    node_changed = find_known_state(n2);
	    if (!node_changed)
		node_changed = find_known_state(n);
	    if (!node_changed)
		node_changed = classify_loop(n2);
	    if (!node_changed)
		node_changed = flatten_multiplier(n2);

	    if (node_changed)
		n = n2;
	}

	if (n && n->type == T_DEAD && n->last) {
	    n2 = n->last;
	    n3 = n;
	    n = n2->next;
	    if (n3->prev) n3->prev->next = n; else bfprog = n;
	    if (n) n->prev = n3->prev;

	    n2->next = 0;
	    while(n3) {
		n4 = n3->next;
		free(n3);
		n3 = n4;
	    }
	    node_changed = 1;
	}

	if (n && n->type == T_NOP) {
	    n2 = n; n = n->next;
	    if(n2->prev) n2->prev->next = n; else bfprog = n;
	    if(n) n->prev = n2->prev;
	    free(n2);
	    node_changed = 1;
	}

	if (!node_changed) {
	    n=n->next;
	}
    }
}

void
trim_trailing_sets(void) {
    struct bfi * n = bfprog, *lastn = 0;
    int min_pointer = 0, max_pointer = 0, jmp_count = 0;
    int node_changed, i;
    while(n)
    {
	if (n->type == T_MOV)
	    jmp_count ++;
	if (min_pointer > n->offset) min_pointer = n->offset;
	if (max_pointer < n->offset) max_pointer = n->offset;
	lastn = n;
	n=n->next;
    }
    if (jmp_count || max_pointer - min_pointer > 32) return;

    if (lastn) {
	n = tcalloc(1, sizeof*n);
	lastn->next = n;

	for(i=min_pointer; i<= max_pointer; i++) {
	    n->prev = lastn;
	    n->type = T_EQU;
	    n->count = 0;
	    n->offset = i;
	    node_changed = find_known_state(n);
	    if (n->prev == 0) {
		if (bfprog == n) {
		    bfprog = n->next;
		    free(n);
		}
		return;
	    }
	}
	lastn->next = 0;
	free(n);
    }
}

int
find_known_state(struct bfi * v)
{
    struct bfi *n = v;
    int unmatched_ends = 0;
    int const_found = 0, known_value = 0, non_zero_unsafe = 0;
    int n_used = 0;
    int distance = 0;

    if(n == 0) return 0;
    n = n->prev;
    while(n)
    {
	switch(n->type)
	{
	case T_NOP:
	    break;

	case T_EQU:
	    if (n->offset == v->offset) {
		/* We don't know if this node will be hit */
		if (unmatched_ends) goto break_break;

		known_value = n->count;
		const_found = 1;
		goto break_break;
	    }
	    break;

	case T_SET:
	    if (n->offset == v->offset)
		goto break_break;
	    if (n->offset2 == v->offset || n->offset3 == v->offset)
		n_used = 1;
	    break;

	case T_PRT:
	    if (n->count == -1 && n->offset == v->offset) {
		n_used = 1;
		goto break_break;
	    }
	    break;

	case T_INP:
	    if (n->offset == v->offset) {
		n_used = 1;
		goto break_break;
	    }
	    break;

	case T_ADD:	/* Possible instruction merge for T_ADD */
	    if (n->offset == v->offset)
		goto break_break;
	    break;

	case T_END:
	    if (n->offset == v->offset) {
		/* We don't know if this node will be hit */
		if (unmatched_ends) goto break_break;

		known_value = 0;
		const_found = 1;
		n_used = 1;
		goto break_break;
	    }
	    unmatched_ends++;
	    break;

	case T_IF:
	    /* Can search past an IF because it has only one entry point. */
	    if (n->offset == v->offset)
		n_used = 1;
	    if (unmatched_ends>0)
		unmatched_ends--;
	    else
		n_used = 1; /* All MAY be used because we might get skipped */
	    break;

	case T_MULT:
	case T_CMULT:
	case T_WHL:
	case T_FOR:
	    if (n->offset == v->offset)
		n_used = 1;
	    if (unmatched_ends>0)
		unmatched_ends--;
#if 0
	    if ( ??? ) break;
#endif

	case T_MOV:
	default:
	    goto break_break;
	}
	n=n->prev;

	/* How did we get this far! */
	if (++distance > 200) goto break_break;
    }

    known_value = 0;
    const_found = (!noheader);
    n_used = 1;

break_break:
    if (const_found && cell_size == 0
	&& known_value != 0 && (known_value&0xFF) == 0) {
	/* Only known if we know the final cell size so ... */
	non_zero_unsafe = 1;
	/* The problem also occurs for other values, but only zero can
	 * be used to detect dead code and change the flow of control.
	 * all other substitutions are cell size agnostic. */
    }
    if (cell_mask > 0)
	known_value &= cell_mask;

#if 1
    if (verbose>5) {
	if (const_found)
	    fprintf(stderr, "Known value is %d%s for ",
		    known_value, non_zero_unsafe?" (unsafe zero)":"");
	else
	    fprintf(stderr, "Unknown value for ");
	printtreecell(stderr, 0,v);
	fprintf(stderr, "\n  From ");
	printtreecell(stderr, 0,n);
	fprintf(stderr, "\n");
    }
#endif

    if (!const_found) {
	switch(v->type) {
	    case T_ADD:
		if (n && n->type == T_ADD && n->offset == v->offset &&
			!unmatched_ends && !n_used) {
		    n->count += v->count;
		    v->type = T_NOP;
		    if (verbose>5) fprintf(stderr, "  Delete this node.\n");
		    return 1;
		}
		break;
	    case T_EQU:
		if (n && n->type == T_ADD && n->offset == v->offset &&
			!unmatched_ends && !n_used) {

		    struct bfi *n2;

		    n->type = T_NOP;
		    n2 = n; n = n->prev;
		    if (n) n->next = n2->next; else bfprog = n2->next;
		    if (n2->next) n2->next->prev = n;
		    free(n2);
		    if (verbose>5) fprintf(stderr, "  Delete old node.\n");
		    return 1;
		}
		break;
	}

	return 0;
    }

    switch(v->type) {
	case T_ADD: /* Promote to SET. */
	    v->type = T_EQU;
	    v->count += known_value;
	    if (cell_mask > 0)
		v->count &= cell_mask;
	    if (verbose>5) fprintf(stderr, "  Upgrade to T_EQU.\n");
	    /*FALLTHROUGH*/

	case T_EQU: /* Delete if same or unused */
	    if (known_value == v->count && (n || distance < 100)) {
		v->type = T_NOP;
		if (verbose>5) fprintf(stderr, "  Delete this T_EQU.\n");
		return 1;
	    } else if (n && n->type == T_EQU && !n_used) {
		struct bfi *n2;

		n->type = T_NOP;
		n2 = n; n = n->prev;
		if (n) n->next = n2->next; else bfprog = n2->next;
		if (n2->next) n2->next->prev = n;
		free(n2);
		if (verbose>5) fprintf(stderr, "  Delete old T_EQU.\n");
		return 1;
	    }
	    break;

	case T_PRT: /* Print literal character. */
	    if (v->count != -1) break;
	    if (do_bf) break; /* BF output can't do lits. */
	    if (known_value < 0 && iostyle == 0)
		known_value = (0xFF & known_value);
	    if (known_value == -1) break;
	    v->count = known_value;
	    if (verbose>5) fprintf(stderr, "  Make literal putchar.\n");
	    return 0; /* No need to rescan or fix this node */

	case T_IF:
	case T_FOR:
	case T_MULT:
	case T_CMULT:
	case T_WHL: /* Change to (possible) constant loop or dead code. */
	    if (!non_zero_unsafe) {
		if (known_value == 0) {
		    v->type = T_DEAD;
		    if (verbose>5) fprintf(stderr, "  Dead loop.\n");
		    return 1;
		}

		if (v->type == T_IF ) {
		    v->type = T_NOP;
		    v->last->type = T_NOP;
		    if (verbose>5) fprintf(stderr, "  Change to If loop.\n");
		    return 1;
		}
	    }
	    if (v->type != T_IF)
		return flatten_loop(v, known_value);
	    break;

	case T_END: /* Change to IF command or infinite loop (error exit) */
	    if (!non_zero_unsafe) {
		if (known_value == 0 && v->parent->type != T_IF) {
		    v->parent->type = T_IF;
		    if (verbose>5) fprintf(stderr, "  Change to If loop.\n");
		    return 1;
		}
	    }
	    break;

	default:
	    break;
    }

#if 0
    if (verbose>5)
	printtreecell(2,v);
#endif
    return 0;
}

int
flatten_loop(struct bfi * v, int constant_count)
{
    struct bfi *n, *dec_node = 0;
    int is_znode = 0;

    n = v->next;
    while(1)
    {
	if (n == 0) return 0;
	if (n->type == T_END) break;
	if (n->type != T_ADD && n->type != T_EQU) return 0;
	if (n->offset == v->offset) {
	    if (dec_node) return 0; /* Two updates, hmmm */
	    if (n->type != T_ADD || n->count != -1) {
		if (n->type != T_EQU || n->count != 0) return 0;
		is_znode = 1;
	    }
	    dec_node = n;
	}
	n=n->next;
    }
    if (dec_node == 0) return 0;

    /* Found a loop with a known value at entry that contains only T_ADD and
     * T_EQU where the loop variable is decremented nicely. GOOD! */

    if (is_znode) 
	constant_count = 1;

    if (verbose>5) fprintf(stderr, "  Loop flattened.\n");
    n = v->next;
    while(1)
    {
	if (n->type == T_END) break;
	if (n->type == T_ADD)
	    n->count = n->count * constant_count;
	n=n->next;
    }

    n->type = T_NOP;
    v->type = T_NOP;

    return 1;
}

int
classify_loop(struct bfi * v)
{
    struct bfi *n, *dec_node = 0;
    int is_znode = 0;
    int has_set = 0;
    int has_add = 0;
    int is_loop_only = 1;
    int typewas;
    int has_belowloop = 0;
    int has_negoff = 0;
    int complex_loop = 0;

    if (opt_level < 3) return 0;
    if (!v || v->orgtype != T_WHL) return 0;
    typewas = v->type;
    n = v->next;
    while(n != v->last)
    {
	if (n == 0 || n->type == T_MOV) return 0;
	if (n->type != T_ADD && n->type != T_EQU)
	    complex_loop = 1;
	else {
	    if (n->offset == v->offset) {
		if (dec_node) return 0; /* Two updates, nope! */
		if (n->type != T_ADD || n->count != -1) {
		    if (n->type != T_EQU || n->count != 0) return 0;
		    is_znode = 1;
		}
		dec_node = n;
	    } else {
		if (n->offset < v->offset) has_belowloop = 1;
		if (n->offset < 0) has_negoff = 1;
		if (n->type == T_ADD) has_add = 1;
		if (n->type == T_EQU) has_set = 1;
		is_loop_only = 0;
	    }
	}
	n=n->next;
    }
    if (dec_node == 0) return 0;

    if (complex_loop) {
	/* Complex contents but loop is simple so call it a for (or better) */
	if (v->type == T_WHL)
	    v->type = T_FOR;
	return (v->type != typewas);
    }

    /* Found a loop that contains only T_ADD and T_EQU where the loop
     * variable is decremented nicely. This is a multiply loop. */

    /* The loop decrement isn't at the end; move it there. */
    if (dec_node != v->last->prev) {
	struct bfi *n1, *n2;

	/* Snip it out of the loop */
	n1 = dec_node;
	n2 = n1->prev;
	n2->next = n1->next;
	n2 = n1->next;
	n2->prev = n1->prev;

	/* And put it back */
	n2 = v->last->prev;
	n1->next = n2->next;
	n1->prev = n2;
	n2->next = n1;
	n1->next->prev = n1;
    }

    if (!has_add && !has_set) {
	n->type = T_NOP;
	v->type = T_NOP;
	dec_node->type = T_EQU;
	dec_node->count = 0;
    } else if (is_znode) {
	v->type = T_IF;
    } else {
	v->type = T_CMULT;
	/* Without T_EQU if all offsets >= Loop offset we don't need the if. */
	/* BUT: Any access below the loop can possibly by before the start
	 * of the tape IF we've got a T_MOV! */
	if (!has_set) {
	    if (!has_belowloop || opt_level>3)
		v->type = T_MULT;
	    else if (!has_negoff && node_type_counts[T_MOV] == 0)
		v->type = T_MULT;
	}
    }
    return (v->type != typewas);
}

int
flatten_multiplier(struct bfi * v)
{
    struct bfi *n;
    if (v->type != T_MULT && v->type != T_CMULT) return 0;

    n = v->next;
    while(n != v->last)
    {
	if (n->type == T_ADD) {
	    n->type = T_SET;
	    n->count2 = 1;
	    n->offset2 = n->offset;
	    n->count3 = n->count;
	    n->offset3 = v->offset;
	    n->count = 0;

	    if (n->offset2 == n->offset3 || n->count3 == 0) {
		n->count2 += n->count3;
		n->count3 = 0;
		n->offset3 = n->offset2;
	    }

	    if (n->count2 == 0 && n->count3 == 0)
		n->type = T_EQU;

	    if (n->count == 0 && n->count2 == 1 && n->count3 == 0 &&
		    n->offset2 == n->offset) {
		n->type = T_NOP;
		n->count = 0;
		n->count2 = 0;
	    }
	}
	n = n->next;
    }
    
    if (v->type == T_MULT) {
	n->type = T_NOP;
	v->type = T_NOP;
    } else
	v->type = T_IF;

    return 1;
}

#if 0
void
print_asm_string(char * charmap, char * strbuf, int strsize)
{
static int textno = 0;
    int i;

    if (strsize <= 0) return;
    if (strsize == 1) {
	int ch = *strbuf & 0xFF;
	if (charmap[ch] == 0) {
	    charmap[ch] = 1;
	    printf("section .data\n");
	    printf("litchar_%02x:\n", ch);
	    printf("\tdb 0x%02x\n", ch);

	    printf("section .textlib\n");
	    printf("litprt_%02x:\n", ch);
	    printf("\tpush ecx\n");
	    printf("\tmov ecx,litchar_%02x\n", ch);
	    printf("\tmov al, 4\n");
	    printf("\tmov ebx, edx\n");
	    printf("\tint 0x80\n");
	    printf("\tpop ecx\n");
	    printf("\tret\n");
	    printf("section .text\n");
	}
	printf("\tcall litprt_%02x\n", ch);
    } else {
	textno++;
	printf("section .data\n");
	printf("text_%d:\n", textno);
	for(i=0; i<strsize; i++) {
	    if (i%8 == 0) {
		if (i) printf("\n");
		printf("\tdb ");
	    } else
		printf(", ");
	    printf("0x%02x", (strbuf[i] & 0xFF));
	}
	printf("\n");
	printf("section .text\n");
	printf("\tpush ecx\n");
	printf("\tmov ecx,text_%d\n", textno);
	printf("\tmov ebx, edx\n");
	printf("\tmov edx,%d\n", strsize);
	printf("\tmov al, 4\n");
	printf("\tint 0x80\n");
	printf("\txor eax, eax\n");
	printf("\tcdq\n");
	printf("\tinc edx\n");
	printf("\tpop ecx\n");
    }
}
#endif

void
print_asm_string(char * charmap, char * strbuf, int strsize)
{
static int textno = -1;
    if (textno == -1) {
	textno++;

	printf("section .data\n");
	printf("putchbuf: db 0\n");
	printf("section .textlib\n");
	printf("putch:\n");
        printf("\tpush ecx\n");
        printf("\tmov ecx,putchbuf\n");
        printf("\tmov byte [ecx],al\n");
        printf("\tjmp syswrite\n");
	printf("prttext:\n");
        printf("\tpush ecx\n");
        printf("\tmov ecx,eax\n");
	printf("syswrite:\n");
        printf("\txor eax,eax\n");
        printf("\tmov ebx,eax\n");
        printf("\tinc ebx\n");
        printf("\tmov al, 4\n");
        printf("\tint 0x80\t; write(ebx, ecx, edx);\n");
        printf("\txor eax, eax\n");
        printf("\tcdq\n");
        printf("\tinc edx\n");
        printf("\tpop ecx\n");
        printf("\tret\n");
	printf("section .text\n");
    }

    if (strsize <= 0) return;
    if (strsize == 1) {
	int ch = *strbuf & 0xFF;
	if (charmap[ch] == 0) {
	    charmap[ch] = 1;
	    printf("section .textlib\n");
	    printf("litprt_%02x:\n", ch);
	    printf("\tmov al,0x%02x\n", ch);
	    printf("\tjmp putch\n");
	    printf("section .text\n");
	}
	printf("\tcall litprt_%02x\n", ch);
    } else {
	int i;
	textno++;
	printf("section .data\n");
	printf("text_%d:\n", textno);
	for(i=0; i<strsize; i++) {
	    if (i%8 == 0) {
		if (i) printf("\n");
		printf("\tdb ");
	    } else
		printf(", ");
	    printf("0x%02x", (strbuf[i] & 0xFF));
	}
	printf("\n");
	printf("section .text\n");
	printf("\tmov eax,text_%d\n", textno);
	printf("\tmov edx,%d\n", strsize);
	printf("\tcall prttext\n");
    }
}

void 
print_asm()
{
    struct bfi * n = bfprog;
    char string_buffer[300], *sp = string_buffer;
    char charmap[256];
    int m = 0xFF;
    memset(charmap, 0, sizeof(charmap));

    if (verbose)
	fprintf(stderr, "Generating NASM Code.\n");

/* System calls used ...
    int 0x80: %eax is the syscall number; %ebx, %ecx, %edx, %esi, %edi and %ebp
    eax
    1	exit(ebx);		Used.
    2	fork(
    3	read(ebx,ecx,edx);	Used.
    4	write(ebx,ecx,edx);	Used.
    5	open(
    6	close(
    7 ...

    Register allocation.
    eax = 0x000000xx, only AL is loaded on syscall.
    ebx = unknown, always reset.
    ecx = current pointer.
    edx = 1, dh used for loop cmp.
*/

    if (!noheader) {
	calculate_stats();

	printf("; %s, asmsyntax=nasm\n", curfile);
	printf("; nasm -f bin tiny.asm ; chmod +x tiny\n");
	printf("\n");
	printf("BITS 32\n");
	printf("\n");
	if (node_type_counts[T_MOV] == 0)
	    printf("memsize\tequ\t%d\n", max_pointer-min_pointer+1);
	else
	    printf("memsize\tequ\t0x8000\n");
	printf("orgaddr\tequ\t0x08048000\n");
	printf("\torg\torgaddr\n");
	printf("\n");
	printf("; A nice legal ELF header here, bit short, but that's okay.\n");
	printf("ehdr:\t\t\t\t\t\t; Elf32_Ehdr\n");
	printf("\tdb\t0x7F, \"ELF\", 1, 1, 1, 0\t\t;   e_ident\n");
	printf("\ttimes 8 db\t0\n");
	printf("\tdw\t2\t\t\t\t;   e_type\n");
	printf("\tdw\t3\t\t\t\t;   e_machine\n");
	printf("\tdd\t1\t\t\t\t;   e_version\n");
	printf("\tdd\t_start\t\t\t\t;   e_entry\n");
	printf("\tdd\tphdr - $$\t\t\t;   e_phoff\n");
	printf("\tdd\t0\t\t\t\t;   e_shoff\n");
	printf("\tdd\t0\t\t\t\t;   e_flags\n");
	printf("\tdw\tehdrsize\t\t\t;   e_ehsize\n");
	printf("\tdw\tphdrsize\t\t\t;   e_phentsize\n");
	printf("\tdw\t1\t\t\t\t;   e_phnum\n");
	printf("\tdw\t0\t\t\t\t;   e_shentsize\n");
	printf("\tdw\t0\t\t\t\t;   e_shnum\n");
	printf("\tdw\t0\t\t\t\t;   e_shstrndx\n");
	printf("\n");
	printf("ehdrsize      equ     $ - ehdr\n");
	printf("\n");
	printf("phdr:\t\t\t\t\t\t; Elf32_Phdr\n");
	printf("\tdd\t1\t\t\t\t;   p_type\n");
	printf("\tdd\t0\t\t\t\t;   p_offset\n");
	printf("\tdd\t$$\t\t\t\t;   p_vaddr\n");
	printf("\tdd\t$$\t\t\t\t;   p_paddr\n");
	printf("\tdd\tfilesize\t\t\t;   p_filesz\n");
	printf("\tdd\tfilesize+memsize\t\t;   p_memsz\n");
	printf("\tdd\t7\t\t\t\t;   p_flags\n");
	printf("\tdd\t0x1000\t\t\t\t;   p_align\n");
	printf("\n");
	printf("phdrsize      equ     $ - phdr\n");
	printf("\n");
	printf("_start:\n");
	printf("\txor\teax, eax\t; EAX = 0 ;don't change high bits.\n");
	printf("\tcdq\t\t\t; EDX = 0 ;sign bit of EAX\n");
	printf("\tinc\tedx\t\t; EDX = 1 ;ARG4 for system calls\n");
	printf("\tmov\tecx,mem\n");
	printf("\tsection\t.textlib\n");
	printf("\tsection\t.text\n");
	printf("\n");
    }

    while(n)
    {
	if (sp != string_buffer) {
	    if ((n->type != T_EQU && n->type != T_ADD && n->type != T_PRT) ||
		(n->type == T_PRT && n->count == -1) ||
		(sp >= string_buffer + sizeof(string_buffer) - 2)
	       ) {
		print_asm_string(charmap, string_buffer, sp-string_buffer);
		sp = string_buffer;
	    }
	}

	if (enable_trace) { /* Sort of! */
	    printf("; "); printtreecell(stdout, 0, n); printf("\n");
	}

	switch(n->type)
	{
	case T_MOV:
	    if (n->count == 0)
		;
	    else if (n->count >= -128)
		printf("\tadd ecx,%d\n", n->count);
	    else
		printf("\tsub ecx,%d\n", -n->count);

	    if(n->offset != 0 || n->count == 0)
		fprintf(stderr, "WARN: T_MOV found with offset=%d, count=%d!\n",
			n->offset, n->count);
	    break;
	case T_ADD:
	    if (n->count != 0)
		printf("\tadd byte [ecx+%d],%d\n", n->offset, m&(n->count));
	    break;
	    
	case T_EQU:
	    printf("\tmov byte [ecx+%d],%d\n", n->offset, m&(n->count));
	    break;
	    
	case T_SET:
	    if (0) {
		printf("; SET [ecx+%d] = %d + [ecx+%d]*%d + [ecx+%d]*%d\n",
			n->offset, n->count,
			n->offset2, n->count2,
			n->offset3, n->count3);
	    }
	    if (n->count2 == 1 && n->count3 == 0) {
		/* m[1] = m[2]*1 + m[3]*0 */
		printf("\tmov bl,byte [ecx+%d]\n", n->offset2);
		printf("\tmov byte [ecx+%d],bl\n", n->offset);
	    } else if (n->count2 == 1 && n->offset2 == n->offset) {
		/* m[1] = m[1]*1 + m[3]*n */
		if (n->count3 == -1) {
		    printf("\tmov al,byte [ecx+%d]\n", n->offset3);
		    printf("\tsub byte [ecx+%d],al\n", n->offset);
		} else if (n->count3 == 1) {
		    printf("\tmov al,byte [ecx+%d]\n", n->offset3);
		    printf("\tadd byte [ecx+%d],al\n", n->offset);
		} else {
		    printf("\tmov byte al,[ecx+%d]\n", n->offset3);
		    printf("\timul ebx,eax,%d\n", m&n->count3);
		    printf("\tadd byte [ecx+%d],bl\n", n->offset);
		}
	    } else {
		printf("\t; Full multiply\n");
		printf("\tmov bl,byte [ecx+%d]\n", n->offset2);
		printf("\timul bx,ax,%d\n", m&n->count2);
		printf("\tmov byte [ecx+%d],bl\n", n->offset);
		printf("\tmov byte al,[ecx+%d]\n", n->offset3);
		printf("\timul ebx,eax,%d\n", m&n->count3);
		printf("\tadd byte [ecx+%d],bl\n", n->offset);
	    }

	    if (m& (n->count))
		printf("\tadd byte [ecx+%d],%d\n", n->offset, m& (n->count));
	    break;
	    
	case T_PRT:
	    if (n->count > -1) {
		*sp++ = n->count;
		break;
	    }

	    if (n->count == -1) {
		print_asm_string(0,0,0);
		printf("\tmov al,[ecx+%d]\n", n->offset);
		printf("\tcall putch\n");
	    } else {
		*string_buffer = n->count;
		print_asm_string(charmap, string_buffer, 1);
	    }
	    break;

	case T_INP:
	    if (n->offset != 0) {
		printf("\tpush ecx\n");
		printf("\tadd ecx,%d\n", n->offset);
	    }
	    printf("\tmov al, 3\n");
	    printf("\txor ebx, ebx\n");
	    printf("\tint 0x80\t; read(ebx, ecx, edx);\n");
	    if (n->offset != 0) {
		printf("\tpop ecx\n");
	    }
	    break;

	case T_MULT: case T_CMULT:
	case T_ZFIND: case T_ADDWZ: case T_IF: case T_FOR:
	case T_WHL:
	    if (n->type != T_WHL)
		printf("; %s\n", tokennames[n->type&0xF]);
	    if (0 /* Optimise for Space */ )
		printf("start_%d:\n", n->count);
	    printf("\tcmp dh,byte [ecx+%d]\n", n->offset);
	    printf("\tjz near end_%d\n", n->count);
	    if (n->type != T_IF)
		printf("loop_%d:\n", n->count);
	    break;

	case T_END: 
	    if (n->parent->type == T_IF) {
		printf("end_%d:\n", n->parent->count);
	    } else if (0 /* Optimise for Space */ ) {
		printf("\tjmp near start_%d\n", n->parent->count);
		printf("end_%d:\n", n->parent->count);
	    } else {
		printf("\tcmp dh,byte [ecx+%d]\n", n->parent->offset);
		printf("\tjnz near loop_%d\n", n->parent->count);
		printf("end_%d:\n", n->parent->count);
	    }
	    break;

	case T_STOP: 
	    printf("\tcmp dh,byte [ecx+%d]\n", n->offset);
	    printf("\tjz near exit_prog\n");
	    break;

	case T_NOP: 
	    fprintf(stderr, "Warning on code generation: "
	           "NOP node: ptr+%d, cnt=%d, @(%d,%d).\n",
		    n->offset, n->count, n->line, n->col);
	    break;

	default:
	    printf("; Bad node: type %d: ptr+%d, cnt=%d.\n",
		    n->type, n->offset, n->count);
	    fprintf(stderr, "Error on code generation:\n"
	           "Bad node: type %d: ptr+%d, cnt=%d.\n",
		    n->type, n->offset, n->count);
	    break;
	}
	n=n->next;
    }

    if(sp != string_buffer) {
	print_asm_string(charmap, string_buffer, sp-string_buffer);
    }

    if (!noheader) {
	printf("\n");
	printf("exit_prog:\n");
	printf("\tmov\tbl, 0\t\t; Exit status\n");
	printf("\txor\teax, eax\n");
	printf("\tinc\teax\t\t; syscall(1, 0 {, ecx, edx} )\n");
	printf("\tint\t0x80\n");
	printf("\t;; EXIT ;;\n");
	printf("\n");
	printf("filesize equ\tsection..bss.start-orgaddr\n");
	printf("\tsection\t.bss\n");
	printf("mem:\n");
    }
}

#ifdef TCC_OUTPUT_MEMORY
void
run_ccode(void)
{
    char * ccode;
    size_t ccodelen;
    int imagesize;
    void * image;

    FILE * ofd;
    TCCState *s;
    int rv;
    int (*func)(void);

    ofd = open_memstream(&ccode, &ccodelen);
    printccode(ofd);
    if (ofd == NULL) { perror("open_memstream"); exit(7); }
    putc('\0', ofd);
    fclose(ofd);

    s = tcc_new();
    if (s == NULL) { perror("tcc_new()"); exit(7); }
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
    tcc_compile_string(s, ccode);

    imagesize = tcc_relocate(s, 0);
    if (imagesize <= 0) {
        fprintf(stderr, "tcc_relocate failed to return code size.\n");
        exit(1);
    }
    image = malloc(imagesize);
    rv = tcc_relocate(s, image);
    if (rv) {
        fprintf(stderr, "tcc_relocate failed error=%d\n", rv);
        exit(1);
    }

    func = tcc_get_symbol(s, "run_bf");
    if (!func) {
        fprintf(stderr, "Could not find compiled code entry point\n");
        exit(1);
    }
    tcc_delete(s);
    free(ccode);

    func();
    free(image);
}
#else
#warning Please add -include libtcc.h, -ltcc and -ldl on the compile command to enable -rc.
#endif

void 
print_adder(void)
{
    struct bfi * n = bfprog;
    int last_offset = 0;
    int last_offset_ok = 0;

    if (verbose)
	fprintf(stderr, "Generating Adding machine code.\n");

    while(n)
    {
	if (enable_trace) { /* Sort of! */
	    printf("# "); printtreecell(stdout, 0, n); printf("\n");
	}

	switch(n->type)
	{
	case T_MOV:
	    if (n->next == 0 || n->next->type != T_END) {
		fprintf(stderr, "Adding machine conversion failed; needs -O1\n");
		exit(1);
	    }
	    break;

	case T_ADD:
	    if (n->offset == last_offset && last_offset_ok)
		printf("add %d\n", n->count);
	    else
		printf("add %d,%d\n", n->count, n->offset);
	    last_offset = n->offset;
	    last_offset_ok = 1;
	    break;

	case T_EQU:
	    printf("loop %d\n", n->offset);
	    printf("add -1\n");
	    printf("next\n");
	    if (n->count != 0)
		printf("add %d\n", n->count);
	    last_offset = n->offset;
	    last_offset_ok = 1;
	    break;

	case T_PRT:
	    if (n->count > -1) {
		printf("outchar %d\n", n->count);
		break;
	    }
	    if (n->offset == last_offset && last_offset_ok)
		printf("out\n");
	    else
		printf("out %d\n", n->offset);
	    last_offset = n->offset;
	    last_offset_ok = 1;
	    break;

	case T_INP:
	    if (n->offset == last_offset && last_offset_ok)
		printf("in\n");
	    else
		printf("in %d\n", n->offset);
	    last_offset = n->offset;
	    last_offset_ok = 1;
	    break;

	case T_MULT: case T_CMULT:
	case T_ZFIND: case T_ADDWZ: case T_IF: case T_FOR:
	case T_WHL:
	    if (n->offset == last_offset && last_offset_ok)
		printf("loop\n");
	    else
		printf("loop %d\n", n->offset);
	    last_offset = n->offset;
	    last_offset_ok = 1;
	    break;

	case T_END: 
	    if (n->prev && n->prev->type == T_MOV) {
		printf("next %d\n", n->offset + n->prev->count);
		last_offset_ok = 0;
	    } else {
		printf("next\n");
		last_offset_ok = 1;
	    }
	    last_offset = n->parent->offset;
	    break;

	case T_NOP: 
	    fprintf(stderr, "Warning on code generation: "
	           "NOP node: ptr+%d, cnt=%d, @(%d,%d).\n",
		    n->offset, n->count, n->line, n->col);
	    break;

	default:
	    printf("# Bad node: type %d: ptr+%d, cnt=%d.\n",
		    n->type, n->offset, n->count);
	    fprintf(stderr, "Error on code generation:\n"
	           "Bad node: type %d: ptr+%d, cnt=%d.\n",
		    n->type, n->offset, n->count);
	    break;
	}
	n=n->next;
    }
}

void 
print_bf(void)
{
    struct bfi * n = bfprog;
    int last_offset = 0;
    int last_offset_ok = 0;
    int i;

    if (verbose)
	fprintf(stderr, "Generating brainfuck code.\n");

    while(n)
    {
	if (enable_trace) { /* Sort of! */
	    printf("# "); printtreecell(stdout, 0, n); printf("\n");
	}

	if (n->type != T_MOV && n->offset!=last_offset ) {
	    while(n->offset>last_offset) { putchar('>'); last_offset++; };
	    while(n->offset<last_offset) { putchar('<'); last_offset--; };
	}

	switch(n->type)
	{
	case T_MOV:
	    i = n->count;
	    while(i>0) { putchar('>'); i--; }
	    while(i<0) { putchar('<'); i++; }
	    break;

	case T_EQU:
	    printf("[-]");
	    i = n->count;
	    while(i>0) { putchar('+'); i--; }
	    while(i<0) { putchar('-'); i++; }
	    break;

	case T_ADD:
	    i = n->count;
	    while(i>0) { putchar('+'); i--; }
	    while(i<0) { putchar('-'); i++; }
	    break;

	case T_PRT:
	    if (n->count > -1) {
		/* BF extension, literal putchar */
		putchar('\'');
		putchar(n->count);
	    } else
		putchar('.');
	    break;

	case T_INP:
	    putchar(',');
	    break;

	case T_MULT: case T_CMULT:
	case T_ZFIND: case T_ADDWZ: case T_IF: case T_FOR:
	case T_WHL:
	    putchar('[');
	    break;

	case T_END: 
	    putchar(']');
	    break;

	case T_NOP: 
	    fprintf(stderr, "Warning on code generation: "
	           "NOP node: ptr+%d, cnt=%d, @(%d,%d).\n",
		    n->offset, n->count, n->line, n->col);
	    break;

	default:
	    printf("# Bad node: type %d: ptr+%d, cnt=%d.\n",
		    n->type, n->offset, n->count);
	    fprintf(stderr, "Error on code generation:\n"
	           "Bad node: type %d: ptr+%d, cnt=%d.\n",
		    n->type, n->offset, n->count);
	    break;
	}
	putchar('\n');
	n=n->next;
    }
}

/* -- */
#ifdef USEHUGERAM
void * 
map_hugeram(void)
{
    void * mp;
    cell_array_alloc_len = MEMSIZE + 2*MEMGUARD;

    /* Map the memory and two guard regions */
    mp = mmap(0, cell_array_alloc_len,
		PROT_READ+PROT_WRITE,
		MAP_PRIVATE+MAP_ANONYMOUS+MAP_NORESERVE, -1, 0);

    if (mp == MAP_FAILED) {
	fprintf(stderr, "Cannot map memory for cell array\n");
	exit(1);
    }
    cell_array_low_addr = mp;

    if (MEMGUARD > 0) {
	/* unmap the guard regions */
	munmap(mp, MEMGUARD); mp += MEMGUARD;
	munmap(mp+MEMSIZE, MEMGUARD);
    }

    cell_array_pointer = mp;
    if (opt_level > 3)
	cell_array_pointer += MEMSKIP;

    return cell_array_pointer;
}

void
sigsegv_report(int signo, siginfo_t * siginfo, void * ptr)
{
    if(cell_array_pointer != 0) {
	if (siginfo->si_addr > cell_array_pointer - MEMGUARD &&
	    siginfo->si_addr < cell_array_pointer + MEMSIZE + MEMGUARD) {

	    fprintf(stderr, "Tape pointer has moved %s available space\n",
		    siginfo->si_addr > cell_array_pointer? "above": "below");
	    exit(1);
	}
    }
    if(siginfo->si_addr < (void*)0 + 1024*1024) {
	fprintf(stderr, "Program fault: NULL Pointer dereference.\n");
    } else {
	fprintf(stderr, "Segmentation violation at unusual address.\n");
    }
    exit(4);
}

void
trap_sigsegv(void)
{
    struct sigaction saSegf;

    saSegf.sa_sigaction = sigsegv_report;
    sigemptyset(&saSegf.sa_mask);
    saSegf.sa_flags = SA_SIGINFO;

    if(0 > sigaction(SIGSEGV, &saSegf, NULL))
	perror("Error trapping SIGSEGV, ignoring");
}
#endif
