/*
This is a BF, compiler, interpreter and converter.
It can convert to C, x86 assembler or it can run the BF itself.

It has four methods of running the program, GNU "lightning" JIT, TCCLIB 
complied C code, a fast array based interpreter and a slower profiling
and tracing tree based interpreter.

In the simple case the C conversions for BF are:

    >   becomes   ++p;			<   becomes   --p;	    
    +   becomes   ++*p;			-   becomes   --*p;	    
    .   becomes   putchar(*p);		,   becomes   *p = getchar();
    [   becomes   while (*p) {		]   becomes   }

Or
    .   becomes   write(1,*p,1);	,   becomes   read(0,*p,1);
    [   becomes   if(*p) do {;		]   becomes   } while(*p);

Or
    [   becomes   if(!*p) goto match;	]   becomes   if(*p) goto match;

Note: read() and write() only work with byte cells, getchar() only works
correctly with cells larger than a byte.

For best speed you should use an optimising C compiler like GCC, the
interpreter loops especially require perfect code generation for best
speed.  Even with 'very good' code generation the reaction of a modern
CPU is very unpredictable.

Note: If you're generating C code for later telling this program the
Cell size you will be using allows it to generate better code.

------------------------------------------------------------------------------

TODO:
    NAME THIS INTERPRETER!

    T_INP if known_value T_INP unchanged on EOF if not EOF = 0 for <= 8bit and
    EOF = -1 for > 8bit.

    Add option for "*123" suffixes to "<>-+" commands for user RLE.

    Add option to strip/not-strip CR from getch() input.

    Allow generated C code use a expanding realloc'd array.
	"if ((m+=2) >= memend) m = realloc_mem(m);"
    For negative too ?
	Note: makes railrunners very slow.

    Move non-C generators (or all ?) into other files.
	Each type has a #define to enable it in this file.
	Plus an overall one.

    Other macros "[>>]" rail runner extensions, array access.
	Mole style array access has NO prequesites.

    Switch the token to a register machine? Temp? Only for basic blocks?

    Note: With T_IF loops the condition may depend on the cell size.
	If so, within the loop the cell size may be known.

    IF "known non-zero" at start of loop --> do { } while();

    New C function on Level 2 end while.
	Outer while and direct code in main() rest in functions.
	Useful for a when using a large outer loop to simulate JMPs.
	(ie: while(jmp) switch(jmp) {...} )

    BF instruction counting, even with optimisation.

    T_HINT(offset), save results of known value search.
	-- Problem, values become known as the code simplifies.
	-- Manually added hints in a hash array?
	-- Only use the manual hints "sometimes", when?

    Code: "m[0]++; m[1] += !m[0];" 16 bit inc.
	If T_MOV common factors includes 2 this will always be aligned.
	But code for 16bit inc uses both m[-1] and m[2].
	Probably requires known (8bit) cell size.

    Multiple files, loaded then run in sequence on same tape.
	Common tape.
	Common pointer ?
	Common control flow ?

    Define comment start on command line. eg: "bfi -C //"
	-- If first 2 characters are '#!' allow comments beginning with '#'
	-- Poss if first character is a '#'

	#!	Comment if at start of FILE
	#	Comment if at SOL
	%	Comment character til EOL
	//  Comment til EOL.

	*/ /*	C comments or REVERSED C comments ?


    ;	Input cell as a decimal NUMBER, trim initial whitespace.
    :	Output cell as a decimal NUMBER
    #	Debugging flag.

#ifdef DEBUG
        case '@': {int a; scanf("%d", &a); m=a;} break;
        case '=': {int a; scanf("%d", &a); mem[m]=a;} break;
        case '?': printf("Cell %d has value %d\n", m, mem[m]); break;
        case '#':
            {
                int i,j;
                printf("Pointer = %d\n", m);
                for(i=0; i<48; i+=16) {
                    for(j=0; j<16; j++) {
                        printf("%s%5d", (i+j)==m?">":" ",mem[i+j]);
                    }
                    printf("\n");
                }
            }
            break;
#endif

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

#include "bfi.tree.h"
#include "bfi.run.h"

#ifndef NO_EXT_OUT
#include "bfi.nasm.h"
#include "bfi.bf.h"
#include "bfi.jit.h"
#endif

#ifdef MAP_NORESERVE
#define USEHUGERAM
#endif

#ifdef USEHUGERAM
#define MEMSIZE	    2UL*1024*1024*1024
#define MEMGUARD    16UL*1024*1024
#define MEMSKIP	    1UL*1024*1024
#else
#ifndef MEMSIZE
#define MEMSIZE	    256*1024
#endif
#warning Using small memory, define MEMSIZE to increase.
#endif

#if !defined(__STRICT_ANSI__) && !defined(NO_TCCLIB)
#define USETCCLIB
#endif

#ifdef USETCCLIB
#include <libtcc.h>
#endif

char * program = "C";
int verbose = 0;
int noheader = 0;
int do_run = 0;
enum codestyle { c_default, c_c,
#ifdef _BFI_NASM_H
    c_asm,
#endif
#ifdef _BFI_JIT_H
    c_jit,
#endif
#ifdef _BFI_BF_H
    c_bf,
#endif
    c_adder };
int do_codestyle = c_default;

int opt_level = 3;
int hard_left_limit = 0;
int enable_trace = 0;
int iostyle = 0; /* 0=ASCII, 1=UTF8 */
int eofcell = 0; /* 0=> Default, 1=> No Change, 2= -1, 3= 0, 4=EOF, 5=No Input. */

int cell_size = 0;  /* 0 is 8,16,32 or more. 7 and up are number of bits */
int cell_mask = -1; /* -1 is don't mask. */
char *cell_type = "C";
#define SM(vx) (( ((int)(vx)) <<(32-cell_size))>>(32-cell_size))
#define UM(vx) ((vx) & cell_mask)

char * curfile = 0;
int curr_line = 0, curr_col = 0;
int cmd_line = 0, cmd_col = 0;
int bfi_num = 0;

#define GEN_TOK_STRING(NAME) "T_" #NAME,
const char* tokennames[] = { TOKEN_LIST(GEN_TOK_STRING) };

struct bfi *bfprog = 0;

/* Stats */
double run_time = 0;
int loaded_nodes = 0;
int total_nodes = 0;
int node_type_counts[TCOUNT+1];
int node_profile_counts[TCOUNT+1];
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
void printtreecell(FILE * efd, int indent, struct bfi * n);
void printtree(void);
void run_tree(void);
void delete_tree(void);
void run_ccode(void);
void print_ccode(FILE * ofd);
void calculate_stats(void);
void pointer_scan(void);
void quick_scan(void);
void invariants_scan(void);
void trim_trailing_sets(void);
int find_known_state(struct bfi * v);
int find_known_calc_state(struct bfi * v);
int flatten_loop(struct bfi * v, int constant_count);
int classify_loop(struct bfi * v);
int flatten_multiplier(struct bfi * v);
void build_string_in_tree(struct bfi * v);
void * tcalloc(size_t nmemb, size_t size);
struct bfi * add_node_after(struct bfi * p);
void print_adder(void);
void * map_hugeram(void);
void trap_sigsegv(void);
int getch(int oldch);
void putch(int oldch);
void set_cell_size(int cell_bits);
void convert_tree_to_runarray(void);

void LongUsage(FILE * fd)
{
    fprintf(fd, "%s: Version 0.1.0\n", program);
    fprintf(fd, "Usage: %s [options] [BF file]\n", program);
    if (fd != stdout) {
	fprintf(fd, "   -h   Long help message.\n");
	fprintf(fd, "   -v   Verbose, repeat for more.\n");
	fprintf(fd, "   -r   Run in interpreter.\n");
	fprintf(fd, "   -c   Create C code.\n");
	exit(1);
    }

    printf("   -h   This message.\n");
    printf("   -v   Verbose, repeat for more.\n");
    printf("        Three -v or more switches to profiling interpreter\n");
    printf("   -r   Run in interpreter. Default is fastest available interpreter.\n");
#ifdef _BFI_JIT_H
    printf("   -j   Run using the JIT interpreter/compiler.\n");
#endif
#ifdef USETCCLIB
    printf("   -c   Create C code. If combined with -r the code is run using TCCLIB.\n");
#else
    printf("   -c   Create C code.\n");
#endif
#ifdef _BFI_NASM_H
    printf("   -s   Create NASM code.\n");
#endif
    printf("\n");
    printf("   -T   Create trace statements in output C code or switch to\n");
    printf("        profiling interpreter and turn on tracing.\n");
    printf("   -H   Remove headers in output code\n");
    printf("        Also prevents optimiser assuming the tape starts blank.\n");
    printf("   -u   Wide character (unicode) I/O%s\n", iostyle?" (default)":"");
    printf("   -a   Ascii I/O%s\n", iostyle?"":" (default)");
    printf("\n");
    printf("   -On  'Optimisation level'\n");
    printf("   -O0      Turn off all optimisation, just leave RLE.\n");
    printf("   -O1      Only enable pointer motion optimisation.\n");
    printf("   -O2      Allow a few simple optimisations.\n");
    printf("   -O3      Maximum normal level, default.\n");
    printf("   -O-1     Turn off all optimisation, disable RLE too.\n");
    printf("   -m   Minimal processing; same as -O0\n");
    printf("\n");
    printf("   -B8  Use 8 bit cells.\n");
    printf("   -B16 Use 16 bit cells.\n");
    printf("   -B32 Use 32 bit cells.\n");
    printf("        Default for C code is 'unknown', ASM can only be 8bit.\n");
    printf("        Other bitwidths work (including 7) for the interpreter and C.\n");
    printf("        Full Unicode characters need 21 bits.\n");
    printf("        The optimiser may work better if this is not unknown.\n");
    printf("\n");
    printf("   -En  End of file processing for '-r'.\n");
    printf("   -E1      End of file gives no change for ',' command.\n");
    printf("   -E1      End of file gives no change for ',' command.\n");
    printf("   -E2      End of file gives -1.\n");
    printf("   -E3      End of file gives 0.\n");
    printf("   -E4      End of file gives EOF.\n");
    printf("   -E5      Disable ',' command.\n");
    printf("\n");
#ifdef _BFI_BF_H
    printf("   -F       Attempt to regenerate BF code.\n");
#endif
    printf("   -A       Generate token list output, possibly suitable as a base for a macro assembler.\n");
    exit(1);
}

