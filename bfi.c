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
    Second and later files are input to BF program.

    Move non-C generators into other files.
	Add GNU Jit variant ?
	Add i386 binary version ?
	Each type has a #define to enable it in this file.
	Plus an overall one.

    Note: With T_IF loops the condition may depend on the cell size.
	If so, within the loop the cell size may be known.

    IF Known not-zero at start of loop --> do { } while();

    New C function on Level 2 end while.

    T_PRT, add string print to tree.

    T_HINT(offset), save results of known value search.
	-- Problem, values become known as the code simplifies.

    Add T_ENDIF, will never loop. Move loop variable reset out of loop.

    Other macros "[>>]" rail runner extensions, array access.

    Code: "m[0]++; m[1] += !m[0];" 16 bit inc.

------------------------------------------------
    If first 2 characters are '#!' allow comments beginning with '#'
	-- Poss if first character is a '#'
------------------------------------------------
    #!	Comment if at start of FILE
    #	Comment if at SOL
    %	Comment character til EOL
    //  Comment til EOL.

*/ /*	C comments or REVERSED C comments ?

    ;	Input cell as a decimal NUMBER, trim initial whitespace.
    :	Output cell as a decimal NUMBER
    =	Set memory cell to zero. (macro for [-] )

    #	Debugging flag.
    @	On stdin for end of program / start of data.

    \	Input next character as a literal. ie *m = 'next';
    "..."   Input characters from string on succesive calls.

    'Easy' mode, read from stdin single BF commands.
	"+-<>,." are executed immediatly.
	"]" is ignored.
	"[" reads source until matching "]" and optimises and executes loop.
	    Or discards it if it's a comment.

    Hard 'Easy' mode. Do initial loop read character by character.
*/

#ifdef __STRICT_ANSI__
#define _POSIX_C_SOURCE 200809UL
#if __STDC_VERSION__ < 199901L
#error This program needs at least the C99 standard.
#endif
#else
#define _GNU_SOURCE
#endif

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
#include <wchar.h>

#ifdef MAP_NORESERVE
#define USEHUGERAM
#endif

#define USEPRINTF

#ifdef USEHUGERAM
#define MEMSIZE	    2UL*1024*1024*1024
#define MEMGUARD    16UL*1024*1024
#define MEMSKIP	    1UL*1024*1024
#else
#define MEMSIZE	    256*1024
#warning Using small memory
#endif

#if !defined(__STRICT_ANSI__) && !defined(TCCLIBMISSING)
#define USETCCLIB
#endif

#ifdef USETCCLIB
#include <libtcc.h>
#endif

char * program = "C";
int verbose = 0;
int noheader = 0;
int do_run = 0;
enum codestyle { c_default, c_c, c_asm, c_bf, c_adder };
int do_codestyle = c_default;

int opt_level = 3;
int disable_rle = 0;
int hard_left_limit = 0;
int output_limit = 0;
int enable_trace = 0;
int iostyle = 0; /* 0=ASCII, 1=UTF8 */
int eofcell = 0; /* 0=> Undecided, 1=> No Change, 2= -1, 3= 0, 4=EOF, 5=No Input. */

int cell_size = 0;  /* 0=> 8,16,32 or more. 7 and up are number of bits */
int cell_mask = -1;
char *cell_type = "C";
#define SM(vx) (( ((int)(vx)) <<(32-cell_size))>>(32-cell_size))
#define UM(vx) ((vx) & cell_mask)

char * curfile = 0;
int curr_line = 0, curr_col = 0;
int cmd_line = 0, cmd_col = 0;

#define TOKEN_LIST(Mac) \
    Mac(MOV) Mac(ADD) Mac(PRT) Mac(INP) Mac(WHL) Mac(END) \
    Mac(EQU) Mac(SET) \
    Mac(ZFIND) Mac(MFIND) Mac(ADDWZ) \
    Mac(IF) Mac(MULT) Mac(CMULT) Mac(FOR) \
    Mac(RAILC) Mac(SET2) Mac(SET3) \
    Mac(STOP) Mac(NOP) Mac(DEAD)

#define GEN_TOK_ENUM(NAME) T_ ## NAME,
enum token { TOKEN_LIST(GEN_TOK_ENUM) TCOUNT};

#define GEN_TOK_STRING(NAME) "T_" #NAME,
const char* tokennames[] = { TOKEN_LIST(GEN_TOK_STRING) };

struct bfi
{
    struct bfi *next;
    int type;
    int count;
    int offset;
    struct bfi *jmp;

    int count2;
    int offset2;
    int count3;
    int offset3;

    int profile;
    int line, col;
    int inum;

    int orgtype;
    struct bfi *prev;
} *bfprog = 0, *bfbase = 0;

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
void print_ccode(FILE * ofd);
void calculate_stats(void);
void pointer_scan(void);
void quick_scan(void);
void invariants_scan(void);
void trim_trailing_sets(void);
int find_known_state(struct bfi * v);
int find_known_set_state(struct bfi * v);
int flatten_loop(struct bfi * v, int constant_count);
int classify_loop(struct bfi * v);
int flatten_multiplier(struct bfi * v);
void * tcalloc(size_t nmemb, size_t size);
void print_adder(void);
void print_bf(void);
void * map_hugeram(void);
void trap_sigsegv(void);
int get_char(void);
void set_cell_size(int cell_bits);
void convert_tree_to_runarray(void);

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
    fprintf(stderr, "   -u   Unicode I/O%s\n", iostyle?" (default)":"");
    fprintf(stderr, "   -a   Ascii I/O%s\n", iostyle?"":" (default)");
    fprintf(stderr, "   -On  'Optimisation level'\n");
    fprintf(stderr, "   -O0      Turn off all optimisation, just leave RLE.\n");
    fprintf(stderr, "   -O1      Only pointer motion removal.\n");
    fprintf(stderr, "   -O2      A few simple changes.\n");
    fprintf(stderr, "   -O3      Maximum normal level, default.\n");
    fprintf(stderr, "   -m   Minimal processing, -O0 and disable RLE.\n");
    fprintf(stderr, "   -B8  Use 8 bit cells.\n");
    fprintf(stderr, "   -B16 Use 16 bit cells.\n");
    fprintf(stderr, "   -B32 Use 32 bit cells.\n");
    fprintf(stderr, "        Default for C is 'unknown', ASM can only be 8bit.\n");
    fprintf(stderr, "        Other bitwidths work (including 7) for the interpreter and C.\n");
    fprintf(stderr, "        Unicode characters need at least 16 bits.\n");
    fprintf(stderr, "   -E0  One of the other options; choose later.\n");
    fprintf(stderr, "   -E1  End of file gives no change for ',' command.\n");
    fprintf(stderr, "   -E2  End of file gives -1.\n");
    fprintf(stderr, "   -E3  End of file gives 0.\n");
    fprintf(stderr, "   -E4  End of file gives EOF.\n");
    fprintf(stderr, "   -E5  Disable ',' command.\n");
    exit(1);
}