void
Usage(void)
{
    LongUsage(stderr);
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
                    case 'h': LongUsage(stdout); break;
                    case 'v': verbose++; break;
                    case 'H': noheader=1; break;
                    case 'r': do_run=1; break;
                    case 'c': do_codestyle = c_c; break;
                    case 'A': do_codestyle = c_adder; break;
#ifdef _BFI_JIT_H
                    case 'j': do_run=1; do_codestyle = c_jit; break;
#endif
#ifdef _BFI_NASM_H
                    case 's': do_codestyle = c_asm; break;
#endif
#ifdef _BFI_BF_H
                    case 'F': do_codestyle = c_bf; 
			hard_left_limit = 1;
			break;
#endif
                    case 'm': opt_level=0; break;
                    case 'T': enable_trace=1; break;
                    case 'u': iostyle=1; break;
                    case 'a': iostyle=0; break;

                    default: 
                        ch = *p;
#ifdef TAIL_ALWAYS_ARG
                        if (p[1]) { ap = p+1; p=" "; }
#else
#ifdef NO_NUL_ARG
                        if (p==argv[ar]+1 && p[1]) { ap = p+1; p=" "; }
#else
                        if (p==argv[ar]+1) { ap = p+1; p=" "; }
#endif
#endif
                        else {
                            if (ar+1>=argc) Usage();
                            ap = argv[++ar];
                        }
                        switch(ch) {
                            case 'O':	if(*ap == 0) opt_level = 1;
					else opt_level = strtol(ap,0,10);
					break;
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

#ifdef _BFI_NASM_H
    if (do_codestyle == c_asm)
    {
	if (do_run || (cell_size && cell_size != 8)) {
	    fprintf(stderr, "The -s flag cannot be combined with -r or -B\n");
	    exit(255);
	}
	set_cell_size(8);
    }
#endif

    if (!do_run && do_codestyle == c_default) {
	do_run = 1;
#ifdef _BFI_JIT_H
	if (verbose<3 && !enable_trace)
	    do_codestyle = c_jit;
#else
#ifdef USETCCLIB
	if (verbose<3)
	    do_codestyle = c_c;
#endif
#endif
    }

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

    if (n) { while(n->next) n=n->next; p = n->prev;}

    curr_line = 1; curr_col = 0;

    if (!fname || strcmp(fname, "-") == 0) ifd = stdin;
    else if ((ifd = fopen(fname, "r")) == 0) {
	perror(fname);
	exit(1);
    }
    while((ch = getc(ifd)) != EOF && (ifd!=stdin || ch != '!' || !n)) {
	int c = 0;
	if (ch == '\n') { curr_line++; curr_col=0; }
	else curr_col ++;

	switch(ch) {
	case '>': ch = T_MOV; c=  1; break;
	case '<': ch = T_MOV; c= -1; break;
	case '+': ch = T_ADD; c=  1; break;
	case '-': ch = T_ADD; c= -1; break;
	case '[': ch = T_WHL; break;
	case ']': ch = T_END; break;
	case '.': ch = T_PRT; break;
	case ',': ch = T_INP; break;
	default:  ch = T_NOP; break;
	}
	if (ch != T_NOP) {
#ifndef NO_BFCOMMENT
	    /* Comment loops, can never be run */
	    /* This BF code isn't just dead it's been buried in soft peat
	     * for three months and recycled as firelighters. */

	    if (dld ||  (ch == T_WHL && p==0 && !noheader) ||
			(ch == T_WHL && p!=0 && p->type == T_END)   ) {
		if (ch == T_WHL) dld ++;
		if (ch == T_END) dld --;
		continue;
	    }
#endif
#ifndef NO_RLE
	    if (opt_level>=0) {
		/* RLE compacting of instructions. */
		if (p && ch == p->type && p->count ){
		    p->count += c;
		    continue;
		}
	    }
#endif
	    n = tcalloc(1, sizeof*n);
	    n->inum = bfi_num++;
	    if (p) { p->next = n; n->prev = p; } else bfprog = n;
	    n->type = ch; p = n;
	    n->line = curr_line; n->col = curr_col;
	    if (n->type == T_WHL) { n->jmp=j; j = n; }
	    else if (n->type == T_END) {
		if (j) { n->jmp = j; j = j->jmp; n->jmp->jmp = n;
		} else n->type = T_ERR;
	    } else
		n->count = c;
	}
    }
    if (ifd!=stdin) fclose(ifd);

    while (j) { n = j; j = j->jmp; n->type = T_ERR; n->jmp = 0; }

    total_nodes = loaded_nodes = 0;
    for(n=bfprog; n; n=n->next) {
	switch(n->type)
	{
	    case T_WHL: n->count = ++lid; break;
	    case T_INP: if (eofcell == 5) n->type = T_NOP; break;
	    case T_PRT: n->count = -1; break;

	    case T_ERR: /* Unbalanced bkts */
		fprintf(stderr,
			"Warning: unbalanced bracket at Line %d, Col %d\n",
			n->line, n->col);
		n->type = T_NOP;
		break;
	}
	loaded_nodes++;
	n->orgtype = n->type;
    }
    total_nodes = loaded_nodes;
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

struct bfi *
add_node_after(struct bfi * p)
{ 
    struct bfi * n = tcalloc(1, sizeof*n);
    n->inum = bfi_num++;
    n->type = T_NOP;
    n->orgtype = T_NOP;
    if (p) {
	n->line = p->line;
	n->col = p->col;
	n->prev = p;
	n->next = p->next;
	if (n->next) n->next->prev = n;
	n->prev->next = n;
    } else if (bfprog) {
	fprintf(stderr, "Error in add_node_after()\n");
    } else bfprog = n;

    return n;
}

void 
printtreecell(FILE * efd, int indent, struct bfi * n)
{
    int i = indent;
    while(i-->0) 
	fprintf(efd, " ");
    if (n == 0) {fprintf(efd, "NULL Cell"); return; }
    if (n->type >= 0 && n->type < TCOUNT)
	fprintf(efd, "%s", tokennames[n->type]);
    else
	fprintf(efd, "TOKEN(%d)", n->type);
    switch(n->type)
    {
    case T_MOV:
	if(n->offset == 0)
	    fprintf(efd, " %d ", n->count);
	else
	    fprintf(efd, " %d,(off=%d), ", n->count, n->offset);
	break;
    case T_ADD: case T_SET:
	fprintf(efd, "[%d]:%d, ", n->offset, n->count);
	break;
    case T_CALC:
	if (n->count3 == 0) {
	    fprintf(efd, "[%d] = %d + [%d]*%d, ",
		n->offset, n->count, n->offset2, n->count2);
	} else if (n->offset == n->offset2 && n->count2 == 1) {
	    fprintf(efd, "[%d] += [%d]*%d + %d, ",
		n->offset, n->offset3, n->count3, n->count);
	} else
	    fprintf(efd, "[%d] = %d + [%d]*%d + [%d]*%d, ",
		n->offset, n->count, n->offset2, n->count2,
		n->offset3, n->count3);
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
    case T_STOP: fprintf(efd, "[%d], ", n->offset); break;

    case T_ZFIND: case T_MFIND: case T_ADDWZ:
    case T_FOR: case T_IF: case T_MULT: case T_CMULT:
    case T_WHL:
	fprintf(efd, "[%d],id=%d, ", n->offset, n->count);
	break;

    case T_END: case T_ENDIF:
	fprintf(efd, "[%d] id=%d, ", n->offset, n->jmp->count);
	break;

    default:
	fprintf(efd, "[%d]:%d, ", n->offset, n->count);
	if (n->offset2 != 0 || n->count2 != 0)
	    fprintf(efd, "x2[%d]:%d, ", n->offset2, n->count2);
	if (n->offset3 != 0 || n->count3 != 0)
	    fprintf(efd, "x3[%d]:%d, ", n->offset3, n->count3);
    }
    fprintf(efd, "$%d, ", n->inum);
    if (indent>=0) {
	if(n->next == 0) 
	    fprintf(efd, "next 0, ");
	else if(n->next->prev != n) 
	    fprintf(efd, "next $%d, ", n->next->inum);
	if(n->prev == 0)
	    fprintf(efd, "prev 0, ");
	else if(n->prev->next != n) 
	    fprintf(efd, "prev $%d, ", n->prev->inum);
	if(n->jmp) 
	    fprintf(efd, "jmp $%d, ", n->jmp->inum);
	if(n->prevskip)
	    fprintf(efd, "skip $%d, ", n->prevskip->inum);
	if(n->profile) 
	    fprintf(efd, "prof %d, ", n->profile);
	if(n->line || n->col)
	    fprintf(efd, "@(%d,%d)", n->line, n->col);
	else
	    fprintf(efd, "@new");
    } else {
	if(n->jmp) 
	    fprintf(efd, "jmp $%d, ", n->jmp->inum);
	if(n->profile) 
	    fprintf(efd, "prof %d, ", n->profile);
    }
}

void
printtree(void)
{
    int indent = 0;
    struct bfi * n = bfprog;
    fprintf(stderr, "Whole tree dump...\n");
    while(n)
    {
	fprintf(stderr, " ");
	if (indent>0 && n->orgtype == T_END) indent--;
	fprintf(stderr, " ");
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
	    printtreecell(ofd, -1, n);
	    fprintf(ofd, "\",m+ %d)\n", n->offset);
	}
    }
}

void
print_c_header(FILE * ofd, int * minimal_p)
{
    int use_full = 0;
    int memsize = 60000;
    int memoffset = 0;

    calculate_stats();

    if (enable_trace || do_run || !minimal_p) use_full = 1;

    if (total_nodes == node_type_counts[T_PRT] && !use_full) {
	fprintf(ofd, "#include <stdio.h>\n\n");
	fprintf(ofd, "int main(void)\n{\n");
	*minimal_p = 1;
	return ;
    }

    if (node_type_counts[T_INP] != 0 && !do_run) {
	fprintf(ofd, "#ifdef WIDECHAR\n");
	fprintf(ofd, "#include <locale.h>\n");
	fprintf(ofd, "#include <wchar.h>\n");
	fprintf(ofd, "#endif\n\n");
    }

    fprintf(ofd, "#include <stdio.h>\n");
    fprintf(ofd, "#include <stdlib.h>\n\n");
    fprintf(ofd, "#include <stdint.h>\n\n");

    if (cell_size == 0) {
	fprintf(ofd, "# ifndef C\n");
	fprintf(ofd, "# define C int\n");
	fprintf(ofd, "# endif\n\n");
    }

    if (node_type_counts[T_INP] != 0 && !do_run)
    {
	fprintf(ofd, "static int \n");
	fprintf(ofd, "getch(int oldch)\n");
	fprintf(ofd, "{\n");
	fprintf(ofd, "    int ch;\n");
	fprintf(ofd, "#ifdef WIDECHAR\n");
	fprintf(ofd, "    ch = getwchar();\n");
	fprintf(ofd, "#else\n");
	fprintf(ofd, "    ch = getchar();\n");
	fprintf(ofd, "#endif\n");
	fprintf(ofd, "#ifndef EOFCELL\n");
	fprintf(ofd, "    if (ch != EOF) return ch;\n");
	fprintf(ofd, "    return oldch;\n");
	fprintf(ofd, "#else\n");
	fprintf(ofd, "#if EOFCELL == EOF\n");
	fprintf(ofd, "    return ch;\n");
	fprintf(ofd, "#else\n");
	fprintf(ofd, "    if (ch != EOF) return ch;\n");
	fprintf(ofd, "    return EOFCELL;\n");
	fprintf(ofd, "#endif\n");
	fprintf(ofd, "#endif\n");
	fprintf(ofd, "}\n\n");
    }

    if (node_type_counts[T_PRT] != 0 && !do_run) {
	fprintf(ofd, "#ifdef WIDECHAR\n");
	fprintf(ofd, "#define putch(ch) printf(\"%%lc\",ch)\n");
	fprintf(ofd, "#else\n");
	fprintf(ofd, "#define putch(ch) putchar(ch);\n");
	fprintf(ofd, "#endif\n\n");
    }

#if 0
    if (node_type_counts[T_ZFIND] != 0 &&
	    (cell_size == 16 || cell_size == 32)) {

	if (do_run) {
	    fprintf(ofd, "extern %s *\n", cell_type);
	    fprintf(ofd, "libbf_zfind%d(%s * m, int off);\n",
			    cell_size, cell_type);
	} else {
	    fprintf(ofd, "%s *\n", cell_type);
	    fprintf(ofd, "libbf_zfind%d(%s * m, int off) {\n",
			    cell_size, cell_type);
	    fprintf(ofd, "  while(*m) m += off;\n");
	    fprintf(ofd, "  return m;\n");
	    fprintf(ofd, "}\n\n");
	}
    }
#endif

    if (do_run) {
	fprintf(ofd, "extern void putch(int ch);\n");
	fprintf(ofd, "extern int getch(int ch);\n");
    }

    if (enable_trace)
	fputs(	"#define t(p1,p2,p3,p4) fprintf(stderr, \"P(%d,%d)=%s"
		"mem[%d]=%d%s\\n\", \\\n\tp1, p2, p3,"
		" p4-mem, (p4>=mem?*(p4):0), p4>=mem?\"\":\" SIGSEG\");\n\n",
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
	fprintf(ofd, "static %s m[%d];\n", cell_type, max_pointer+1);
	fprintf(ofd, "int main(){\n");
	if (enable_trace)
	    fprintf(ofd, "#define mem m\n");
    } else {
	if (!do_run)
	    fprintf(ofd, "static %s mem[%d];\n", cell_type, memsize);
	else
	    fprintf(ofd, "extern %s mem[];\n", cell_type);
	fprintf(ofd, "int main(){\n");
#if 0
	fprintf(ofd, "#if defined(__GNUC__) && defined(__i386__)\n");
	fprintf(ofd, "  register %s * m asm (\"%%ebx\");\n", cell_type);
	fprintf(ofd, "#else\n");
	fprintf(ofd, "  %s*m;\n", cell_type);
	fprintf(ofd, "#endif\n");
#else
	fprintf(ofd, "  static %s * m;\n", cell_type);
#endif

	if (memoffset > 0 && !do_run)
	    fprintf(ofd, "  m = mem + %d;\n", memoffset);
	else
	    fprintf(ofd, "  m = mem;\n");
    }

    if (node_type_counts[T_INP] != 0) {
	fprintf(ofd, "  setbuf(stdout, 0);\n");
	fprintf(ofd, "#ifdef WIDECHAR\n");
	fprintf(ofd, "  setlocale(LC_ALL, \"\");\n");
	fprintf(ofd, "#endif\n");
    }
}

void 
print_ccode(FILE * ofd)
{
    int indent = 0;
    struct bfi * n = bfprog;
    char string_buffer[180], *sp = string_buffer, lastspch = 0;
    int ok_for_printf = 0, spc = 0;
    int minimal = 0;
    int add_mask = 0;
    if (cell_size > 0 &&
	cell_size != sizeof(int)*8 &&
	cell_size != sizeof(short)*8 &&
	cell_size != sizeof(char)*8)
	add_mask = cell_mask;

    if (verbose)
	fprintf(stderr, "Generating C Code.\n");

    if (!noheader)
	print_c_header(ofd, &minimal);

    while(n)
    {
	if (n->type == T_PRT) 
	    ok_for_printf = ( ( n->count >= ' ' && n->count <= '~') ||
				(n->count == '\n') || (n->count == '\033'));

	if (sp != string_buffer) {
	    if ((n->type != T_SET && n->type != T_ADD && n->type != T_PRT) ||
	        (n->type == T_PRT && !ok_for_printf) ||
		(sp >= string_buffer + sizeof(string_buffer) - 8) ||
		(sp > string_buffer+1 && lastspch == '\n')
	       ) {
		*sp++ = 0;
		pt(ofd, indent,0);
		if (strchr(string_buffer, '%') == 0 )
		    fprintf(ofd, "printf(\"%s\");\n", string_buffer);
		else	
		    fprintf(ofd, "fwrite(\"%s\", 1, %d, stdout);\n",
			    string_buffer, spc);
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
	    if (n->count == 1)
		fprintf(ofd, "++m;\n");
	    else if (n->count == -1)
		fprintf(ofd, "--m;\n");
	    else if (n->count < 0)
		fprintf(ofd, "m -= %d;\n", -n->count);
	    else if (n->count > 0)
		fprintf(ofd, "m += %d;\n", n->count);
	    else
		fprintf(ofd, "/* m += 0; */\n");
	    break;
	case T_ADD:
	    pt(ofd, indent,n);
	    if(n->offset == 0) {
		if (n->count == 1)
		    fprintf(ofd, "++*m;\n");
		else if (n->count == -1)
		    fprintf(ofd, "--*m;\n");
		else if (n->count < 0)
		    fprintf(ofd, "*m -= %d;\n", -n->count);
		else if (n->count > 0)
		    fprintf(ofd, "*m += %d;\n", n->count);
		else
		    fprintf(ofd, "/* *m += 0; */\n");
	    } else {
		if (n->count == 1)
		    fprintf(ofd, "++m[%d];\n", n->offset);
		else if (n->count == -1)
		    fprintf(ofd, "--m[%d];\n", n->offset);
		else if (n->count < 0)
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
	case T_SET:
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

#if 0
	case T_CALC:
	    pt(ofd, indent, n);
	    {
		int ac = '=';
		fprintf(ofd, "m[%d]", n->offset);

		if (n->count2) {
		    int c = n->count2;
		    if (ac == '+' && c < 0) { c = -c; ac = '-'; };
		    if (ac == '=' && n->offset == n->offset2 && c == 1) {
			fprintf(ofd, " += ");
			ac = 0;
		    } else if (c == -1) {
			fprintf(ofd, " %c -m[%d]", ac, n->offset2);
			ac = '+';
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

		if (n->count) {
		    if (ac)
			fprintf(ofd, " %c %d", ac, n->count);
		    else
		        fprintf(ofd, " %d", n->count);
		    ac = '+';
		}

		if (ac == '=' || ac == 0)
		    fprintf(ofd, " = 0 /*T_CALC!*/");
		fprintf(ofd, ";\n");
	    }
#endif

	case T_CALC:
	    pt(ofd, indent, n);
	    do {
		if (n->count == 0) {
		    if (n->offset == n->offset2 && n->count2 == 1)
		    {
			if (n->count3 == 1) {
			    fprintf(ofd, "m[%d] += m[%d];\n",
				    n->offset, n->offset3);
			    break;
			}

			if (n->count3 == -1) {
			    fprintf(ofd, "m[%d] -= m[%d];\n",
				    n->offset, n->offset3);
			    break;
			}

			if (n->count3 != 0) {
			    fprintf(ofd, "m[%d] += m[%d]*%d;\n",
				    n->offset, n->offset3, n->count3);
			    break;
			}
		    }

		    if (n->count3 == 0 && n->count2 != 0) {
			if (n->count2 == 1) {
			    fprintf(ofd, "m[%d] = m[%d];\n",
				n->offset, n->offset2);
			} else if (n->count2 == -1) {
			    fprintf(ofd, "m[%d] = -m[%d];\n",
				n->offset, n->offset2);
			} else {
			    fprintf(ofd, "m[%d] = m[%d]*%d;\n",
				n->offset, n->offset2, n->count2);
			}
			break;
		    }

		    if (n->count3 == 1 && n->count2 != 0) {
			fprintf(ofd, "m[%d] = m[%d]*%d + m[%d];\n",
			    n->offset, n->offset2, n->count2, n->offset3);
			break;
		    }
		}

		if (n->offset == n->offset2 && n->count2 == 1) {
		    if (n->count3 == 1) {
			fprintf(ofd, "m[%d] += m[%d] + %d;\n",
				n->offset, n->offset3, n->count);
			break;
		    }
		    fprintf(ofd, "m[%d] += m[%d]*%d + %d;\n",
			    n->offset, n->offset3, n->count3, n->count);
		    break;
		}

		if (n->count3 == 0) {
		    if (n->count2 == 1) {
			fprintf(ofd, "m[%d] = m[%d] + %d;\n",
			    n->offset, n->offset2, n->count);
			break;
		    }
		    if (n->count2 == -1) {
			fprintf(ofd, "m[%d] = -m[%d] + %d;\n",
			    n->offset, n->offset2, n->count);
			break;
		    }

		    fprintf(ofd, "m[%d] = %d + m[%d]*%d;\n",
			n->offset, n->count, n->offset2, n->count2);
		} else {
		    fprintf(ofd, "m[%d] = %d + m[%d]*%d + m[%d]*%d; /*T_CALC*/\n",
			n->offset, n->count, n->offset2, n->count2,
			n->offset3, n->count3);
		}
	    } while(0);

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
		if (minimal)
		    fprintf(ofd, "printf(\"%%lc\", m[%d]);\n", n->offset);
		else if (n->offset == 0)
		    fprintf(ofd, "putch(*m);\n");
		else
		    fprintf(ofd, "putch(m[%d]);\n", n->offset);
	    } else {
		int ch = n->count;
		if (add_mask>0) ch &= add_mask;
		if (minimal) {
		    if (ch < 128)
			fprintf(ofd, "putchar(%d);\n", ch);
		    else
			fprintf(ofd, "printf(\"%%lc\",%d);\n", ch);
		} else if (ch >= ' ' && ch <= '~' &&
		    ch != '\'' && ch != '\\')
		    fprintf(ofd, "putch('%c');\n", ch);
		else if (ch == '\n')
		    fprintf(ofd, "putch('\\n');\n");
		else if (ch == '\t')
		    fprintf(ofd, "putch('\\t');\n");
		else if (ch == '\\')
		    fprintf(ofd, "putch('\\\\');\n");
		else
		    fprintf(ofd, "putch(%d);\n", ch);
	    }
	    break;
	case T_INP:
	    pt(ofd, indent,n);
	    if (n->offset == 0)
		fprintf(ofd, "*m = getch(*m);\n");
	    else
		fprintf(ofd, "m[%d] = getch(m[%d]);\n", n->offset, n->offset);
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
	    /* TCCLIB generates a slow 'strlen', libc is better, but the 
	     * function call overhead is a killer.
	     */
	    if (cell_size == 8 && add_mask <= 0 &&
		    n->next->next == n->jmp && n->next->count == 1) {
		pt(ofd, indent,n);
		if (n->offset)
		    fprintf(ofd, "m += strlen(m + %d);\n", n->offset);
		else
		    fprintf(ofd, "m += strlen(m);\n");
		n=n->jmp;
	    }
	    else if (n->next->next == n->jmp &&
			(cell_size == 16 || cell_size == 32)) {
		pt(ofd, indent,n);
		fprintf(ofd, "if(m[%d]) ", n->offset);
		if (n->offset)
		    fprintf(ofd, "m = libbf_zfind%d(m + %d, %d) - %d;\n",
			    cell_size, n->offset, n->next->count, n->offset);
		else
		    fprintf(ofd, "m = libbf_zfind%d(m, %d);\n",
			    cell_size, n->next->count);
		n=n->jmp;
	    }
	    else
#endif
	    {
		pt(ofd, indent,n);
		if (n->next->next != n->jmp) {
		    fprintf(stderr, "Code generator made a broken rail runner\n");
		    exit(1);
		}
		if (n->next->count == 1) {
		    fprintf(ofd, "while(m[%d]) ++m;\n", n->offset);
		} else if (n->next->count == -1) {
		    fprintf(ofd, "while(m[%d]) --m;\n", n->offset);
		} else if (n->next->count < 0) {
		    fprintf(ofd, "while(m[%d]) m -= %d;\n",
			n->offset, -n->next->count);
		} else {
		    fprintf(ofd, "while(m[%d]) m += %d;\n",
			n->offset, n->next->count);
		}
		n=n->jmp;
	    }
	    break;

	case T_WHL:
	case T_CMULT:
	case T_MULT:
	case T_MFIND:
	case T_ADDWZ:
	    pt(ofd, indent,n);
	    if (n->offset)
		fprintf(ofd, "while(m[%d]) {\n", n->offset);
	    else
		fprintf(ofd, "while(*m) {\n");
	    break;
	case T_FOR:
	    pt(ofd, indent,n);
	    fprintf(ofd, "for(;m[%d];) {\n", n->offset);
	    break;

	case T_END: case T_ENDIF:
	    pt(ofd, indent,n);
	    fprintf(ofd, "}\n");
	    break;

	case T_STOP: 
	    pt(ofd, indent,n);
	    fprintf(ofd, "return 1;\n");
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
	if (strchr(string_buffer, '%') == 0 )
	    fprintf(ofd, "  printf(\"%s\");\n", string_buffer);
	else
	    fprintf(ofd, "  fwrite(\"%s\", 1, %d, stdout);\n",
		    string_buffer, spc);
    }
    if (!noheader)
	fprintf(ofd, "  return 0;\n}\n");
}

void
process_file(void)
{
#define tickstart() do{ if (verbose>2) gettimeofday(&tv_step, 0); } while(0)
#define tickend(str) do{ \
	    if (verbose>2) {					    \
		gettimeofday(&tv_end, 0);			    \
		fprintf(stderr, str " %.3f\n",			    \
		(tv_end.tv_sec - tv_step.tv_sec) +		    \
		(tv_end.tv_usec - tv_step.tv_usec)/1000000.0);	    \
	    } } while(0)

    struct timeval tv_end, tv_step;

    if (verbose>5) printtree();
    if (opt_level>=1) {
	if (verbose>2)
	    fprintf(stderr, "Starting optimise level %d\n", opt_level);

	tickstart();
	pointer_scan();
	tickend("Time for pointer scan");

	tickstart();
	quick_scan();
	tickend("Time for quick scan");

	if (opt_level>=2) {
	    calculate_stats();
	    if (verbose>5) printtree();

	    tickstart();
	    invariants_scan();
	    tickend("Time for invariant scan");

	    if (!noheader) {
		if (verbose>5) printtree();

		tickstart();
		trim_trailing_sets();
		tickend("Time for trailing scan");
	    }
	}

	if (verbose>2)
	    fprintf(stderr, "Optimise level %d complete\n", opt_level);
	if (verbose>5) printtree();
    }

    if (verbose)
	print_tree_stats();

    if (do_run) {
	if (do_codestyle == c_c) {
#ifdef USETCCLIB
	    run_ccode();
#else
	    fprintf(stderr, "Sorry cannot compile C code in memory, TCCLIB not linked\n");
	    exit(2);
#endif
#ifdef _BFI_JIT_H
	} else if (do_codestyle == c_jit) {
	    run_jit_asm();
#endif
	} else if (verbose>2 || enable_trace) {
	    if (verbose)
		fprintf(stderr, "Starting profiling interpreter\n");
	    run_tree();

	    if (verbose>2) {
		print_tree_stats();
		printtree();
	    }

	} else {
	    if (verbose)
		fprintf(stderr, "Starting array interpreter\n");
	    convert_tree_to_runarray();
	}
    } else {
	if (verbose)
	    fprintf(stderr, "Generating output code\n");

	switch(do_codestyle) {
	    case c_c:
		print_ccode(stdout);
		break;
#ifdef _BFI_NASM_H
	    case c_asm:
		print_asm();
		break;
#endif
#ifdef _BFI_BF_H
	    case c_bf:
		print_bf();
		break;
#endif

	    default:
		fprintf(stderr,
			"Output for code style %d not found, dumping tree.\n",
			do_codestyle);
		/*FALLTRHOUGH*/

	    case c_adder:
		print_adder();
		break;
	}
    }

#undef tickstart
#undef tickend
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
    int i, has_node_profile = 0;;
    calculate_stats();

    fprintf(stderr, "Total nodes %d, loaded %d", total_nodes, loaded_nodes);
    fprintf(stderr, " (%dk)", (int)(loaded_nodes * sizeof(struct bfi) +1023) / 1024);
    fprintf(stderr, ", Offset range %d..%d\n", min_pointer, max_pointer);
    if (profile_hits > 0)
	fprintf(stderr, "Run time %.4fs, cycles %.0f, %.3fns/cycle\n",
	    run_time, profile_hits, 1000000000.0*run_time/profile_hits);
    fprintf(stderr, "Tokens");
    for(i=0; i<TCOUNT; i++) {
	if (node_type_counts[i])
	    fprintf(stderr, ", %s=%d", tokennames[i], node_type_counts[i]);
	if (node_profile_counts[i])
	    has_node_profile++;
    }
    fprintf(stderr, "\n");
    if (has_node_profile) {
	fprintf(stderr, "Token profiles");
	for(i=0; i<TCOUNT; i++) {
	    if (node_profile_counts[i])
		fprintf(stderr, ", %s=%d", tokennames[i], node_profile_counts[i]);
	}
	fprintf(stderr, "\n");
    }
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
	node_profile_counts[n->type]++;
	if (enable_trace) {
	    int off = (p+n->offset) - oldp;
	    fprintf(stderr, "P(%d,%d)=", n->line, n->col);
	    printtreecell(stderr, -1, n);
	    if (n->type == T_MOV)
		fprintf(stderr, "\n");
	    else if (n->type != T_CALC) {
		if (off >= 0)
		    fprintf(stderr, "mem[%d]=%d\n", off, oldp[off]);
		else
		    fprintf(stderr, "mem[%d]= UNDERFLOW\n", off);
	    } else {
		if (n->offset == n->offset2 && n->count2 == 1) {
		    if (n->count3 != 0 && p[n->offset3] == 0) 
			fprintf(stderr, "mem[%d]=%d, BF Skip.\n",
			    (p+n->offset3)-oldp, p[n->offset3]);
		    else {
			fprintf(stderr, "mem[%d]=%d", off, oldp[off]);
			if (n->count3 != 0)
			    fprintf(stderr, ", mem[%d]=%d",
				(p+n->offset3)-oldp, p[n->offset3]);
			if (off < 0)
			    fprintf(stderr, " UNDERFLOW\n");
			else
			    fprintf(stderr, "\n");
		    }
		} else {
		    fprintf(stderr, "mem[%d]=%d", off, oldp[off]);
		    if (n->count2 != 0)
			fprintf(stderr, ", mem[%d]=%d",
			    (p+n->offset2)-oldp, p[n->offset2]);
		    if (n->count3 != 0)
			fprintf(stderr, ", mem[%d]=%d",
			    (p+n->offset3)-oldp, p[n->offset3]);
		    fprintf(stderr, "\n");
		}
	    }
	}
	switch(n->type)
	{
	    case T_MOV: p += n->count; break;
	    case T_ADD: p[n->offset] += n->count; break;
	    case T_SET: p[n->offset] = n->count; break;
	    case T_CALC:
		p[n->offset] = n->count
			    + n->count2 * p[n->offset2]
			    + n->count3 * p[n->offset3];
		break;

	    case T_MULT: case T_CMULT:
	    case T_IF: case T_FOR:

	    case T_WHL: if(M(p[n->offset]) == 0) n=n->jmp;
		break;

	    case T_END: if(M(p[n->offset]) != 0) n=n->jmp;
		break;

	    case T_ENDIF:
		break;

	    case T_PRT:
	    {
		int ch = M(n->count == -1?p[n->offset]:n->count);
		/*TODO: stop the clock ? */
		putch(ch);
		break;
	    }

	    case T_INP:
		gettimeofday(&tv_pause, 0); /* Stop the clock. */

		p[n->offset] = getch(p[n->offset]);

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
		/* Found a rail runner */
		if(M(p[n->offset]) == 0) { n=n->jmp; break; }
		while(M(p[n->offset]))
		    p += n->next->count;
		n=n->jmp;
		break;

	    case T_MFIND:
	    /* Search along a rail for a minus 1 */
		{
		    int stride = n->jmp->prev->count;
		    while(M(p[n->offset])) {
			p[n->offset] -= 1;
			p += stride;
			p[n->offset] += 1;
		    }
		}
		n=n->jmp;
		break;

	    case T_ADDWZ:
		/* Found a rail cleaner */
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
		if (verbose)
		    fprintf(stderr, "STOP Command executed.\n");
		goto break_break;

	    case T_NOP:
		break;

	    default:
		fprintf(stderr, "Execution error:\n"
		       "Bad node: type %d: ptr+%d, cnt=%d.\n",
			n->type, n->offset, n->count);
		exit(1);
	}
	if (enable_trace && n->type != T_PRT && n->type != T_ENDIF) {
	    int off = (p+n->offset) - oldp;
	    fprintf(stderr, "P(%d,%d):", n->line, n->col);
	    fprintf(stderr, "mem[%d]=%d\n", off, oldp[off]);
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
delete_tree(void)
{
    struct bfi * n = bfprog, *p;
    bfprog = 0;
    while(n) { p = n; n=n->next; free(p); }
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
	    if (verbose>5) {
		fprintf(stderr,  "\n prev ...\n");
		printtreecell(stderr, 2, n->prev);
	    }
	    fprintf(stderr,  "\n next ...\n");
	    printtreecell(stderr, 2, n->next);
	    if (n->next && verbose>5) {
		fprintf(stderr, "\n next->next ...\n");
		printtreecell(stderr, 4, n->next->next);
		if (n->next && n->next->jmp) {
		    fprintf(stderr, "\n next->jmp->prev ...\n");
		    printtreecell(stderr, 4, n->next->jmp->prev);
		    fprintf(stderr, "\n next->jmp ...\n");
		    printtreecell(stderr, 4, n->next->jmp);
		}
	    }
	    fprintf(stderr, "\n");
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

		if (n->count) {
		    /* Insert record at loop start */
		    n2 = n->next;
		    n2->offset += n->count;

		    n4 = add_node_after(n2);
		    n4->type = n->type;
		    n4->count = n->count;

		    /* Insert record at loop end */
		    n4 = add_node_after(n->next->jmp->prev);
		    n4->type = n->type;
		    n4->count = -n->count;
		}

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
	/* The loop classifier won't pick up [+] */
	if( n->type == T_WHL && n->next && n->next->next &&
	    n->next->type == T_ADD &&
	    n->next->next->type == T_END &&
	    ( n->next->count == 1 || n->next->count == -1 )
	    ) {
	    /* Change to T_SET */
	    n->next->type = T_SET; 
	    n->next->count = 0;

	    /* Delete the loop. */
	    n->type = T_NOP;
	    n->next->next->type = T_NOP;

	    if(verbose>4) {
		fprintf(stderr, "Replaced T_WHL [-/+] with T_SET.\n");
		printtreecell(stderr, 1, n->next);
		fprintf(stderr, "\n");
	    }
	}

	/* Looking for "[]" the Trivial infinite loop, replace it with
	 * conditional abort.
	 */
	if( n->type == T_WHL && n->next &&
	    n->next->type == T_END ) {

	    /* Insert a T_STOP */
	    n2 = add_node_after(n);
	    n2->type = T_STOP;

	    /* With the T_STOP there the 'loop' is now a T_IF */
	    n->type = T_IF;
	    n2->next->type = T_ENDIF;

	    /* Insert a T_SET for the const. */
	    n2 = add_node_after(n2->next);
	    n2->type = T_SET;
	    n2->offset = n->offset;
	    n2->count = 0;

	    if(verbose>4) {
		fprintf(stderr, "Inserted a T_STOP in a [].\n");
		printtreecell(stderr, 1, n->next);
		fprintf(stderr, "\n");
	    }
	}

	/* Clean up the T_NOPs */
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
	    if (node_changed) {
		n2 = n;
		n = n->next;
		build_string_in_tree(n2);
	    }
	    break;

	case T_SET:
	case T_ADD:
	case T_WHL:
	    node_changed = find_known_state(n);
	    break;

	case T_CALC:
	    node_changed = find_known_calc_state(n);
	    if (node_changed && n->prev)
		n = n->prev;
	    break;

	case T_END: case T_ENDIF:
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
    int min_pointer = 0, max_pointer = 0;
    int node_changed, i;
    while(n)
    {
	if (n->type == T_MOV) return;

	if (min_pointer > n->offset) min_pointer = n->offset;
	if (max_pointer < n->offset) max_pointer = n->offset;
	lastn = n;
	n=n->next;
    }
    if (max_pointer - min_pointer > 32) return;

    if (lastn) {
#if 0
	n = tcalloc(1, sizeof*n);
	n->inum = bfi_num++;
	lastn->next = n;
#endif
	lastn = add_node_after(lastn);
	n = add_node_after(lastn);

	for(i=min_pointer; i<= max_pointer; i++) {
	    n->type = T_SET;
	    n->count = -1;
	    n->offset = i;
	    node_changed = find_known_state(n);
	    if (node_changed && n->type != T_SET) {
		n->type = T_SET;
		n->count = 1;
		n->offset = i;
		node_changed = find_known_state(n);
	    }
	}
	lastn->next = 0;
	free(n);
	if (lastn->prev) lastn->prev->next = 0; else bfprog = 0;
	free(lastn);
    }
}

void
find_known_value(struct bfi * n, int v_offset, int allow_recursion,
		struct bfi * n_stop, 
		struct bfi ** n_found, 
		int * const_found_p, int * known_value_p, int * unsafe_p,
		int * hit_stop_node_p)
{
    /*
     * n		Node to start the search
     * v_offset		Which offset are we interested in
     * allow_recursion  If non-zero we can split on a loop.
     * n_stop		Node to stop the search
     *
     * n_found		Node we found IF it's safe to change/delete
     * const_found_p	True if we found a known value
     * known_value_p	The known value.
     * unsafe_p		True if this value is unsuitable for loop deletion.
     * hit_stop_node_p	True if we found n_stop.
     */
    int unmatched_ends = 0;
    int const_found = 0, known_value = 0, non_zero_unsafe = 0, hit_stop = 0;
    int n_used = 0;
    int distance = 0;

    if (opt_level < 3) allow_recursion = 0;

    if (hit_stop_node_p) *hit_stop_node_p = 0;

    if (verbose>5) {
	fprintf(stderr, "%d: Checking value for offset %d starting: ",
		SEARCHDEPTH-allow_recursion, v_offset);
	printtreecell(stderr, 0,n);
	fprintf(stderr, "\n");
    }
    while(n)
    {
	switch(n->type)
	{
	case T_NOP:
	    break;

	case T_SET:
	    if (n->offset == v_offset) {
		/* We don't know if this node will be hit */
		if (unmatched_ends) goto break_break;

		known_value = n->count;
		const_found = 1;
		goto break_break;
	    }
	    break;

	case T_CALC:
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
	    /*FALLTHROUGH*/

	case T_ENDIF:
#ifndef NO_RECURSION
	    // Check both n->jmp and n->prev
	    // If they are compatible we may still have a known value.
	    if (allow_recursion) {
		int known_value2 = 0, non_zero_unsafe2 = 0;

		if (verbose>5) fprintf(stderr, "%d: END: searching for double known\n", __LINE__);
		find_known_value(n->prev, v_offset, allow_recursion-1, n,
		    0, &const_found, &known_value2, &non_zero_unsafe2, &hit_stop);
		if (const_found) {
		    if (verbose>5) fprintf(stderr, "%d: Const found in loop.\n", __LINE__);
		    hit_stop = 0;
		    find_known_value(n->jmp->prev, v_offset, allow_recursion-1, n_stop,
			0, &const_found, &known_value, &non_zero_unsafe, &hit_stop);

		    if (hit_stop) {
			const_found = 1;
			known_value = known_value2;
			non_zero_unsafe = non_zero_unsafe2;
		    } else if (known_value != known_value2) {
			if (verbose>5) fprintf(stderr, "%d: Two know values found %d != %d\n", __LINE__, 
				known_value, known_value2);
			const_found = 0;
		    }
		} else if (hit_stop) {
		    if (verbose>5) fprintf(stderr, "%d: Nothing found in loop; continuing.\n", __LINE__);

		    n = n->jmp;
		    break;
		}
		else if (verbose>5) fprintf(stderr, "%d: Nothing found.\n", __LINE__);

		n = 0;
	    }
	    goto break_break;
#endif
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

#ifndef NO_RECURSION
	    // Check both n->jmp and n->prev
	    // If they are compatible we may still have a known value.
	    if (allow_recursion) {
		int known_value2 = 0, non_zero_unsafe2 = 0;

		if (verbose>5) fprintf(stderr, "%d: WHL: searching for double known\n", __LINE__);
		find_known_value(n->jmp->prev, v_offset, allow_recursion-1, n,
		    0, &const_found, &known_value2, &non_zero_unsafe2, &hit_stop);
		if (const_found) {
		    if (verbose>5) fprintf(stderr, "%d: Const found in loop.\n", __LINE__);
		    find_known_value(n->prev, v_offset, allow_recursion-1, 0,
			0, &const_found, &known_value, &non_zero_unsafe, 0);

		    if (known_value != known_value2) {
			if (verbose>5) fprintf(stderr, "%d: Two know values found %d != %d\n", __LINE__, 
				known_value, known_value2);
			const_found = 0;
		    }
		} else if (hit_stop) {
		    if (verbose>5) fprintf(stderr, "%d: Nothing found in loop; continuing.\n", __LINE__);

		    find_known_value(n->prev, v_offset, allow_recursion-1, 0,
			0, &const_found, &known_value, &non_zero_unsafe, 0);
		}
		else if (verbose>5) fprintf(stderr, "%d: Nothing found.\n", __LINE__);

		n = 0;
	    }
#endif

	case T_MOV:
	default:
	    goto break_break;
	}
	if (n->prevskip)
	    n=n->prevskip;
	else
	    n=n->prev;

	if (n_stop && n_stop == n) {
	    /* Hit the 'stop' node */
	    *hit_stop_node_p = 1;
	    n = 0;
	    goto break_break;
	}

	/* How did we get this far! */
	if (++distance > SEARCHRANGE) goto break_break;
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
	if (n==0 || n_used || unmatched_ends || distance > SEARCHRANGE/4*3)
	    *n_found = 0;
	else
	    *n_found = n;
    }

    if (verbose>5) {
	if (const_found)
	    fprintf(stderr, "%d: Known value is %d%s",
		    SEARCHDEPTH-allow_recursion, known_value,
		    non_zero_unsafe?" (unsafe zero)":"");
	else
	    fprintf(stderr, "%d: Unknown value for offset %d",
		    SEARCHDEPTH-allow_recursion, v_offset);
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

    find_known_value(v->prev, v->offset, SEARCHDEPTH, 0,
		&n, &const_found, &known_value, &non_zero_unsafe, 0);

    if (!const_found) {
	switch(v->type) {
	    case T_ADD:
		if (n && n->offset == v->offset &&
			(n->type == T_ADD || n->type == T_CALC) ) {
		    n->count += v->count;
		    v->type = T_NOP;
		    if (verbose>5) fprintf(stderr, "  Delete this node.\n");
		    return 1;
		}
		break;
	    case T_SET:
		if (n && 
			( (n->type == T_ADD && n->offset == v->offset) ||
			  (n->type == T_CALC && n->offset == v->offset)
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
	    case T_IF:
		{
		    /* Nested T_IF with same condition */
		    struct bfi *n2 = v;
		    while(n2->prev &&
			(n2->prev->type == T_ADD || n2->prev->type == T_SET || n2->prev->type == T_CALC) &&
			    n2->prev->offset != v->offset)
			n2 = n2->prev;
		    if (n2) n2 = n2->prev;
		    if (n2 && n2->offset == v->offset && ( n2->orgtype == T_WHL ) ) {
			v->type = T_NOP;
			v->jmp->type = T_NOP;
			return 1;
		    }
		}
		break;
	}

	return 0;
    }

    switch(v->type) {
	case T_ADD: /* Promote to T_SET. */
	    v->type = T_SET;
	    v->count += known_value;
	    if (cell_mask > 0)
		v->count = SM(v->count);
	    if (verbose>5) fprintf(stderr, "  Upgrade to T_SET.\n");
	    /*FALLTHROUGH*/

	case T_SET: /* Delete if same or unused */
	    if (known_value == v->count) {
		v->type = T_NOP;
		if (verbose>5) fprintf(stderr, "  Delete this T_SET.\n");
		return 1;
	    } else if (n && (n->type == T_SET || n->type == T_CALC)) {
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
	    known_value = UM(known_value);
	    if (known_value < 0) known_value &= 0xFF;
	    if (known_value == -1) break;
	    v->count = known_value;
	    if (verbose>5) fprintf(stderr, "  Make literal putchar.\n");
	    return 1;

	case T_ZFIND: case T_MFIND: case T_ADDWZ:
	case T_IF: case T_FOR: case T_MULT: case T_CMULT:
	case T_WHL: /* Change to (possible) constant loop or dead code. */
	    if (!non_zero_unsafe) {
		if (known_value == 0) {
		    v->type = T_DEAD;
		    if (verbose>5) fprintf(stderr, "  Change loop to T_DEAD (and delete).\n");
		    return 1;
		}

		/* A T_IF or a T_FOR with an index of 1 is a simple block */
		if (v->type == T_IF || 
		   (v->type == T_FOR && known_value == 1) ) {
		    v->type = T_NOP;
		    v->jmp->type = T_NOP;
		    if (verbose>5) fprintf(stderr, "  Change Loop to T_NOP.\n");
		    return 1;
		}
	    }
	    if (v->type == T_MULT || v->type == T_CMULT ||
		    v->type == T_FOR || v->type == T_WHL)
		return flatten_loop(v, known_value);
	    break;

	case T_END:
	    /* 
	     * If the value at the end of a loop is known the 'loop' may be a
	     * T_IF loop whatever it's contents.
	     *
	     * If the known_value is non-zero this is an infinite loop.
	     */
	    if (!non_zero_unsafe) {
		if (known_value == 0 && v->jmp->type != T_IF) {
		    struct bfi *n2;

		    if (verbose>5) fprintf(stderr, "  Change loop to T_IF.\n");
		    v->jmp->type = T_IF;
		    v->type = T_ENDIF;

		    /*
		     * Move the set out of the loop, if it won't move duplicate
		     * it to make searches easier as a T_ENDIF does NOT 
		     * guarantee it's cell is zero.
		     */
		    if (n && n->type == T_SET) {
			n->type = T_NOP;
			n2 = n; n = n->prev;
			if (n) n->next = n2->next; else bfprog = n2->next;
			if (n2->next) n2->next->prev = n;
			free(n2);
		    }

		    n2 = add_node_after(v);
		    n2->type = T_SET;
		    n2->count = 0;
		    n2->offset = v->offset;

#if 1
		    /* Also note the loop variable may have been saved as it
		     * was zeroed to restore after the end of the loop.
		     * We could check all though the loop and then make sure
		     * that it wasn't modified or used after. But the BF
		     * technique would be to put the save right at the end
		     * of the 'loop'.
		     */
		    n = v->prev;
		    if (n && n->type == T_CALC && n->count == 0 && 
			n->count2 != 0 && n->count3 == 0 && 
			n->offset2 == v->jmp->offset &&
			n->offset != n->offset2) {
			/* T_CALC: temp += loopidx*N */
			/* Also need to know that the temp will be zero before
			 * the loop so the calculation is okay for zero too. */

			int const_found2=0, known_value2=0, non_zero_unsafe2=0;

			find_known_value(v->jmp->prev, n->offset, SEARCHDEPTH, 0,
				    0, &const_found2, &known_value2,
				    &non_zero_unsafe2, 0);

			if (!const_found2 || non_zero_unsafe2 ||
			    known_value2 != 0)
			    return 1;

			/* Okay, move it out, before the T_SET */
			n2 = add_node_after(v);
			n2->type = n->type;
			n2->offset = n->offset;
			n2->offset2 = n->offset2;
			n2->count2 = n->count2;
			n->type = T_NOP;
			return 1;
		    }
#endif
#if 1
		    /* Of course it's unlikely that the programmer zero'd
		     * the temp first! */
		    if (n && n->type == T_CALC && n->count == 0 && 
			n->count2 == 1 && n->offset2 == n->offset &&
			n->count3 != 0 &&
			n->offset3 == v->jmp->offset &&
			n->offset != n->offset3) {
			/* T_CALC: temp += loopidx*N */
			/* Also need to know that the temp will be zero before
			 * the loop so the calculation is okay for zero too. */

			int const_found2=0, known_value2=0, non_zero_unsafe2=0;

			find_known_value(v->jmp->prev, n->offset, SEARCHDEPTH, 0,
				    0, &const_found2, &known_value2,
				    &non_zero_unsafe2, 0);

			if (!const_found2 || non_zero_unsafe2 ||
			    known_value2 != 0)
			    return 1;

			/* Okay, move it out, before the T_SET */
			n2 = add_node_after(v);
			n2->type = n->type;
			n2->offset = n->offset;
			n2->offset2 = n->offset3;
			n2->count2 = n->count3;
			n->type = T_NOP;
			return 1;
		    }
#endif
		    return 1;
		}
	    }
	    break;

	default:
	    break;
    }

    return 0;
}

int
search_for_update_of_offset(struct bfi *n, struct bfi *v, int n_offset)
{
    while(n!=v && n)
    {
	switch(n->type)
	{
	case T_IF: case T_FOR: case T_MULT: case T_CMULT: case T_WHL:

	case T_ADD: case T_SET: case T_CALC:
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
find_known_calc_state(struct bfi * v)
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
    if (v->type != T_CALC) return 0;

    if (v->count2 && v->count3 && v->offset2 == v->offset3) {
	/* Merge identical sides */
	v->count2 += v->count3;
	v->count3 = 0;
	rv = 1;
    }

    if (v->count3 != 0 && v->offset == v->offset3) {
	/* Swap them so += looks nice and code generation is safer. */
	/* Only offset2 can be the same as offset */
	int t;

	t = v->offset2;
	v->offset2 = v->offset3;
	v->offset3 = t;
	t = v->count2;
	v->count2 = v->count3;
	v->count3 = t;
	rv = 1;
    }

    if ((v->count2 == 0 || v->offset2 != v->offset) &&
        (v->count3 == 0 || v->offset3 != v->offset)) {
	find_known_value(v->prev, v->offset, SEARCHDEPTH, 0,
		    &n1, &const_found1, &known_value1, &non_zero_unsafe1, 0);
	if (n1 &&
	    (n1->type == T_ADD || n1->type == T_SET || n1->type == T_CALC)) {
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

    if (v->count2) {
	find_known_value(v->prev, v->offset2, SEARCHDEPTH, 0,
		    &n2, &const_found2, &known_value2, &non_zero_unsafe2, 0);
	n2_valid = 1;
    }

    if (v->count3) {
	find_known_value(v->prev, v->offset3, SEARCHDEPTH, 0, 
		    &n3, &const_found3, &known_value3, &non_zero_unsafe3, 0);
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

    /* Make sure offset2 is used first */
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

    if (n2 && n2->type == T_CALC && n2_valid &&
	    v->count2 == 1 && v->count3 == 0 && v->count == 0) {
	/* A direct assignment from n2 but not a constant */
	/* plus n2 seems safe to delete. */
	int is_ok = 1;

	/* Do we still have the values available that we used to make n2 ? */
	if (is_ok && n2->count2)
	    is_ok = search_for_update_of_offset(n2, v, n2->offset2);

	if (is_ok && n2->count3)
	    is_ok = search_for_update_of_offset(n2, v, n2->offset3);

	if (is_ok) {
	    /* Okay, move it to here, get rid of the double assign. */
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
		if (verbose>5) fprintf(stderr, "  Delete old T_SET.\n");
		if (n3 == n2) n3 = 0;
		n2 = 0;
	    }
	    rv = 1;
	    n2_valid = 0;
	}
    }

    if (n2 && n2->type == T_CALC && n2_valid &&
	    v->count2 == 1 && v->count3 == 0 && v->count == 0 &&
	    n2->next == v && v->next &&
	    v->next->type == T_SET && v->next->offset == n2->offset) {
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

#if 1
    if (n2 && n2->type == T_CALC && n2_valid && v->next && v->count2 != 0 &&
	    n2->count == 0 && n2->count3 == 0 && n2->next == v &&
	    v->next->offset == n2->offset &&
	    (v->next->type == T_SET ||
	      ( v->next->type == T_CALC &&
		v->next->count3 == 0 &&
		v->next->offset2 != n2->offset)
	    )) {
	/* A simple multiplication from n2 and it's the previous node and the next
	 * node wipes it. */
	/* This is a BF standard form. */

	v->offset2 = n2->offset2;
	v->count2 = v->count2 * n2->count2;

	n2->type = T_NOP;
	if (n2->prev) n2->prev->next = n2->next; else bfprog = n2->next;
	if (n2->next) n2->next->prev = n2->prev;
	free(n2);
	n2_valid = 0;
	return 1;
    }
#endif

    if (v->count2 == 0 && v->count3 == 0) {
	/* This is now a simple T_SET */
	v->type = T_SET;
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

/*
 * This function will remove very simple loops that have a constant loop
 * variable on entry.
 */
int
flatten_loop(struct bfi * v, int constant_count)
{
    struct bfi *n, *dec_node = 0;
    int is_znode = 0;
    int loop_step = 0;

    n = v->next;
    while(1)
    {
	if (n == 0) return 0;
	if (n->type == T_END || n->type == T_ENDIF) break;
	if (n->type != T_ADD && n->type != T_SET) return 0;
	if (n->offset == v->offset) {
	    if (dec_node) return 0; /* Two updates, hmmm */
	    if (n->type != T_ADD) {
		if (n->type != T_SET || n->count != 0) return 0;
		is_znode = 1;
	    }
	    else loop_step = n->count;
	    dec_node = n;
	}
	n=n->next;
    }
    if (dec_node == 0) return 0;

    if (!is_znode && loop_step != -1)
    {
	int sum, cnt;
	if (loop_step == 0) return 0; /* Infinite ! */

	/* Not a simple decrement hmmm. */
	/* This sort of sillyness is only used for 'wrapping' 8bits so ... */
	if (cell_size == 8) {

	    /* I could solve the modulo division, but for eight bits
	     * it's probably quicker just to trial it. */
	    sum = constant_count;
	    cnt = 0;
	    while(sum && cnt < 256) {
		cnt++;
		sum = ((sum + (loop_step & 0xFF)) & 0xFF);
	    }

	    if (cnt >= 256) return 0;

	    constant_count = cnt;
	    loop_step = -1;
	} else {
	    /* of course it might be exactly divisible ... */
	    if (loop_step >= 0 || constant_count <= 0) return 0;
	    if (constant_count % -loop_step != 0) return 0;
	    
	    constant_count /= -loop_step;
	    loop_step = -1;
	}
    }

    /* Found a loop with a known value at entry that contains only T_ADD and
     * T_SET where the loop variable is decremented nicely. GOOD! */

    if (is_znode) 
	constant_count = 1;

    if (verbose>5) fprintf(stderr, "  Loop replaced with T_NOP.\n");
    n = v->next;
    while(1)
    {
	if (n->type == T_END || n->type == T_ENDIF) break;
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
    int nested_loop_count = 0;

    if (opt_level < 3) return 0;
    if(verbose>5) {
	fprintf(stderr, "Classify loop: ");
	printtreecell(stderr, 0, v);
	fprintf(stderr, "\n");
    }

    if (!v || v->orgtype != T_WHL) return 0;
    typewas = v->type;
    n = v->next;
    while(n != v->jmp)
    {
	if (n == 0 || n->type == T_MOV) return 0;
	if (n->type != T_ADD && n->type != T_SET) {
	    complex_loop = 1;

	    /* If the loop count is modified inside a subloop this is a problem. */
	    if (n->orgtype == T_WHL) nested_loop_count++;
	    if (n->orgtype == T_END) nested_loop_count--;
	} else {
	    if (n->offset == v->offset) {
		if (dec_node) return 0; /* Two updates, nope! */
		if (nested_loop_count) {
		    if(verbose>5) fprintf(stderr, "Nested loop changes count\n");
		    return 0;
		}
		if (n->type != T_ADD || n->count != -1) {
		    if (n->type != T_SET || n->count != 0) return 0;
		    is_znode = 1;
		}
		dec_node = n;
	    } else {
		if (n->offset < v->offset) has_belowloop = 1;
		if (n->offset < 0) has_negoff = 1;
		if (n->type == T_ADD) has_add = 1;
		if (n->type == T_SET) has_equ = 1;
		is_loop_only = 0;
	    }
	}
	n=n->next;
    }
    if (dec_node == 0) return 0;

    if (complex_loop) {
	/* Complex contents but loop is simple so call it a for (or better) */
	if (v->type == T_WHL) {
	    if(verbose>5) fprintf(stderr, "Changed Complex loop to T_FOR\n");
	    v->type = T_FOR;
	}
	return (v->type != typewas);
    }

    /* Found a loop that contains only T_ADD and T_SET where the loop
     * variable is decremented nicely. This is a multiply loop. */

    /* The loop decrement isn't at the end; move it there. */
    if (dec_node != v->jmp->prev || is_znode) {
	struct bfi *n1, *n2;

	/* Snip it out of the loop */
	n1 = dec_node;
	n2 = n1->prev;
	n2->next = n1->next;
	n2 = n1->next;
	n2->prev = n1->prev;

	/* And put it back, but it this is an if (or unconditional) put it
	 * back AFTER the loop. */
	if (!is_znode) {
	    n2 = v->jmp->prev;
	    n1->next = n2->next;
	    n1->prev = n2;
	    n2->next = n1;
	    n1->next->prev = n1;
	} else {
	    n2 = v->jmp;
	    n1->next = n2->next;
	    n1->prev = n2;
	    n2->next = n1;
	    if(n1->next) n1->next->prev = n1;
	}
    }

    if (!has_add && !has_equ) {
	if (verbose>5) fprintf(stderr, "Loop flattened to single run\n");
	n->type = T_NOP;
	v->type = T_NOP;
	dec_node->type = T_SET;
	dec_node->count = 0;
    } else if (is_znode) {
	if (verbose>5) fprintf(stderr, "Loop flattened to T_IF\n");
	v->type = T_IF;
	v->jmp->type = T_ENDIF;
    } else {
	if (verbose>5) fprintf(stderr, "Loop flattened to multiply\n");
	v->type = T_CMULT;
	/* Without T_SET if all offsets >= Loop offset we don't need the if. */
	/* BUT: Any access below the loop can possibly be before the start
	 * of the tape if there's a T_MOV left anywhere in the program. */
	if (!has_equ) {
	    if (!has_belowloop || !hard_left_limit)
		v->type = T_MULT;
	    else if (!has_negoff && node_type_counts[T_MOV] == 0)
		v->type = T_MULT;
	}
    }
    return (v->type != typewas);
}

/*
 * This function will generate T_CALC tokens for simple multiplication loops.
 */
int
flatten_multiplier(struct bfi * v)
{
    struct bfi *n, *dec_node = 0;
    if (v->type != T_MULT && v->type != T_CMULT) return 0;

    if (opt_level<3) return 0;
#ifdef _BFI_BF_H
    if (do_codestyle == c_bf) return 0;
#endif

    n = v->next;
    while(n != v->jmp)
    {
	if (n->offset == v->offset)
	    dec_node = n;

	if (n->type == T_ADD) {
	    n->type = T_CALC;
	    n->count2 = 1;
	    n->offset2 = n->offset;
	    n->count3 = n->count;
	    n->offset3 = v->offset;
	    n->count = 0;
	}
	n = n->next;
    }
    
    if (v->type == T_MULT) {
	if (verbose>5) fprintf(stderr, "Multiplier flattened to single run\n");
	v->type = T_NOP;
	n->type = T_NOP;
    } else {
	if (verbose>5) fprintf(stderr, "Multiplier flattened to T_IF\n");
	v->type = T_IF;
	n->type = T_ENDIF;
	if (dec_node) {
	    /* Move this node out of the loop. */
	    struct bfi *n1, *n2;

	    /* Snip it out of the loop */
	    n1 = dec_node;
	    n2 = n1->prev;
	    n2->next = n1->next;
	    n2 = n1->next;
	    n2->prev = n1->prev;

	    /* And put it back AFTER the loop. */
	    n2 = n;
	    n1->next = n2->next;
	    n1->prev = n2;
	    n2->next = n1;
	    if(n1->next) n1->next->prev = n1;
	}
    }

    return 1;
}

/*
 * This moves literal T_PRT nodes back up the list to join to the previous
 * group of similar T_PRT nodes. An additional pointer (prevskip) is set 
 * so that they all point at the first in the growing 'string'.
 *
 * This allows the constant searching to disregard these nodes in one step.
 *
 * Because the nodes are all together it can also help the code generators 
 * build "printf" commands.
 */
void
build_string_in_tree(struct bfi * v)
{
    struct bfi * n = v->prev;
    while(n) {
	switch (n->type) {
	default: return; /* Nothing to attach to. */
	case T_MOV: case T_ADD: case T_CALC: case T_SET: /* Safe */
	    break;

	case T_PRT:
	    if (n->count == -1) return; /* Not a string */
	    if (verbose>5) {
		fprintf(stderr, "Found String at ");
		printtreecell(stderr, 0, n);
		fprintf(stderr, "\nAdding ");
		printtreecell(stderr, 0, v);
		fprintf(stderr, "\n");
	    }

	    v->prev->next = v->next;
	    if (v->next) v->next->prev = v->prev;
	    v->next = n->next;
	    v->prev = n;
	    if(v->next) v->next->prev = v;
	    v->prev->next = v;

	    /* Skipping past the whole string with prev */
	    if (n->prevskip)
		v->prevskip = n->prevskip;
	    else
		v->prevskip = n;
	    return;
	}
	n = n->prev;
    }
}

#ifdef USETCCLIB
int *
libbf_zfind32(int * m, int off) {
  while(*m) m += off;
  return m;
}

unsigned short *
libbf_zfind16(unsigned short * m, int off) {
  while(*m) m += off;
  return m;
}

void
run_ccode(void)
{
    char * ccode;
    size_t ccodelen;

    FILE * ofd;
    TCCState *s;
    int rv;
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
     *
     * So have the loaded C code use our getch and putch functions.
     */
    tcc_add_symbol(s, "getch", &getch);
    tcc_add_symbol(s, "putch", &putch);

    /* GCC is a lot faster at these */
    tcc_add_symbol(s, "libbf_zfind16", &libbf_zfind16);
    tcc_add_symbol(s, "libbf_zfind32", &libbf_zfind32);

#if defined(TCC0925) && !defined(TCCDONE)
#define TCCDONE
#error "Version specific"
    {
	int (*func)(void);
	int imagesize;
	void * image = 0;

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
    }
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

/*
 * This outputs the tree as a flat 'machine code' like language.
 * In theory this can easily be used directly as macro invocations by any
 * macro assembler.
 *
 * Currently it's just a clean tree dump.
 */
void 
print_adder(void)
{
    struct bfi * n = bfprog;

    while(n)
    {
	if (enable_trace)
	    printf("; %d,%3d\n", n->line, n->col);
	switch(n->type)
	{
	case T_MOV:
	    printf("ptradd\t%d\t; p += %d\n", n->count, n->count);
	    break;

	case T_ADD:
	    printf("add\t%d,%d\t; p[%d] += %d\n",
		    n->offset, n->count,
		    n->offset, n->count);
	    break;

	case T_SET:
	    printf("setci\t%d,%d\t; p[%d] = %d\n",
		    n->offset, n->count,
		    n->offset, n->count);
	    break;

	case T_CALC:
	    if (n->count == 0) {
		if (n->offset == n->offset2 && n->count2 == 1 && n->count3 == 1) {
		    printf("addc\t%d,%d\t; p[%d] += p[%d]\n",
			    n->offset, n->offset3,
			    n->offset, n->offset3);
		    break;
		}

		if (n->offset == n->offset2 && n->count2 == 1) {
		    printf("addmul\t%d,%d,%d\t; p[%d] += p[%d]*%d\n",
			    n->offset, n->offset3, n->count3,
			    n->offset, n->offset3, n->count3);
		    break;
		}

		if (n->count3 == 0) {
		    if (n->count2 == 1) {
			printf("setc\t%d,%d",
			    n->offset, n->offset2);
			printf("\t; p[%d] = p[%d]\n",
			    n->offset, n->offset2);
		    } else {
			printf("setmulc\t%d,%d,%d",
			    n->offset, n->offset2, n->count2);
			printf("\t; p[%d] = p[%d]*%d\n",
			    n->offset, n->offset2, n->count2);
		    }
		    break;
		}
	    }

	    if (n->offset == n->offset2 && n->count2 == 1) {
		printf("addmuli\t%d,%d,%d,%d\t; p[%d] += p[%d]*%d + %d\n",
			n->offset, n->offset3, n->count3, n->count,
			n->offset, n->offset3, n->count3, n->count);
		break;
	    }

	    if (n->count3 == 0) {
		printf("calc4\t%d,%d,%d,%d",
		    n->offset, n->count, n->offset2, n->count2);
		printf("\t; p[%d] = %d + p[%d]*%d\n",
		    n->offset, n->count, n->offset2, n->count2);
	    } else {
		printf("calc6\t%d,%d,%d,%d,%d,%d\n",
		    n->offset, n->count, n->offset2, n->count2, n->offset3, n->count3);
		printf("\t; p[%d] = %d + p[%d]*%d + p[%d]*%d\n",
		    n->offset, n->count, n->offset2, n->count2,
		    n->offset3, n->count3);
	    }
	    break;

	case T_PRT:
	    if (n->count > -1) {
		if (n->count >= ' ' && n->count <= '~' && n->count != '\'')
		    printf("putchar\t'%c'\n", n->count);
		else
		    printf("putchar\t%d\n", n->count);
	    } else
		printf("write\t%d\n", n->offset);
	    break;

	case T_INP:
	    printf("read\t%d\n", n->offset);
	    break;

	case T_IF:
	    printf("if\t%d,%d\t\n", n->offset, n->count);
	    break;

	case T_ENDIF:
	    printf("endif\t%d,%d\n", n->jmp->offset, n->jmp->count);
	    break;

	case T_MULT: case T_CMULT:
	case T_MFIND:
	case T_ZFIND: case T_ADDWZ: case T_FOR:
	case T_WHL:
	    printf("loop\t%d,%d\t; %s\n", n->offset, n->count,
		tokennames[n->type]);
	    break;

	case T_END:
	    printf("next\t%d,%d\n", n->jmp->offset, n->jmp->count);
	    break;

	case T_NOP: 
	    fprintf(stderr, "Warning on code generation: "
	           "NOP node: ptr+%d, cnt=%d, @(%d,%d).\n",
		    n->offset, n->count, n->line, n->col);
	    break;

	default:
	    printf("%s\t%d,%d,%d,%d,%d,%d\n",
		tokennames[n->type],
		    n->offset, n->count,
		    n->offset2, n->count2,
		    n->offset3, n->count3);
	    fprintf(stderr, "Error on code generation:\n"
	           "Bad node: type %d: ptr+%d, cnt=%d.\n",
		    n->type, n->offset, n->count);
	    break;
	}
	n=n->next;
    }
}

int
getch(int oldch)
{
    int c = EOF;
    /* The "standard" getwchar() is really dumb, it will refuse to read
     * characters from a stream if getchar() has touched it first.
     * It does this EVEN if this is an ASCII+Unicode machine.
     *
     * So I have to mess around with this...
     *
     * But it does give me somewhere to stick the EOF rubbish.
     */
    if (eofcell != 5)
	for(;;) {
	    if (iostyle == 1) {
		int rv;
		wchar_t wch = EOF;
		rv = scanf("%lc", &wch);
		if (rv == EOF)
		    c = EOF;
		else if (rv == 0)
		    continue;
		else
		    c = wch;
	    } else
		c = getchar();

	    if (c == '\r') continue;
	    break;
	}
    if (c != EOF) return c;
    switch(eofcell)
    {
    case 2: return -1;
    case 3: return 0;
    case 4: return EOF;
    default: return oldch;
    }
}

void
putch(int ch)
{
    if (ch > 127 && iostyle == 1)
	printf("%lc", ch);
    else
	putchar(ch);
}

void
set_cell_size(int cell_bits)
{
    cell_size = cell_bits;
    cell_mask = (1 << cell_size) - 1;
    if (cell_mask <= 0) cell_mask = -1;

    if (verbose>5) {
	fprintf(stderr, "Cell bits set to %d, mask = 0x%x\n", 
		cell_size, cell_mask);
    }

    if (cell_size < 7) {
	fprintf(stderr, "A minimum cell size of 7 bits is needed for ASCII.\n");
	exit(1);
    } else if (cell_size <= 8)
	cell_type = "unsigned char";
    else if (cell_size <= 16)
	cell_type = "unsigned short";
    else if ((size_t)cell_size > sizeof(int)*8) {
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
	/* Try half size */
	cell_array_alloc_len = MEMSIZE/2 + 2*MEMGUARD;
	mp = mmap(0, cell_array_alloc_len,
		    PROT_READ+PROT_WRITE,
		    MAP_PRIVATE+MAP_ANONYMOUS+MAP_NORESERVE, -1, 0);
    }

    if (mp == MAP_FAILED) {
	fprintf(stderr, "Cannot map memory for cell array\n");
	exit(1);
    }

    if (cell_array_alloc_len < MEMSIZE)
	fprintf(stderr, "Warning: Only able to map part of cell array, continuing.\n");

    cell_array_low_addr = mp;

#ifdef MADV_MERGEABLE
    if( madvise(mp, cell_array_alloc_len, MADV_MERGEABLE | MADV_SEQUENTIAL) )
	perror("madvise on tape");
#endif

    if (MEMGUARD > 0) {
	/* unmap the guard regions */
	munmap(mp, MEMGUARD); mp += MEMGUARD;
	munmap(mp+cell_array_alloc_len-2*MEMGUARD, MEMGUARD);
    }

    cell_array_pointer = mp;
    if (!hard_left_limit)
	cell_array_pointer += MEMSKIP;

    return cell_array_pointer;
}

void
sigsegv_report(int signo, siginfo_t * siginfo, void * ptr)
{
/*
 *  A segfault handler is likely to be called from inside library functions so
 *  I'll avoid calling the standard I/O routines.
 *
 *  But it's be pretty much safe once I've checked the failed access is
 *  within the tape range so I will call exit() rather than _exit() to 
 *  flush the standard I/O buffers in that case.
 */

#define estr(x) write(2, x, sizeof(x)-1)
    if(cell_array_pointer != 0) {
	if (siginfo->si_addr > cell_array_pointer - MEMGUARD &&
	    siginfo->si_addr <
		    cell_array_pointer + cell_array_alloc_len - MEMGUARD) {

	    if( siginfo->si_addr > cell_array_pointer )
		estr("Tape pointer has moved above available space\n");
	    else
		estr("Tape pointer has moved below available space\n");
	    exit(1);
	}
    }
    if(siginfo->si_addr < (void*)0 + 128*1024) {
	estr("Program fault: NULL Pointer dereference.\n");
    } else {
	estr("Segmentation violation, address not recognised.\n");
    }
    _exit(4);
}

void
trap_sigsegv(void)
{
    struct sigaction saSegf;

    saSegf.sa_sigaction = sigsegv_report;
    sigemptyset(&saSegf.sa_mask);
    saSegf.sa_flags = SA_SIGINFO|SA_NODEFER|SA_RESETHAND;

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
void run_progarray(int * progarray);

void
convert_tree_to_runarray(void)
{
    struct bfi * n = bfprog;
    int arraylen = 0;
    int * progarray = 0;
    int * p;
    int last_offset = 0;
#ifdef M
    if (sizeof(*m)*8 != cell_size && cell_size>0) {
	fprintf(stderr, "Sorry, the interpreter has been configured with a fixed cell size of %d bits\n", sizeof(icell)*8);
	exit(1);
    }
#else
#define M(x) UM(x)
#endif

    while(n)
    {
	switch(n->type)
	{
	case T_MOV:
	    break;

	case T_CALC:
	    arraylen += 7;
	    break;

	default:
	    arraylen += 3;
	    break;
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
	case T_INP:
	    break;

	case T_PRT: case T_ADD: case T_SET:
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
	    p[-1] = T_WHL;
	    /*FALLTHROUGH*/

	case T_WHL:
	    /* Storing the location of the instruction in the T_END's count
	     * field; it's not normally used */
	    n->jmp->count = p-progarray;
	    *p++ = 0;
	    break;

	case T_ENDIF:
	    progarray[n->count] = (p-progarray) - n->count -1;
	    break;

	case T_END:
	    progarray[n->count] = (p-progarray) - n->count;
	    *p++ = -progarray[n->count];
	    break;

	case T_CALC:
	    if (n->count3 == 0) {
	    /*  m[off] = m[off2]*count2 */
		p[-1] = T_CALC2;
		*p++ = n->count;
		*p++ = n->offset2 - last_offset;
		*p++ = n->count2;
	    } else if ( n->count == 0 && n->count2 == 1 && n->offset == n->offset2 ) {
	    /*  m[off] += m[off3]*count3 */
		p[-1] = T_CALC3;
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

	default:
	    fprintf(stderr, "Invalid node type found = %d\n", n->type);
	    exit(1);
	}
	n = n->next;
    }
    *p++ = 0;
    *p++ = T_STOP;

    delete_tree();
    run_progarray(progarray);
    free(progarray);
}

#ifdef __GNUC__
__attribute((optimize(3),hot,aligned(64)))
#endif
void
run_progarray(int * p)
{
    icell * m;
#ifndef USEHUGERAM
    void * freep;
#endif
#ifdef USEHUGERAM
    m = map_hugeram();
#else
    m = freep = tcalloc(MEMSIZE, sizeof*m);
#endif

    for(;;) {
	m += *p++;
	switch(*p)
	{
	case T_ADD: *m += p[1]; p += 2; break;
	case T_SET: *m = p[1]; p += 2; break;

	case T_END: if(M(*m) != 0) p += p[1];
	    p += 2;
	    break;

	case T_WHL:
	    if(M(*m) == 0) p += p[1];
	    p += 2;
	    break;

	case T_ENDIF:
	    p += 1;
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

	case T_CALC:
	    *m = p[1] + m[p[2]] * p[3] + m[p[4]] * p[5];
	    p += 6;
	    break;

	case T_CALC2:
	    *m = p[1] + m[p[2]] * p[3];
	    p += 4;
	    break;

	case T_CALC3:
	    *m += m[p[1]] * p[2];
	    p += 3;
	    break;

	case T_INP:
	    *m = getch(*m);
	    p += 1;
	    break;

	case T_PRT:
	    {
		int ch = M(p[1] == -1?*m:p[1]);
		putch(ch);
	    }
	    p += 2;
	    break;

	case T_STOP:
	    goto break_break;
	}
    }
break_break:;
#ifndef USEHUGERAM
    free(freep);
#endif
#undef icell
#undef M
}