void
UsageInt64()
{
    fprintf(stderr,
	"You cannot *specify* a cell size > %zd bits on this machine.\n"
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
    if (isatty(STDOUT_FILENO)) setbuf(stdout, 0);
    setlocale(LC_ALL, "");

    if (!strcmp("UTF-8", nl_langinfo(CODESET))) iostyle = 1;

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
                    case 'c': do_codestyle = c_c; break;
                    case 's': do_codestyle = c_asm; break;
                    case 'A': do_codestyle = c_adder; break;
                    case 'F': do_codestyle = c_bf; break;
                    case 'm': opt_level=0; disable_rle = 1; break;
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
                            case 'E': eofcell=strtol(ap,0,10); break;
                            case 'B': set_cell_size(strtol(ap,0,10)); break;
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

    if (do_codestyle == c_asm)
    {
	if (do_run || (cell_size && cell_size != 8)) {
	    fprintf(stderr, "The -s flag cannot be combined with -r or -B\n");
	    exit(255);
	}
	set_cell_size(8);
    }

    if (!do_run && do_codestyle == c_default)
	do_run = 1;

    if (cell_size == 0 && do_run) set_cell_size(32);

#ifdef USEHUGERAM
    trap_sigsegv();
#endif

    if( !filename || !*filename )
      open_file(0);
    else
      open_file(filename);

    process_file();

    exit(0);
}

void
open_file(char * fname)
{
    FILE * ifd;
    int ch, dld = 0, lid = 0;
    struct bfi *p=0, *n=bfprog, *j=0;
    if (n) p = n->prev;

    curr_line = 1; curr_col = 0;
    total_nodes = loaded_nodes = 0;

    if (!fname || strcmp(fname, "-") == 0) ifd = stdin;
    else if ((ifd = fopen(fname, "r")) == 0) {
	perror(fname);
	exit(1);
    }
    while((ch = getc(ifd)) != EOF && (ifd!=stdin || ch != '!')) {
	if (ch == '\n') { curr_line++; curr_col=0; }
	else curr_col ++;

	if (ch == '<' || ch == '>' || ch == '+' || ch == '-' ||
	    ch == '[' || ch == ']' || ch == ',' || ch == '.') {
#ifndef NO_BFCOMMENT
	    /* Comment loops, can never be run */
	    if (dld || (ch == '[' && !noheader && (!p || p->type == ']'))) {
		if (ch == '[') dld ++;
		if (ch == ']') dld --;
		continue;
	    }
#endif
#ifndef NO_RLE
	    if (!disable_rle) {
		/* RLE compacting of instructions. */
		if (p && ch == p->type && p->count ){
		    p->count++;
		    continue;
		}
#endif
	    }
	    n = tcalloc(1, sizeof*n);
	    loaded_nodes++;
	    if (p) { p->next = n; n->prev = p; } else bfprog = n;
	    n->type = ch; p = n;
	    n->line = curr_line; n->col = curr_col;
	    if (n->type == '[') { n->jmp=j; j = n; }
	    else if (n->type == ']') {
		if (j) { n->jmp = j; j = j->jmp; n->jmp->jmp = n;
		} else n->type = ')';
	    } else if (ch != '.' && ch != ',') 
		n->count = 1;
	}
    }
    if (ifd!=stdin) fclose(ifd);

    while (j) { n = j; j = j->jmp; n->type = '('; n->jmp = 0; }

    bfbase=bfprog;
    total_nodes = loaded_nodes;

    for(n=bfprog; n; n=n->next) {
	switch(n->type)
	{
	    case '>': n->type = T_MOV; break;
	    case '<': n->type = T_MOV; n->count = -n->count; break;
	    case '+': n->type = T_ADD; break;
	    case '-': n->type = T_ADD; n->count = -n->count; break;
	    case '[': n->type = T_WHL; n->count = ++lid; break;
	    case ']': n->type = T_END; break;
	    case '.': n->type = T_PRT; n->count = -1; break;
	    case ',':
		    if (eofcell == 5) 
			n->type = T_NOP;
		    else
			n->type = T_INP;
		    break;

	    case '(': case ')': /* Unbalanced bkts */
		fprintf(stderr,
			"Warning: unbalanced bracket at Line %d, Col %d\n",
			n->line, n->col);
	    default: n->type = T_NOP; break;
	}
	n->orgtype = n->type;
#ifndef NO_RLE
	/* Merge '<' with '>' and '+' with '-' */
	if (!disable_rle && n->prev && n->prev->type == n->type &&
		(n->type == T_MOV || n->type == T_ADD)) {
	    n->prev->next = n->next;
	    if (n->next) n->next->prev = n->prev;
	    n->prev->count += n->count;
	    p = n->prev;
	    free(n);
	    total_nodes--;
	    n = p;
	}
#endif
    }
}

void *
tcalloc(size_t nmemb, size_t size)
{
    void * m;
    m = calloc(nmemb, size);
    if (m) return m;

    fprintf(stderr, "Allocate of %zd*%zd bytes failed, ABORT\n", nmemb, size);
    exit(42);
}

void 
printtreecell(FILE * efd, int indent, struct bfi * n)
{
#define CN(pv) ((int)(((int)(pv) - (int)bfbase) / sizeof(struct bfi)))
    while(indent-->0) 
	fprintf(efd, " ");
    if (n == 0 ) {fprintf(efd, "NULL Cell"); return; }
    fprintf(efd, "%s", tokennames[n->type]);
    switch(n->type)
    {
    case T_MOV:
	if(n->offset == 0)
	    fprintf(efd, " %d ", n->count);
	else
	    fprintf(efd, " %d,(off=%d), ", n->count, n->offset);
	break;
    case T_ADD: case T_EQU:
	fprintf(efd, "[%d]:%d, ", n->offset, n->count);
	break;
    case T_SET:
	fprintf(efd, "[%d]=%d+[%d]*%d+[%d]*%d, ",
	    n->offset, n->count, n->offset2, n->count2, n->offset3, n->count3);
	break;
    case T_PRT:
	if (n->count == -1 )
	    fprintf(efd, "[%d], ", n->offset);
	else {
	    if (n->count >= ' ' && n->count <= '~' && n->count != '"')
		fprintf(efd, " '%c', ", n->count);
	    else
		fprintf(efd, " 0x%02x, ", n->count);
	}
	break;
    case T_INP: fprintf(efd, "[%d], ", n->offset); break;
    case T_DEAD: fprintf(efd, "if(0) id=%d, ", n->count); break;
    case T_END: fprintf(efd, "[%d] id=%d, ", n->offset, n->jmp->count); break;
    case T_STOP: fprintf(efd, "[%d], ", n->offset); break;

    case T_ZFIND: case T_MFIND: case T_ADDWZ:
    case T_FOR: case T_IF: case T_MULT: case T_CMULT:
    case T_WHL:
	fprintf(efd, "[%d],id=%d, ", n->offset, n->count);
	break;

    default:
	fprintf(efd, "[%d]:%d, ", n->offset, n->count);
	if (n->offset2 != 0 || n->count2 != 0)
	    fprintf(efd, "x2[%d]:%d, ", n->offset2, n->count2);
	if (n->offset3 != 0 || n->count3 != 0)
	    fprintf(efd, "x3[%d]:%d, ", n->offset3, n->count3);
    }
    fprintf(efd, "cell 0x%04x: ", CN(n));
    if(n->next == 0) 
	fprintf(efd, "next 0, ");
    else if(n->next->prev != n) 
	fprintf(efd, "next 0x%04x, ", CN(n->next));
    if(n->prev == 0)
	fprintf(efd, "prev 0, ");
    else if(n->prev->next != n) 
	fprintf(efd, "prev 0x%04x, ", CN(n->prev));
    if(n->jmp) 
	fprintf(efd, "jmp 0x%04x, ", CN(n->jmp));
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
    fprintf(stderr, "End of whole tree dump.\n");
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
    int use_full = 0;
    int memsize = 60000;
    int memoffset = 0;

    calculate_stats();

    if (enable_trace || do_run) use_full = 1;

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
	fprintf(ofd, "# define C int\n");
	fprintf(ofd, "# endif\n");
    }

    if (node_type_counts[T_INP] != 0)
    {
	if (eofcell == 0) {
	    fprintf(ofd, "#ifndef EOFCELL\n");
	    fprintf(ofd, "# define save(vn,gf) {int ch=gf; if(ch!=EOF) vn=ch;}\n");
	    fprintf(ofd, "#else\n");
	    fprintf(ofd, "# if EOFCELL == EOF\n");
	    fprintf(ofd, "#  define save(vn,gf) vn=gf\n");
	    fprintf(ofd, "# else\n");
	    fprintf(ofd, "#  define save(vn,gf) {int ch=gf; "
			    "if(ch!=EOF) vn=ch; else vn=EOFCELL;}\n");
	    fprintf(ofd, "# endif\n");
	    fprintf(ofd, "#endif\n");
	    fprintf(ofd, "#define getch(vn) save(*(vn), get_char())\n");
	} else if (eofcell == 1) {
	    fprintf(ofd, "#define getch(vn) {int ch=get_char(); "
			    "if(ch!=EOF) *(vn)=ch;}\n");
	} else if (eofcell == 2) {
	    fprintf(ofd, "#define getch(vn) {int ch=get_char(); "
			    "if(ch!=EOF) *(vn)=ch; else *(vn)=-1;}\n");
	} else if (eofcell == 3) {
	    fprintf(ofd, "#define getch(vn) {int ch=get_char(); "
			    "if(ch!=EOF) *(vn)=ch; else *(vn)=0;}\n");
	} else if (eofcell == 4) {
	    fprintf(ofd, "#define getch(vn) *(vn)=get_char()\n");
	} else
	    fprintf(ofd, "#define getch(vn)\n");

	if (!do_run || iostyle != 1)
	    fprintf(ofd, "#define get_char() get%schar()\n", (iostyle==1?"w":""));
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
    if (!hard_left_limit) {
	if (memoffset == 0 && min_pointer < 0)
	    memoffset -= min_pointer;
	memoffset -= most_negative_mov;
	if (memoffset < 1024 && memoffset != 0)
	    memoffset = 1024;
	memsize += memoffset;
	if (memsize < 65000)
	    memsize = 65000;
    }

    if (node_type_counts[T_MOV] == 0 && memoffset == 0) {
	fprintf(ofd, "%s m[%d];\n", cell_type, memsize);
	fprintf(ofd, "int main(){\n");
    } else {
	if (!do_run)
	    fprintf(ofd, "%s mem[%d];\n", cell_type, memsize);
	else
	    fprintf(ofd, "extern %s mem[];\n", cell_type);
	fprintf(ofd, "int main(){\n");
#if 0
	fprintf(ofd, "#if defined(__GNUC__) && defined(__i386__)\n");
	fprintf(ofd, "  register %s * m asm (\"%%eax\");\n", cell_type);
	fprintf(ofd, "#else\n");
	fprintf(ofd, "  %s*m;\n", cell_type);
	fprintf(ofd, "#endif\n");
#else
	fprintf(ofd, "  %s*m;\n", cell_type);
#endif

	if (memoffset > 0 && !do_run)
	    fprintf(ofd, "  m = mem + %d;\n", memoffset);
	else
	    fprintf(ofd, "  m = mem;\n");
    }

    if (node_type_counts[T_INP] != 0)
	fprintf(ofd, "  setbuf(stdout, 0);\n");

    if (iostyle == 1)
	fprintf(ofd, "  setlocale(LC_ALL, \"\");\n");
}

void 
print_ccode(FILE * ofd)
{
    int indent = 0;
    struct bfi * n = bfprog;
    char string_buffer[180], *sp = string_buffer, lastspch = 0;
    int ok_for_printf = 0, spc = 0;
    int add_mask = 0;
    if (cell_size > 0 &&
	cell_size != sizeof(int)*8 &&
	cell_size != sizeof(short)*8 &&
	cell_size != sizeof(char)*8)
	add_mask = cell_mask;

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
	    case T_WHL: case T_ZFIND: case T_MFIND: case T_IF:
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
	    else
		fprintf(ofd, "/* m += 0; */\n");
	    break;
	case T_ADD:
	    pt(ofd, indent,n);
	    if(n->offset == 0) {
		if (n->count < 0)
		    fprintf(ofd, "*m -= %d;\n", -n->count);
		else if (n->count > 0)
		    fprintf(ofd, "*m += %d;\n", n->count);
		else
		    fprintf(ofd, "/* m[%d] += 0; */\n", n->offset);
	    } else {
		if (n->count < 0)
		    fprintf(ofd, "m[%d] -= %d;\n", n->offset, -n->count);
		else if (n->count > 0)
		    fprintf(ofd, "m[%d] += %d;\n", n->offset, n->count);
		else
		    fprintf(ofd, "/* m[%d] += 0; */\n", n->offset);
	    }
	    if (enable_trace) {
		pt(ofd, indent,0);
		fprintf(ofd, "t(%d,%d,\"\",m+ %d)\n", n->line, n->col, n->offset);
	    }
	    break;
	case T_EQU:
	    pt(ofd, indent,n);
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
#if 0
	    pt(ofd, indent, n);
	    fprintf(ofd, "// T_SET[%d]=%d+[%d]*%d+[%d]*%d\n",
		n->offset, n->count, n->offset2, n->count2, n->offset3, n->count3);
#endif
	    pt(ofd, indent, n);
	    {
		int ac = '=';
		fprintf(ofd, "m[%d]", n->offset);
		if (n->count) {
		    fprintf(ofd, " %c %d ", ac, n->count);
		    ac = '+';
		}

		if (n->count2) {
		    int c = n->count2;
		    if (ac == '+' && c < 0) { c = -c; ac = '-'; };
		    if (ac == '=' && n->offset == n->offset2 && c == 1) {
			fprintf(ofd, " += ");
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
	    fprintf(ofd, "getch(m+%d);\n", n->offset);
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
	    if (n->next->next != n->jmp) {
		fprintf(ofd, "while(m[%d]){\n", n->offset);
	    } else {
		fprintf(ofd, "while(m[%d]){m+=%d;}\n",
		    n->offset, n->next->count);
		n=n->jmp;
	    }
	    break;

	case T_WHL:
	case T_CMULT:
	case T_MULT:
	case T_MFIND:
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

	case T_END: 
	    pt(ofd, indent,n);
	    fprintf(ofd, "}\n");
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
    struct timeval tv_start, tv_end, tv_step;

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

	if (verbose>2) gettimeofday(&tv_step, 0);
	quick_scan(); /**/
	if (verbose>2) {
	    gettimeofday(&tv_end, 0);
	    fprintf(stderr, "Time for quick scan %.3f\n", 
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

	    if (!noheader) {
		if (verbose>2) gettimeofday(&tv_step, 0);
		trim_trailing_sets(); /**/
		if (verbose>2) {
		    gettimeofday(&tv_end, 0);
		    fprintf(stderr, "Time for trailing scan %.3f\n", 
			(tv_end.tv_sec - tv_step.tv_sec) +
			(tv_end.tv_usec - tv_step.tv_usec)/1000000.0);
		}
	    }
	}
	if (verbose>5) printtree();
    }
    if (verbose && !do_run)
	print_tree_stats();

    if (do_run) {
	if (do_codestyle == c_c) {
#ifdef USETCCLIB
	    run_ccode();
#else
	    fprintf(stderr, "Sorry cannot compile C code in memory, TCCLIB not linked\n");
	    exit(2);
#endif
	} else if (verbose>2) {
	    fprintf(stderr, "Starting profiling interpreter\n");
	    run_tree();
	} else {
	    if (verbose)
		fprintf(stderr, "Starting array interpreter\n");
	    convert_tree_to_runarray();
	}
    } else switch(do_codestyle) {
	case c_c:
	    print_ccode(stdout);
	    break;
	case c_asm:
	    print_asm();
	    break;
	case c_bf:
	    print_bf();
	    break;
	case c_adder:
	    print_adder();
	    break;
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
	n->inum = total_nodes;
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
    fprintf(stderr, " (%dk)", (int)(loaded_nodes * sizeof(struct bfi) +1023) / 1024);
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

#ifndef NOPCOUNT
/*

#define NOPCOUNT 0
*/
#endif

#if TMASK == 0xFF
#define icell	unsigned char
#define M(x) x
#elif TMASK == 0xFFFF
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
#define M(x) UM(x)
#endif

#ifdef USEHUGERAM
    oldp = p = map_hugeram();
#else
    p = oldp = tcalloc(MEMSIZE, sizeof*p);
    use_free = 1;
#endif

    gettimeofday(&tv_start, 0);

#ifdef NOPCOUNT
#include "nops.h"
#endif

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
	    case T_MFIND:

	    case T_WHL: if(M(p[n->offset]) == 0) n=n->jmp;
		break;

	    case T_END: if(M(p[n->offset]) != 0) n=n->jmp;
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

		c = get_char();
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
		if(M(p[n->offset]) == 0) { n=n->jmp; break; }
#if 0
		/* Too much overhead with strlen, possibly do some unrolling?
		 * But how much ?
		 */
		if (n->next->count == 1 && sizeof*p == 1)
		    /* Cast is to stop the whinge, if p isn't a char* this
		     * is dead code. */
		    p += strlen((char*)p + n->offset);
		else
#endif
		    while(M(p[n->offset]))
			p += n->next->count;
		n=n->jmp;
		break;

	    case T_ADDWZ:
		if(M(p[n->offset]) == 0) { n=n->jmp; break; }
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
		n=n->jmp;
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
#undef M
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
		if (n->next && n->next->jmp) {
		    printtreecell(stderr, 4, n->next->jmp->prev);
		    printtreecell(stderr, 4, n->next->jmp);
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
		n2 = n->next->jmp->prev;
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
		n2 = n->next->jmp;
		n3 = n2->next;
		n = n->next;
		n2->offset += n4->count;

		n2->next = n4;
		if(n3) n3->prev = n4;
		n4->next = n3;
		n4->prev = n2;

	    } else if (n->next->type == T_END) {
		/* Stuck behind an end loop, can't push past this */
		/* Make the line & col of the movement the same as the
		 * T_END that's blocked it. */
		n->line = n->next->line;
		n->col = n->next->col;
	    }
	}
	/* Push it off the end of the program */
	if (n->type == T_MOV && n->next == 0 && n->prev) {
	    if(verbose>4)
		fprintf(stderr, "Pushed to end of program.\n");

	    n->line = n->prev->line + 1;
	    n->col = 0;

	    if (!noheader) {
		n->type = T_NOP;
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
	    /* Sequence  [>>>] Search for zeros. */
	    if ( n->next->type == T_MOV &&
		 n->next->count != 0 &&
		 n->next->next->type == T_END)
		n->type = T_ZFIND;
	    /* Sequence  [-<<] Rails cleaners and similar. */
	    if ( n->next->type == T_ADD &&
		 n->next->next->type == T_MOV &&
		 n->next->next->next->type == T_END)
		n->type = T_ADDWZ;
	    /* Sequence  [->>+] Search for minus one. */
	    if ( n->next->type == T_ADD &&
	         n->next->next->type == T_ADD &&
		 n->next->next->next->type == T_MOV &&
		 n->next->next->next->next->type == T_END ) {

		if (
		    n->offset == n->next->offset 
		     && n->next->next->offset - n->offset == n->next->next->next->count
		     && n->next->count == -1
		     && n->next->next->count == 1
		     )
		    n->type = T_MFIND;
	    }

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

	case T_SET:
	    node_changed = find_known_set_state(n);
	    if (node_changed && n->prev)
		n = n->prev;
	    break;

	case T_END:
	    n2 = n->jmp;
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

	if (n && n->type == T_DEAD && n->jmp) {
	    n2 = n->jmp;
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
    int min_pointer = 0, max_pointer = 0, mov_count = 0;
    int node_changed, i;
    while(n)
    {
	if (n->type == T_MOV)
	    mov_count ++;
	if (min_pointer > n->offset) min_pointer = n->offset;
	if (max_pointer < n->offset) max_pointer = n->offset;
	lastn = n;
	n=n->next;
    }
    if (mov_count || max_pointer - min_pointer > 32) return;

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

void
find_known_value(struct bfi * n, int v_offset, int allow_recursion,
		struct bfi ** n_found, 
		int * const_found_p, int * known_value_p, int * unsafe_p)
{
    /*
     * n		Node to start the search
     * v_offset		Which offset are we interested in
     * allow_recursion  If non-zero we can split on a loop.
     * n_found		Node we found IF it's safe to change/delete
     * const_found_p	True if we found a known value
     * known_value_p	The known value.
     * unsafe_p		True if this value is unsuitable for loop deletion.
     */
    int unmatched_ends = 0;
    int const_found = 0, known_value = 0, non_zero_unsafe = 0;
    int n_used = 0;
    int distance = 0;

    if (verbose>5) {
	fprintf(stderr, "Checking value for offset %d starting: ", v_offset);
	printtreecell(stderr, 0,n);
	fprintf(stderr, "\n");
    }
    while(n)
    {
	switch(n->type)
	{
	case T_NOP:
	    break;

	case T_EQU:
	    if (n->offset == v_offset) {
		/* We don't know if this node will be hit */
		if (unmatched_ends) goto break_break;

		known_value = n->count;
		const_found = 1;
		goto break_break;
	    }
	    break;

	case T_SET:
	    /* Nope, if this were a constant it'd be downgraded already */
	    if (n->offset == v_offset)
		goto break_break;
	    if ((n->count2 != 0 && n->offset2 == v_offset) ||
	        (n->count3 != 0 && n->offset3 == v_offset))
		n_used = 1;
	    break;

	case T_PRT:
	    if (n->count == -1 && n->offset == v_offset) {
		n_used = 1;
		goto break_break;
	    }
	    break;

	case T_INP:
	    if (n->offset == v_offset) {
		n_used = 1;
		goto break_break;
	    }
	    break;

	case T_ADD:	/* Possible instruction merge for T_ADD */
	    if (n->offset == v_offset)
		goto break_break;
	    break;

	case T_END:
	    if (n->offset == v_offset) {
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
	    if (n->offset == v_offset)
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
	    if (n->offset == v_offset)
		n_used = 1;
	    if (unmatched_ends>0)
		unmatched_ends--;

#if 1
	    // Check both n->jmp and n->prev
	    // If they are both known we still have a known value.
	    if (allow_recursion) {
		// fprintf(stderr, "%d: searching for double known\n", __LINE__);
		find_known_value(n->prev, v_offset, allow_recursion-1,
		    0, &const_found, &known_value, &non_zero_unsafe);
		if (const_found) {
		    int known_value2, non_zero_unsafe2;
		// fprintf(stderr, "%d: direct was known. %d\n", __LINE__, known_value);

		    find_known_value(n->jmp->prev, v_offset, allow_recursion-1,
			0, &const_found, &known_value2, &non_zero_unsafe2);
		// fprintf(stderr, "%d: jmp was %sknown. %d\n", __LINE__, const_found?"":"un", known_value2);
		    if (known_value != known_value2)
			const_found = 0;
		}
		n = 0;
	    }
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
	known_value = SM(known_value);
    
    *const_found_p = const_found;
    *known_value_p = known_value;
    *unsafe_p = non_zero_unsafe;
    if (n_found) {
	/* If "n" is safe to modify return it too. */
	if (n==0 || n_used || unmatched_ends || distance > 100)
	    *n_found = 0;
	else
	    *n_found = n;
    }

    if (verbose>5) {
	if (const_found)
	    fprintf(stderr, "Known value is %d%s",
		    known_value, non_zero_unsafe?" (unsafe zero)":"");
	else
	    fprintf(stderr, "Unknown value for offset %d", v_offset);
	fprintf(stderr, "\n  From ");
	printtreecell(stderr, 0,n);
	fprintf(stderr, "\n");
    }
}

int
find_known_state(struct bfi * v)
{
    struct bfi *n = 0;
    int const_found = 0, known_value = 0, non_zero_unsafe = 0;

    if(v == 0) return 0;

    if (verbose>5) {
	fprintf(stderr, "Find state for: ");
	printtreecell(stderr, 0,v);
	fprintf(stderr, "\n");
    }

    find_known_value(v->prev, v->offset, 3, &n, 
		&const_found, &known_value, &non_zero_unsafe);

    if (!const_found) {
	switch(v->type) {
	    case T_ADD:
		if (n && n->type == T_ADD && n->offset == v->offset) {
		    n->count += v->count;
		    v->type = T_NOP;
		    if (verbose>5) fprintf(stderr, "  Delete this node.\n");
		    return 1;
		}
		break;
	    case T_EQU:
		if (n && 
			( (n->type == T_ADD && n->offset == v->offset) ||
			  (n->type == T_SET && n->offset == v->offset)
			)) {

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
		v->count = SM(v->count);
	    if (verbose>5) fprintf(stderr, "  Upgrade to T_EQU.\n");
	    /*FALLTHROUGH*/

	case T_EQU: /* Delete if same or unused */
	    if (known_value == v->count) {
		v->type = T_NOP;
		if (verbose>5) fprintf(stderr, "  Delete this T_EQU.\n");
		return 1;
	    } else if (n && (n->type == T_EQU || n->type == T_SET)) {
		struct bfi *n2;

		if (verbose>5) {
		    fprintf(stderr, "  Delete: ");
		    printtreecell(stderr, 0, n);
		    fprintf(stderr, "\n");
		}
		n->type = T_NOP;
		n2 = n; n = n->prev;
		if (n) n->next = n2->next; else bfprog = n2->next;
		if (n2->next) n2->next->prev = n;
		free(n2);
		return 1;
	    }
	    break;

	case T_PRT: /* Print literal character. */
	    if (v->count != -1) break;
	    if (do_codestyle == c_bf) break; /* BF output can't do lits. */
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
		    v->jmp->type = T_NOP;
		    if (verbose>5) fprintf(stderr, "  Change to If loop.\n");
		    return 1;
		}
	    }
	    if (v->type != T_IF)
		return flatten_loop(v, known_value);
	    break;

	case T_END: /* Change to IF command or infinite loop (error exit) */
	    if (!non_zero_unsafe) {
		if (known_value == 0 && v->jmp->type != T_IF) {
		    v->jmp->type = T_IF;
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
search_from_a_to_b_for_update_of_offset(struct bfi *n, struct bfi *v, int n_offset)
{
    while(n!=v && n)
    {
// fprintf(stderr, "Checking: "); printtreecell(stderr, 0, n); fprintf(stderr, "\n");
	switch(n->type)
	{
	case T_IF: case T_FOR: case T_MULT: case T_CMULT: case T_WHL:

	case T_ADD: case T_EQU: case T_SET:
	    if (n->offset == n_offset)
		return 0;
	    break;
	case T_PRT: case T_NOP:
	    break;
	default: return 0;  /* Any other type is a problem */
	}
	n=n->next;
    }
    return n != 0;
}

int
find_known_set_state(struct bfi * v)
{
    int rv = 0;
    struct bfi *n1 = 0;
    int const_found1 = 0, known_value1 = 0, non_zero_unsafe1 = 0;
    struct bfi *n2 = 0;
    int const_found2 = 0, known_value2 = 0, non_zero_unsafe2 = 0;
    int n2_valid = 0;
    struct bfi *n3 = 0;
    int const_found3 = 0, known_value3 = 0, non_zero_unsafe3 = 0;
    int n3_valid = 0;

    if(v == 0) return 0;
    if (v->type != T_SET) return 0;

    if ((v->count2 == 0 || v->offset2 != v->offset) &&
        (v->count3 == 0 || v->offset3 != v->offset)) {
	find_known_value(v->prev, v->offset, 3, &n1, 
		    &const_found1, &known_value1, &non_zero_unsafe1);
	if (n1 && (n1->type == T_ADD || n1->type == T_EQU)) {
	    /* Overidden change, delete it */
	    struct bfi *n4;

	    n1->type = T_NOP;
	    n4 = n1; n1 = n1->prev;
	    if (n1) n1->next = n4->next; else bfprog = n4->next;
	    if (n4->next) n4->next->prev = n1;
	    free(n4);
	    return 1;
	}
    }

    if (v->count3 == 1 && v->offset != v->offset2 && v->offset == v->offset3) {
	/* Swap them so += looks nice */
	int t;

	t = v->offset2;
	v->offset2 = v->offset3;
	v->offset3 = t;
	t = v->count2;
	v->count2 = v->count3;
	v->count3 = t;
	rv = 1;
    }

    if (v->count2 && v->count3 && v->offset2 == v->offset3) {
	/* Humm, shouldn't this have been done already ? */
	v->count2 += v->count3;
	v->count3 = 0;
	rv = 1;
    }

    if (v->count2) {
	find_known_value(v->prev, v->offset2, 3, &n2, 
		    &const_found2, &known_value2, &non_zero_unsafe2);
	n2_valid = 1;
    }

    if (v->count3) {
	find_known_value(v->prev, v->offset3, 3, &n3, 
		    &const_found3, &known_value3, &non_zero_unsafe3);
	n3_valid = 1;
    }

    if (const_found2) {
	v->count += v->count2 * known_value2;
	v->count2 = 0;
	rv = 1;
	n2_valid = 0;
    }

    if (const_found3) {
	v->count += v->count3 * known_value3;
	v->count3 = 0;
	rv = 1;
    }

    if (v->count2 == 0 && v->count3 != 0) {
	v->offset2 = v->offset3;
	v->count2 = v->count3;
	const_found2 = const_found3;
	known_value2 = known_value3;
	non_zero_unsafe2 = non_zero_unsafe3;
	n2 = n3;
	v->count3 = 0;
	rv = 1;
	n2_valid = n3_valid;
	n3_valid = 0;
    }

    if (n2 && n2->type == T_SET && n2_valid &&
	    v->count2 == 1 && v->count3 == 0 && v->count == 0) {
	/* A direct assignment from n2 but not a constant */
	/* plus n2 seems safe to delete. */
	int is_ok = 1;

	if (is_ok && n2->count2)
	    is_ok = search_from_a_to_b_for_update_of_offset(n2, v, n2->offset2);

	if (is_ok && n2->count3)
	    is_ok = search_from_a_to_b_for_update_of_offset(n2, v, n2->offset3);

	if (is_ok) {
	    v->count = n2->count;
	    v->count2 = n2->count2;
	    v->offset2 = n2->offset2;
	    v->count3 = n2->count3;
	    v->offset3 = n2->offset3;
	    if (n2->offset == v->offset) {
		struct bfi *n4;

		n2->type = T_NOP;
		n4 = n2; n2 = n2->prev;
		if (n2) n2->next = n4->next; else bfprog = n4->next;
		if (n4->next) n4->next->prev = n2;
		free(n4);
		if (verbose>5) fprintf(stderr, "  Delete old T_EQU.\n");
		if (n3 == n2) n3 = 0;
		n2 = 0;
	    }
	    rv = 1;
	    n2_valid = 0;
	}
    }

    if (n2 && n2->type == T_SET && n2_valid &&
	    v->count2 == 1 && v->count3 == 0 && v->count == 0 &&
	    n2->next == v && 
	    v->next && v->next->type == T_EQU && v->next->offset == n2->offset) {
	/* A direct assignment from n2 and it's the previous node and the next
	 * node wipes it. */
	/* This is a BF standard form. */

	int t;
	t = n2->offset;
	n2->offset = v->offset;
	v->offset = t;
	v->offset2 = n2->offset;
	v->type = T_NOP;
	n2_valid = 0;
	rv = 1;
    }

    if (v->count2 == 0 && v->count3 == 0) {
	/* This is now a simple T_EQU */
	v->type = T_EQU;
	if (cell_mask > 0)
	    v->count = SM(v->count);
	rv = 1;
    }

    if (v->count2 == 1 && v->count3 == 0 && v->offset == v->offset2) {
	/* This is now a simple T_ADD */
	v->type = T_ADD;
	if (cell_mask > 0)
	    v->count = SM(v->count);
	/* Okay, it's actually a T_NOP now */
	if (v->count == 0)
	    v->type = T_NOP;
	rv = 1;
    }

    return rv;
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
    int has_equ = 0;
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
    while(n != v->jmp)
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
		if (n->type == T_EQU) has_equ = 1;
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
    if (dec_node != v->jmp->prev) {
	struct bfi *n1, *n2;

	/* Snip it out of the loop */
	n1 = dec_node;
	n2 = n1->prev;
	n2->next = n1->next;
	n2 = n1->next;
	n2->prev = n1->prev;

	/* And put it back */
	n2 = v->jmp->prev;
	n1->next = n2->next;
	n1->prev = n2;
	n2->next = n1;
	n1->next->prev = n1;
    }

    if (!has_add && !has_equ) {
	n->type = T_NOP;
	v->type = T_NOP;
	dec_node->type = T_EQU;
	dec_node->count = 0;
    } else if (is_znode) {
	v->type = T_IF;
    } else {
	v->type = T_CMULT;
	/* Without T_EQU if all offsets >= Loop offset we don't need the if. */
	/* BUT: Any access below the loop can possibly be before the start
	 * of the tape IF we've got a T_MOV! */
	if (!has_equ) {
	    if (!has_belowloop || !hard_left_limit)
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
    if (do_codestyle == c_bf) return 0;

    n = v->next;
    while(n != v->jmp)
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

void
print_asm_string(char * charmap, char * strbuf, int strsize)
{
static int textno = -1;
    int saved_line = curr_line;
    if (textno == -1) {
	textno++;

	printf("%%line 1 lib_string.s\n");
	curr_line = -1;
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
	    printf("%%line 1 lib_putch_%02x.s\n", ch);
	    printf("section .textlib\n");
	    printf("litprt_%02x:\n", ch);
	    printf("\tmov al,0x%02x\n", ch);
	    printf("\tjmp putch\n");
	    printf("section .text\n");
	    printf("%%line %d+0 %s\n", saved_line, curfile);
	    curr_line = saved_line;
	}
	printf("\tcall litprt_%02x\n", ch);
    } else {
	int i;
	textno++;
	if (saved_line != curr_line) {
	    printf("%%line %d+0 %s\n", saved_line, curfile);
	    curr_line = saved_line;
	}
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
    char * neartok = "";

    memset(charmap, 0, sizeof(charmap));
    curr_line = -1;

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

    Sigh ...
    Modern processors don't seem to have a true assembly language; this one
    is so full of old junk that it's impossible to optimise without full and
    complete documentation on the specific processor variant.

    In a real assembly language the only reason for there to be more limited
    versions of instructions is because they're better in some way. perhaps
    faster, perhaps they don't touch the CC register.

    This processor doesn't run x86 machine code; it translates it on the fly
    to it's real internal machine code.

    I need an assembler for *that* machine code, obviously to run it the 
    assembler will have to do a reverse translation to x86 machine code
    though.
*/

    if (!noheader) {
	calculate_stats();

	printf("; %s, asmsyntax=nasm\n", curfile);
	printf("; nasm -f bin -Ox brainfuck.asm ; chmod +x brainfuck\n");
	printf("\n");
	printf("BITS 32\n");
	printf("\n");
	printf("memsize\tequ\t0x10000\n");
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
	if (node_type_counts[T_MOV] != 0)
	    printf("\talign 64\n");
	printf("\tsection\t.textlib align=64\n");
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

	if (enable_trace) {
	    printf("; "); printtreecell(stdout, 0, n); printf("\n");
	}

	if (n->line != 0 && n->line != curr_line && n->type != T_PRT) {
	    printf("%%line %d+0 %s\n", n->line, curfile);
	    curr_line = n->line;
	}

	switch(n->type)
	{
	case T_MOV:
#if 1
	    if (n->count == 1)
		printf("\tinc ecx\n");
	    else if (n->count == -1)
		printf("\tdec ecx\n");
	    else if (n->count == 2)
		printf("\tinc ecx\n" "\tinc ecx\n");
	    else if (n->count == -2)
		printf("\tdec ecx\n" "\tdec ecx\n");
	    else if (n->count < -128)
		printf("\tsub ecx,%d\n", -n->count);
	    else
#endif
	    printf("\tadd ecx,%d\n", n->count);
	    break;

	case T_ADD:
#if 1
	    if (n->count == 1 && n->offset == 0)
		printf("\tinc byte [ecx]\n");
	    else if (n->count == -1 && n->offset == 0)
		printf("\tdec byte [ecx]\n");
	    else if (n->count < -128 && n->offset == 0)
		printf("\tsub byte [ecx],%d\n", -n->count);
	    else if (n->count == 1)
		printf("\tinc byte [ecx+%d]\n", n->offset);
	    else if (n->count == -1)
		printf("\tdec byte [ecx+%d]\n", n->offset);
	    else if (n->count < -128)
		printf("\tsub byte [ecx+%d],%d\n", n->offset, -n->count);
	    else
#endif
	    printf("\tadd byte [ecx+%d],%d\n", n->offset, SM(n->count));
	    break;
	    
	case T_EQU:
	    printf("\tmov byte [ecx+%d],%d\n", n->offset, SM(n->count));
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
		    printf("\timul ebx,eax,%d\n", SM(n->count3));
		    printf("\tadd byte [ecx+%d],bl\n", n->offset);
		}
	    } else if (n->count2 == 1 && n->count3 == -1) {
		printf("\tmov al,byte [ecx+%d]\n", n->offset2);
		printf("\tsub al,byte [ecx+%d]\n", n->offset3);
		printf("\tmov byte [ecx+%d],al\n", n->offset);
	    } else {
		printf("\t; Full T_SET: [ecx+%d] = %d + [ecx+%d]*%d + [ecx+%d]*%d\n",
			n->offset, n->count,
			n->offset2, n->count2,
			n->offset3, n->count3);
		if (SM(n->count2) == 0) {
		    printf("\tmov bl,0\n");
		} else if (n->count2 == -1 ) {
		    printf("\tmov bl,byte [ecx+%d]\n", n->offset2);
		    printf("\tneg bl\n");
		} else {
		    printf("\tmov al,byte [ecx+%d]\n", n->offset2);
		    printf("\timul ebx,eax,%d\n", SM(n->count2));
		}
		if (SM(n->count3) != 0) {
		    printf("\tmov byte al,[ecx+%d]\n", n->offset3);
		    printf("\timul eax,eax,%d\n", SM(n->count3));
		    printf("\tadd bl,al\n");
		}
		printf("\tmov byte [ecx+%d],bl\n", n->offset);
	    }

	    if (SM(n->count))
		printf("\tadd byte [ecx+%d],%d\n", n->offset, SM(n->count));
	    break;
	    
	case T_PRT:
	    if (n->count > -1) {
		*sp++ = n->count;
		break;
	    }

	    if (n->count == -1) {
		print_asm_string(0,0,0);
		if (n->line != 0 && n->line != curr_line) {
		    printf("%%line %d+0 %s\n", n->line, curfile);
		    curr_line = n->line;
		}
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

	case T_ZFIND:
#if 0
	    if (n->next->count == 1) {
		printf("\tlea edi,[ecx+%d]\n",  n->offset);

		printf("\tcmp dh,byte [edi]\n");
		printf("\tjz end_%d\n", n->count);

		printf("\txor al,al\n");
		printf("\tmov ebx,ecx\n");

		printf("\txor ecx,ecx\n");
		printf("\tnot ecx\n");
		printf("\tcld\n");
		printf("\trepne	scasb\n");
		printf("\tnot ecx\n");

		printf("\tadd ecx,ebx\n");
		printf("\tdec ecx\n");
		printf("end_%d:\n", n->count);
		n = n->jmp;
	    } else
		goto While_loop_start;
	    break;

	While_loop_start:;
#endif

/* LoopClass: condition at 1=> end, 2=>start, 3=>both */
#define LoopClass 2

	case T_MULT: case T_CMULT:
	case T_MFIND:
	case T_ADDWZ: case T_IF: case T_FOR:
	case T_WHL:

	    /* Need a "near" for jumps that we know are a long way away because
	     * nasm is VERY slow at working out which sort of jump to use 
	     * without the hint. But we can't be sure exactly what number to
	     * put here without being a lot more detailed about the
	     * instructions we use.
	     * Thiry seems to give a "not too slow" compile of LostKng.b
	     */
	    if (abs(n->inum - n->jmp->inum) > 30)
		neartok = " near";
	    else
		neartok = "";

	    printf("\t; Loop %d, %s\n", n->count, tokennames[n->type]);
	    if (n->type == T_IF || (LoopClass & 2) == 2) {
		if ((LoopClass & 1) == 0)
		    printf("start_%d:\n", n->count);
		printf("\tcmp dh,byte [ecx+%d]\n", n->offset);
		printf("\tjz%s end_%d\n", neartok, n->count);
		if ((LoopClass & 1) == 1 && n->type != T_IF)
		    printf("loop_%d:\n", n->count);
	    } else {
		printf("\tjmp%s last_%d\n", neartok, n->count);
		printf("loop_%d:\n", n->count);
	    }
	    break;

	case T_END: 
	    if (abs(n->inum - n->jmp->inum) > 120)
		neartok = " near";
	    else
		neartok = "";

	    if (n->jmp->type == T_IF) {
		printf("end_%d:\n", n->jmp->count);
	    } else if ((LoopClass & 1) == 0) {
		printf("\tjmp%s start_%d\n", neartok, n->jmp->count);
		printf("end_%d:\n", n->jmp->count);
	    } else {
		if ((LoopClass & 2) == 0)
		    printf("last_%d:\n", n->jmp->count);
		printf("\tcmp dh,byte [ecx+%d]\n", n->jmp->offset);
		printf("\tjnz%s loop_%d\n", neartok, n->jmp->count);
		if ((LoopClass & 2) == 2)
		    printf("end_%d:\n", n->jmp->count);
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
	printf("\tsection\t.bss align=4096\n");
	if (!hard_left_limit) 
	    printf("\tresb 4096\n");
	printf("mem:\n");
    }
}

#ifdef USETCCLIB
void
run_ccode(void)
{
    char * ccode;
    size_t ccodelen;
    int imagesize;
    void * image = 0;

    FILE * ofd;
    TCCState *s;
    int rv;
    int (*func)(void);
    void * memp;

    ofd = open_memstream(&ccode, &ccodelen);
    print_ccode(ofd);
    if (ofd == NULL) { perror("open_memstream"); exit(7); }
    putc('\0', ofd);
    fclose(ofd);

#ifdef USEHUGERAM
    memp = map_hugeram();
#else
    memp = tcalloc(MEMSIZE, sizeof*p);
#endif

    s = tcc_new();
    if (s == NULL) { perror("tcc_new()"); exit(7); }
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);

    tcc_compile_string(s, ccode);

    tcc_add_symbol(s, "mem", memp);

    /* If our code was read from stdin it'll be done in standard mode,
     * the stdio stream is now modal (always a bad idea) so it's been switched
     * to standard mode, stupidly, it's now impossible to switch it back.
     */
    if (iostyle)
	tcc_add_symbol(s, "get_char", &get_char);

#if defined(TCC0925) && !defined(TCCDONE)
#define TCCDONE
#error "Version specific"
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

    /* This line produces a spurious warning. The issue is that ISO99 C 
     * provides no way to covert a void* to a function pointer. This is
     * because on some nasty old machines the pointers are not compatible.
     * For example 8086 'medium model'.
     */
    func = tcc_get_symbol(s, "main");
    if (!func) {
        fprintf(stderr, "Could not find compiled code entry point\n");
        exit(1);
    }
    tcc_delete(s);
    free(ccode);

    func();
    free(image);
#endif

#if !defined(TCCDONE)
#define TCCDONE
    rv = tcc_run(s, 0, 0);
    tcc_delete(s);
    free(ccode);
#endif

#ifndef USEHUGERAM
    free(memp);
#endif
}
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

	case T_SET:
	    printf("set [%d]=%d+[%d]*%d+[%d]*%d\n",
		n->offset, n->count, n->offset2, n->count2, n->offset3, n->count3);
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
	case T_MFIND:
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
	    last_offset = n->jmp->offset;
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
pint(int v)
{
    if (v<0) printf("(%d)", -v);
    else     printf("%d", v);
}

void 
print_bf(void)
{
    struct bfi *v, *n = bfprog;
    int last_offset = 0;
    int i, nocr = 0;

    if (verbose)
	fprintf(stderr, "Generating brainfuck code.\n");

    while(n)
    {
	if (enable_trace && !nocr) { /* Sort of! */
	    v = n;

	    do 
	    {
		printf("// %s", tokennames[v->type&0xF]);
		printf(" O="); pint(v->offset);
		switch(v->type) {
		case T_WHL: case T_ZFIND: case T_ADDWZ: case T_MFIND:
		    printf(" ID="); pint(v->count);
		    break;
		case T_END:
		    printf(" ID="); pint(v->jmp->count);
		    if (v->count != 0) { printf(" C="); pint(v->count); }
		    break;
		case T_ADD:
		    printf(" V="); pint(v->count);
		    break;
		case T_EQU:
		    printf(" ="); pint(v->count);
		    break;
		default:
		    printf(" C="); pint(v->count);
		    break;
		}
		if (v->count2 != 0) {
		    printf(" O2="); pint(v->offset2);
		    printf(" C="); pint(v->count2);
		}
		if (v->count3 != 0) {
		    printf(" O3="); pint(v->offset3);
		    printf(" C="); pint(v->count3);
		}
		printf(" @%d;%d", v->line, v->col);
		printf("\n");
		v=v->next;
	    }
	    while(v && v->prev->type != T_END && (
		n->type == T_MULT ||
		n->type == T_CMULT ||
		n->type == T_ADDWZ ||
		n->type == T_IF ||
		n->type == T_FOR));
	}

	if (n->type == T_MOV) {
	    last_offset -= n->count;
	    n = n->next;
	    continue;
	}

	if (n->offset!=last_offset) {
	    while(n->offset>last_offset) { putchar('>'); last_offset++; };
	    while(n->offset<last_offset) { putchar('<'); last_offset--; };
	    if (!nocr)
		printf("\n");
	}

	switch(n->type)
	{
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
		fprintf(stderr, "Optimisation level too high for BF output\n");
		exit(1);
	    } else
		putchar('.');
	    break;

	case T_INP:
	    putchar(',');
	    break;

	case T_ZFIND:
	    if (n->next->next != n->jmp) {
		putchar('[');
	    } else {
		int i;
		putchar('[');
		if (n->next->count >= 0) {
		    for(i=0; i<n->next->count; i++)
			putchar('>');
		} else {
		    for(i=0; i< -n->next->count; i++)
			putchar('<');
		}
		putchar(']');
		n=n->jmp;
	    }
	    break;

	case T_MULT: case T_CMULT:
	case T_ADDWZ: case T_IF: case T_FOR:
	case T_MFIND:
	    nocr = 1;
	case T_WHL:
	    putchar('[');
	    // BFBasic
	    // if (n->offset == 2) printf("\n> [-]>[-]>[-]>[-]>[-]>[-]>[-]><<<<<<< <");
	    break;

	case T_END: 
	    putchar(']');
	    nocr = 0;
	    break;

	case T_NOP: 
	    fprintf(stderr, "Warning on code generation: "
	           "NOP node: ptr+%d, cnt=%d, @(%d,%d).\n",
		    n->offset, n->count, n->line, n->col);
	    break;

	default:
	    fprintf(stderr, "Error on code generation:\n"
	           "Bad node: %s ptr+%d, cnt=%d.\n",
		    tokennames[n->type], n->offset, n->count);
	    fprintf(stderr, "Optimisation level too high for BF output\n");
	    exit(1);
	}
	if (!nocr)
	    putchar('\n');
	n=n->next;
    }
}

int
get_char(void)
{
    int c;
    /* The "standard" getwchar() is really dumb, it will refuse to read
     * characters from a stream it getchar() has touched it first.
     * It does this EVEN if this is an ASCII+Unicode machine.
     *
     * So I have to mess around with this...
     */
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
    return c;
}

void
set_cell_size(int cell_bits)
{
    cell_size = cell_bits;
    cell_mask = (1 << cell_size) - 1;
    if (cell_mask <= 0) cell_mask = -1;

    if (cell_size < 7) {
	fprintf(stderr, "A minumum cell size of 7 bits is needed for ASCII.\n");
	exit(1);
    } else if (cell_size <= 8)
	cell_type = "unsigned char";
    else if (cell_size <= 16)
	cell_type = "unsigned short";
    else if (cell_size > sizeof(int)*8) {
	UsageInt64();
    } else
	cell_type = "int";
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
    if (!hard_left_limit)
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
#ifdef __GNUC__
__attribute((optimize(3),hot))
#endif
convert_tree_to_runarray(void)
{
    struct bfi * n = bfprog;
    int arraylen = 0;
    int * progarray = 0;
    int * p;
    int last_offset = 0;
    icell * m;
#ifndef USEHUGERAM
    void * freep;
#endif
#ifdef M
    if (sizeof(*m)*8 != cell_size && cell_size>0) {
	fprintf(stderr, "Sorry, the interpreter has been configured with a fixed cell size of %d bits\n", sizeof(icell)*8);
	exit(1);
    }
#else
#define M(x) UM(x)
#endif

#ifdef USEHUGERAM
    m = map_hugeram();
#else
    m = freep = tcalloc(MEMSIZE, sizeof*p);
#endif

    while(n)
    {
	switch(n->type)
	{
	case T_MOV: case T_INP:
	    arraylen += 3;
	    break;

	case T_PRT: case T_ADD: case T_EQU: case T_END:
	case T_WHL: case T_MULT: case T_CMULT:
	case T_ZFIND: case T_ADDWZ: case T_FOR: case T_IF:
	case T_MFIND:
	    arraylen += 3;
	    break;

	case T_SET:
	    arraylen += 7;
	    break;

	default:
	    fprintf(stderr, "Invalid node type found = %d\n", n->type);
	    exit(1);
	}
	n = n->next;
    }

    p = progarray = calloc(arraylen+2, sizeof*progarray);
    if (!progarray) { perror(program); exit(1); }
    n = bfprog;

    last_offset = 0;
    while(n)
    {
	if (n->type != T_MOV) {
	    *p++ = (n->offset - last_offset);
	    last_offset = n->offset;
	}
	else {
	    last_offset -= n->count;
	    n = n->next;
	    continue;
	}

	*p++ = n->type;
	switch(n->type)
	{
	case T_MOV: 
	    *p++ = n->count;
	    break;

	case T_INP:
	    break;

	case T_PRT: case T_ADD: case T_EQU:
	    *p++ = n->count;
	    break;

	case T_ADDWZ:
	    *p++ = n->next->offset - last_offset;
	    *p++ = n->next->count;
	    *p++ = n->next->next->count;
	    n = n->jmp;
	    break;

	case T_ZFIND:
	    *p++ = n->next->count;
	    n = n->jmp;
	    break;

	case T_MFIND:
	    *p++ = n->next->next->next->count;
	    n = n->jmp;
	    break;

	case T_MULT: case T_CMULT:
	    /* Note: for these I could generate multiply code for then enclosed
	     * T_ADD instructions, but that will only happen if the optimiser
	     * section is turned off.
	     */
	case T_FOR: case T_IF:
	case T_WHL:
	    p[-1] = T_WHL;
	    /* Storing the location of the instruction in the T_END's count
	     * field; it's not normally used */
	    n->jmp->count = p-progarray;
	    *p++ = 0;
	    break;

	case T_END:
	    progarray[n->count] = (p-progarray) - n->count;
	    *p++ = -progarray[n->count];
	    break;

	case T_SET:
	    if (n->count3 == 0) {
	    /*  m[off] = m[off2]*count2 */
		p[-1] = T_SET2;
		*p++ = n->count;
		*p++ = n->offset2 - last_offset;
		*p++ = n->count2;
	    } else if ( n->count == 0 && n->count2 == 1 && n->offset == n->offset2 ) {
	    /*  m[off] += m[off3]*count3 */
		p[-1] = T_SET3;
		*p++ = n->offset3 - last_offset;
		*p++ = n->count3;
	    } else {
		*p++ = n->count;
		*p++ = n->offset2 - last_offset;
		*p++ = n->count2;
		*p++ = n->offset3 - last_offset;
		*p++ = n->count3;
	    }
	    break;
	}
	n = n->next;
    }
    *p++ = 0;
    *p++ = T_STOP;

    p = progarray;

    for(;;) {
	m += *p++;
	switch(*p)
	{
	case T_MOV: m += p[1]; p += 2; break;
	case T_ADD: *m += p[1]; p += 2; break;
	case T_EQU: *m = p[1]; p += 2; break;

	case T_END: if(M(*m) != 0) p += p[1];
	    p += 2;
	    break;

	case T_WHL:
	    if(M(*m) == 0) p += p[1];
	    p += 2;
	    break;

	case T_ADDWZ:
	    /* This is normally a running dec, it cleans up a rail */
	    while(M(*m)) {
		m[p[1]] += p[2];
		m += p[3];
	    }
	    p += 4;
	    break;

	case T_ZFIND:
	    /* Search along a rail til you find the end of it. */
	    while(M(*m)) {
		m += p[1];
	    }
	    p += 2;
	    break;

	case T_MFIND:
	    /* Search along a rail for a minus 1 */
	    while(M(*m)) {
		*m -= 1;
		m += p[1];
		*m += 1;
	    }
	    p += 2;
	    break;

	case T_SET:
	    *m = p[1] + m[p[2]] * p[3] + m[p[4]] * p[5];
	    p += 6;
	    break;

	case T_SET2:
	    *m = p[1] + m[p[2]] * p[3];
	    p += 4;
	    break;

	case T_SET3:
	    *m += m[p[1]] * p[2];
	    p += 3;
	    break;

	case T_INP:
	    {
		int c = get_char();
		if (c != EOF) *m = c;
	    }
	    p += 1;
	    break;

	case T_PRT:
	    {
		int ch = M(p[1] == -1?*m:p[1]);
		if (ch == '\n' && output_limit && --output_limit == 0) {
		    fprintf(stderr, "Output limit reached\n");
		    goto break_break;
		}

		if (ch > 127 && iostyle == 1)
		    printf("%lc", ch);
		else
		    putchar(ch);
	    }
	    p += 2;
	    break;

	case T_STOP:
	    goto break_break;
	}
    }
break_break:;
    free(progarray);
#ifndef USEHUGERAM
    free(freep);
#endif
#undef icell
#undef M
}
