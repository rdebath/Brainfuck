#ifdef __STRICT_ANSI__
#ifndef DISABLE_TCCLIB
/* Required for open_memstream in glibc */
#define _GNU_SOURCE 1
#endif
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#if (_POSIX_VERSION >= 200809L)
#define ENABLE_MD5FUNC
#endif

#include "bfi.tree.h"
#include "bfi.run.h"
#include "clock.h"
#include "bfi.ccode.h"
#include "big_int.h"

#ifdef ENABLE_MD5FUNC
#include "md5.h"
#include "hashmem.h"
#endif

#ifndef DISABLE_TCCLIB
#include <libtcc.h>
#endif

#ifndef DISABLE_DLOPEN
#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#endif

typedef void (*void_func)(void);
static const char * putname = "putch";
static int use_putstr = 0;
static int indent = 0, is_simple_statement = 0;

static int fixed_mask = 0;              /* Optimised for specific cell size requiring a mask */
static int mask_defined = 0;
static char const * cell_type = "C";    /* Cell C variable type */
static int cell_type_iso = 0;           /* Using exact type from stdint.h */

#define okay_for_cstr(xc) ((xc) != 0 && (xc) <= '~')

static int use_direct_getchar = 0;
static int use_dynmem = 0;
static int use_goto = 0;
static int use_functions = -1;
static int use_md5dedup = -1;
static int disable_indent = 0;
static int libtcc_specials = 0;
static int knr_c_ok = 1;
static int tiny_tape = 0;
static int gen_getch = 0, gen_putch = 0;

#if defined(DISABLE_DLOPEN)
static const int use_dlopen = 0;
#else
static int use_dlopen = 0;
static int choose_runner = -1;
static char * cc_cmd = 0;
static int pic_opt = -1;
static int in_one = 0;
static int leave_temps = 0;
#endif

#ifndef DISABLE_TCCLIB
static void run_tccode(void);
#endif
#ifndef DISABLE_DLOPEN
static void run_gccode(void);
#endif

static void pt(FILE* ofd, int indent, struct bfi * n);
static char * rvalmsk(int offset);
static char * rval(int offset);
static char * lval(int offset);
static void set_cell_type(void);
static void print_c_header(FILE * ofd);
static void print_lib_funcs(FILE * ofd);

#ifdef ENABLE_MD5FUNC
static struct hashtab * hashtab[1] = {0};
#endif

int
checkarg_ccode(char * opt, char * arg UNUSED)
{
#if !defined(DISABLE_TCCLIB)
    if (!strcmp(opt, "-ltcc")) {
	choose_runner = 0;
	libtcc_specials = 1;
	return 1;
    }
#endif
#if !defined(DISABLE_DLOPEN)
    if (!strcmp(opt, "-ldl")) {
	choose_runner = 1;
	return 1;
    }
    if (!strcmp(opt, "-cc") && arg) {
	cc_cmd = strdup(arg);
	choose_runner = 1;
	return 2;
    }
    if (!strcmp(opt, "-fPIC")) { pic_opt = 2; return 1; }
    if (!strcmp(opt, "-fpic")) { pic_opt = 1; return 1; }
    if (!strcmp(opt, "-fno-pic")) { pic_opt = 0; return 1; }
    if (!strcmp(opt, "-fonecall")) { in_one = 1; return 1; }
    if (!strcmp(opt, "-fleave-temps")) { leave_temps = 1; return 1; }
#if defined(DISABLE_TCCLIB)
    if (!strcmp(opt, "-ltcc")) {
	cc_cmd = "tcc";
	choose_runner = 1;
	libtcc_specials = 1;
	in_one = 1;
	return 1;
    }
#endif
#endif
    if (!strcmp(opt, "-dynmem")) {
	use_dynmem = 1;
	if (opt_regen_mov == -1) opt_regen_mov = 0;
	return 1;
    }
    if (!strcmp(opt, "-fgoto")) { use_goto = 1; return 1; }
    if (!strcmp(opt, "-ffunct")) { use_functions = 1; use_md5dedup = 1; return 1; }
    if (!strcmp(opt, "-fno-funct")) { use_functions = 0; use_md5dedup = 0; return 1; }
    if (!strcmp(opt, "-fflat-funct")) { use_functions = 1; use_md5dedup = 0; return 1; }
    if (!strcmp(opt, "-fno-md5")) { use_md5dedup = 0; return 1; }
    if (!strcmp(opt, "-fno-indent")) { disable_indent = 1; return 1; }
    if (!strcmp(opt, "-fno-knr")) { knr_c_ok = 0; return 1; }
    return 0;
}

void
pt(FILE* ofd, int indent_to, struct bfi * n)
{
    int i, j;
    for(j=(n==0 || !enable_trace); j<2; j++) {
	if (disable_indent)
	    fprintf(ofd, "  ");
	else if (max_indent > 10) {
	    int k = indent_to;
	    if (max_indent < 64) k = 2*k+2; else k++;
	    if (k>128) k = 128;
	    for(i=0; i+7<k; ) {
		fprintf(ofd, "\t");
		i += 8;
	    }
	    for(; i<k; i++)
		fprintf(ofd, " ");
	} else if (indent_to == 0)
	    fprintf(ofd, "  ");
	else for(i=0; i<indent_to; i++)
	    fprintf(ofd, "\t");

	if (j == 0) {
	    fprintf(ofd, "t(%d,%d,\"", n->line, n->col);
	    printtreecell(ofd, -1, n);
	    fprintf(ofd, "\",m+ %d)\n", n->offset);
	}
    }
}

static
char *
bijective_hexavigesimal(unsigned int a)
{
    unsigned int c = 0, x = 1, i;
    static char buf[sizeof(int)*171/100+3];
    while (a >= x) {
	c++;
	a -= x;
	x *= 26;
    }

    for (i = 0; i < c; i++) {
	buf[c-i-1] = 'a' + (a % 26);
	a = a/26;
    }
    buf[c] = 0;

    if (buf[1]) buf[0] = buf[0] + ('A' - 'a');

    return buf;
}

char *
rvalmsk(int offset)
{
static char namebuf[64];

    if (mask_defined) {
	if (tiny_tape)
	    sprintf(namebuf, "M(%s)", bijective_hexavigesimal(offset+1));
	else if (offset)
	    sprintf(namebuf, "M(m[%d])", offset);
	else
	    strcpy(namebuf, "M(*m)");
    } else {
	if (tiny_tape)
	    sprintf(namebuf, "%s", bijective_hexavigesimal(offset+1));
	else if (offset)
	    sprintf(namebuf, "m[%d]", offset);
	else
	    strcpy(namebuf, "*m");
    }
    return namebuf;
}

char *
rval(int offset)
{
static char namebuf[64];
    if (tiny_tape)
	sprintf(namebuf, "%s", bijective_hexavigesimal(offset+1));
    else if (offset)
	sprintf(namebuf, "m[%d]", offset);
    else
	strcpy(namebuf, "*m");
    return namebuf;
}

char *
rval2(int offset)
{
static char namebuf[64];
    if (tiny_tape)
	sprintf(namebuf, "%s", bijective_hexavigesimal(offset+1));
    else if (offset)
	sprintf(namebuf, "m[%d]", offset);
    else
	strcpy(namebuf, "*m");
    return namebuf;
}

char *
lval(int offset)
{
static char namebuf[64];
    if (tiny_tape)
	sprintf(namebuf, "%s", bijective_hexavigesimal(offset+1));
    else if (offset)
	sprintf(namebuf, "m[%d]", offset);
    else
	strcpy(namebuf, "*m");
    return namebuf;
}

void
set_cell_type()
{
    /* First try for an, oversized, exact C cell type */
#if defined(UINT64_MAX)
    if (cell_length == (int)sizeof(uint64_t)*CHAR_BIT) {
	cell_type = "uint64_t";
	cell_type_iso = 1;
    } else
#endif
#if defined(UINTMAX_MAX)
    if (cell_length == (int)sizeof(uintmax_t)*CHAR_BIT) {
	cell_type = "uintmax_t";
	cell_type_iso = 1;
    } else
#endif
    /* Normal integer cell types */
    if (cell_length > 0) {
	if (cell_length <= CHAR_BIT)
	    cell_type = "unsigned char";
	else if (cell_length == (int)sizeof(short)*CHAR_BIT)
	    cell_type = "unsigned short";
	else if (cell_length <= (int)sizeof(int)*CHAR_BIT)
	    cell_type = "unsigned int";
    }

    if (cell_size > 0 &&
	cell_size != sizeof(int)*CHAR_BIT &&
	cell_size != sizeof(short)*CHAR_BIT &&
	cell_size != sizeof(char)*CHAR_BIT)
	fixed_mask = cell_mask;
    else
	fixed_mask = 0;
}

void
print_c_header(FILE * ofd)
{
    int memoffset = 0;
    int l_iostyle = iostyle;

    if (cell_mask > 0 && cell_size < 8 && l_iostyle == 1) l_iostyle = 0;
    use_putstr = 0;

    /* Hello world mode ? */
    if (!enable_trace && !do_run &&
	    total_nodes == node_type_counts[T_CHR] + node_type_counts[T_STR]) {
	int okay = 1;
	int ascii_only = 1;
	struct bfi * n = bfprog;
	/* Check the string; be careful. */
	while(n && okay) {
	    if (n->type != T_STR && (n->type != T_CHR || !okay_for_cstr(n->count)))
		okay = 0;

	    if (n->type == T_CHR && (n->count <= 0 || n->count > 127))
		ascii_only = 0;

	    n = n->next;
	}

	if (ascii_only) l_iostyle = 0;

	if (okay) {
	    fprintf(ofd, "#include <stdio.h>\n");
	    fprintf(ofd, "int main() {\n");
	    putname = "putchar";
	    return ;
	}
    }

    fprintf(ofd, "/* Code generated from %s */\n\n", bfname);
    fprintf(ofd, "#include <stdio.h>\n");

    if (libtcc_specials || use_functions || use_dynmem || !knr_c_ok)
    {
	if (knr_c_ok)
	    fprintf(ofd, "#ifdef __STDC__\n");
	fprintf(ofd, "#include <stdlib.h>\n");
	fprintf(ofd, "#include <string.h>\n");
	if (knr_c_ok) {
	    fprintf(ofd, "#else\n");
	    fprintf(ofd, "#define const\n");
	    fprintf(ofd, "#endif\n");
	}
    }
    fprintf(ofd, "\n");

    if (knr_c_ok && use_functions) {
	fprintf(ofd, "#ifdef __STDC__\n");
	fprintf(ofd, "#define FP(x) x\n");
	fprintf(ofd, "#define FD(x,y) x\n");

	fprintf(ofd, "#else\n");
	fprintf(ofd, "#define FP(x) ()\n");
	fprintf(ofd, "#define FD(x,y) y\n");
	fprintf(ofd, "#endif\n");
	fprintf(ofd, "\n");
    }

    if (cell_size == 0) {
	if (cell_type_iso)
	    fprintf(ofd, "#include <stdint.h>\n\n");
	else {
	    if (knr_c_ok)
		fprintf(ofd, "#ifdef __STDC__\n");

	    fprintf(ofd, "#include <limits.h>\n");
	    fprintf(ofd, "/* LLONG_MAX came in after inttypes.h, limits.h is very old. */\n");
	    fprintf(ofd, "/* We need stdlib.h to set _POSIX_VERSION if limits.h doesn't. */\n");
	    fprintf(ofd, "#if _POSIX_VERSION >= 199506L || defined(LLONG_MAX)\n");
	    fprintf(ofd, "#include <stdlib.h>\n");
	    fprintf(ofd, "#include <inttypes.h>\n");
	    fprintf(ofd, "#endif\n");

	    if (knr_c_ok)
		fprintf(ofd, "#endif\n\n");

	    fprintf(ofd, "#if 0 /*1978 K&R*/\n");
	    fprintf(ofd, "#elif defined(C) /*Okay*/\n");
	    if (cell_length==0 || cell_length>64) {
		fprintf(ofd, "#elif defined(__SIZEOF_INT128__)\n");
		fprintf(ofd, "#define C unsigned __int128\n");
		fprintf(ofd, "#elif defined(_UINT128_T)\n");
		fprintf(ofd, "#define C __uint128_t\n");
	    }
	    fprintf(ofd, "#elif defined(ULLONG_MAX) || defined(__LONG_LONG_MAX__)\n");
	    fprintf(ofd, "#define C unsigned long long\n");
	    fprintf(ofd, "#elif defined(UINTMAX_MAX)\n");
	    fprintf(ofd, "#define C uintmax_t\n");
	    fprintf(ofd, "#elif defined(ULONG_MAX) && LONG_MAX>INT_MAX\n");
	    fprintf(ofd, "#define C unsigned long\n");
	    fprintf(ofd, "#else\n");
	    fprintf(ofd, "#define C unsigned int\n");
	    fprintf(ofd, "#endif\n\n");

	    if (cell_length != INT_MAX) {
		mask_defined = 1;
		fprintf(ofd, "#ifndef M\n");
		if (cell_length > 0 && cell_length < UINTBIG_BIT) {
		    fprintf(ofd, "#define M(V) ((V)&(((C)2<<%d)-1))\n", cell_length-1);

		    if (knr_c_ok) fprintf(ofd, "#ifdef __STDC__\n");
		    if (cell_length > sizeof(int)*CHAR_BIT && cell_length < UINTBIG_BIT)
			fprintf(ofd, "enum { MaskTooBigForCell=1/(sizeof(C)>=%d) };\n",
			    (cell_length+CHAR_BIT-1)/CHAR_BIT);
		    if (knr_c_ok) fprintf(ofd, "#endif\n");
		} else
		    fprintf(ofd, "#define M(V) V\n");
		fprintf(ofd, "#endif\n\n");
	    }

	    if (knr_c_ok) fprintf(ofd, "#ifdef __STDC__\n");
	    if (cell_length <= 0) {
		fprintf(ofd, "enum { MaskTooSmall=1/(M(0x80)) };\n");
	    } else if (mask_defined) {
		fprintf(ofd, "enum { MaskTooSmall=1/(C)(M(0x%x)) };\n", INT_MIN);
	    } else {
		fprintf(ofd, "enum { CellTooSmall=1/(sizeof(C)>=sizeof(int)) };\n");
	    }

	    if (node_type_counts[T_DIV] != 0) {
		fprintf(ofd, "enum { CellTypeMustNotBeSigned = 1/(((%s)-1) > 0) };\n", cell_type);
	    }
	    if (knr_c_ok) fprintf(ofd, "#endif\n\n");
	}

    }

    if (fixed_mask>0) {
	fprintf(ofd, "#define M(V) ((V) & 0x%x)\n\n", fixed_mask);
	mask_defined = 1;
    }

    /* Add the debugging lib routines */
    if (enable_trace || node_type_counts[T_DUMP] != 0) {
	fprintf(ofd, "%s * imem;\n", cell_type);
	fprintf(ofd, "%s%s%s%s%s\n",
		    "static void prtnum(",cell_type," n) {"
	    "\n"    "    ",cell_type," max = 1, v = 0;"
	    "\n"    "    v = n;"
	    "\n"    "    for(;;) {"
	    "\n"    "        v /= 10;"
	    "\n"    "        if (v == 0) break;"
	    "\n"    "        max *= 10;"
	    "\n"    "    }"
	    "\n"    "    for(;;) {"
	    "\n"    "        v = n / max;"
	    "\n"    "        n = n % max;"
	    "\n"    "        fputc('0' + (int)v, stderr);"
	    "\n"    "        if (max == 1) break;"
	    "\n"    "        max /= 10;"
	    "\n"    "    }"
	    "\n"    "}"
	    "\n");

	if (enable_trace) {
	    fputs(  "#define t(p1,p2,p3,p4) trace(p1,p2,p3,p4) ;\n", ofd);

	    fprintf(ofd, "%s%s%s\n",
			"static void trace(int line, int col, "
			"char * desc, ",cell_type," * mp) {"
		"\n"    "    int i;"
		"\n"    "    fprintf(stderr, \"P(%d,%d)\", line, col);"
		"\n"    "    if (desc && *desc)"
		"\n"    "\tfprintf(stderr, \"=%s\", desc);"
		"\n"    "    else"
		"\n"    "\tfprintf(stderr, \" \");"
		"\n"    "    fprintf(stderr, \"mem[%d]=\", (int)(mp-imem));"
		"\n"    "    if (mp>=imem)"
		"\n"    "\tprtnum(*mp);"
		"\n"    "    else fprintf(stderr, \"?\");"
		"\n"    "    fprintf(stderr, \"\\n\");"
		"\n"    "}"
		"\n");
	}

	if (node_type_counts[T_DUMP] != 0) {

	    fprintf(ofd, "%s%s%s\n",
			"void t_dump(",cell_type," * mp, int line, int col)"
		"\n"	"{"
		"\n"	"    int i, doff, off = mp-imem;"
		"\n"	"    fflush(stdout);\t\t\t/* Keep in sequence if merged */"
		"\n"	"    fprintf(stderr, \"P(%d,%d):\", line, col);"
		"\n"	"    doff = off - 8;"
		"\n"	"    doff &= -4;"
		"\n"	"    if (doff < 0) doff = 0;"
		"\n"	"    fprintf(stderr, \"ptr=%d, mem[%d]= \", off, doff);"
		"\n"	"    for(i = 0; i < 16; i++) {");

	    fprintf(ofd, "%s%d%s\n", "\tif (doff+i>=",memsize,") break;");
	    fputs(
		"\n"	"\tfprintf(stderr, \"%s%s\","
		"\n"	"\t\ti ? \", \" : \"\","
		"\n"	"\t\tdoff+i == off ? \">\" : \"\");"
		"\n"	"\tprtnum(imem[doff+i]);"
		"\n"	"    }"
		"\n"	"    fprintf(stderr, \"\\n\");"
		"\n"	"}", ofd);

	}

	fputs("\n", ofd);
    }

    /* Interface/headers for T_PRT and T_INP */
    if (do_run) {
	if (use_dlopen) {
	    /* The structure defined in this chunk of code should be put into
	     * a header file for compiling into this program and converted into
	     * a string so it can be included into the generated code ...
	     * TODO: configure make to do this.
	     */
	    fprintf(ofd, "%s%s%s\n",
		"typedef int (*runfnp)(void);\n"
		"typedef int (*getfnp)(int ch);\n"
		"typedef void (*putfnp)(int ch);\n"
		"static int brainfuck(void);\n"
		"struct bfinit {\n"
		"  runfnp run; void *memptr; putfnp bf_putch; getfnp bf_getch;\n"
		"} bf_init = {brainfuck,0,0,0};\n"
		"#define mem ((", cell_type, "*)bf_init.memptr)\n"
		"#define putch (*bf_init.bf_putch)\n"
		"#define getch (*bf_init.bf_getch)"
		);
	} else {
	    fprintf(ofd, "%s%s%s\n",
		"extern void putch(int ch);\n"
		"extern int getch(int ch);\n"
		"extern ", cell_type, " mem[];\n"
		"#define brainfuck main");
	}
	use_putstr = 1;
	fprintf(ofd, "%s%s%s\n\n",
		"static void putstr(const char * s)"
	"\n"	"{"
	"\n"	"    while(*s) ",putname,"(*s++);"
	"\n"	"}"
	);
    }
    else
    {
	if (node_type_counts[T_INP] != 0 || node_type_counts[T_STR] != 0 ||
	    node_type_counts[T_PRT] != 0 || node_type_counts[T_CHR] != 0) {
	    if (l_iostyle == 1) {
		if (knr_c_ok) fprintf(ofd, "#if defined(__STDC__) && defined(__STDC_ISO_10646__)\n");
		fprintf(ofd, "#include <locale.h>\n");
		fprintf(ofd, "#include <wchar.h>\n");
		if (knr_c_ok) fprintf(ofd, "#endif\n");
		fprintf(ofd, "\n");
	    }
	}
    }

    /* Standalone routines for Input and Print */
    if (!do_run) {
	if (node_type_counts[T_INP] != 0 || node_type_counts[T_INPI] != 0)
	{
	    if (l_iostyle == 0 && eofcell == 4 && input_string == 0 &&
		    node_type_counts[T_INPI] == 0) {
		use_direct_getchar = 1;
	    } else {
		fprintf(ofd, "#ifndef EOFCELL\n");
		switch(eofcell) {
		default:
		case 1: fprintf(ofd, "#define EOFCELL 1\n"); break;
		case 2: fprintf(ofd, "#define EOFCELL -1\n"); break;
		case 3: fprintf(ofd, "#define EOFCELL 0\n"); break;
		case 4: fprintf(ofd, "#define EOFCELL EOF\n"); break;
		case 7: fprintf(ofd, "#define EOFCELL 2\n"); break;
		}
		fprintf(ofd, "#endif\n\n");
		if (knr_c_ok)
		    fprintf(ofd, "%s%s%s%s%s%s%s",
			"#ifdef __STDC__\n"
			 "static ",cell_type," getch(",cell_type," oldch);\n"
			 "#else\n"
			 "static ",cell_type," getch();\n"
			 "#endif\n\n");
		else
		    fprintf(ofd, "%s%s%s%s%s",
			"static ",cell_type," getch(",cell_type," oldch);\n");
		gen_getch = 1;
	    }
	}

	/* Anything special needed for putch? */
	if ((node_type_counts[T_CHR] != 0 || node_type_counts[T_STR] != 0 ||
		node_type_counts[T_PRT] != 0) && l_iostyle == 1) {
	    gen_putch = 1;

	    if (knr_c_ok)
		fputs(
		    "#if defined(__STDC__) && defined(__STDC_ISO_10646__)\n"
		    "static void putch(int ch);\n"
		    "#else\n"
		    "static void putch();\n"
		    "#endif\n"
		    "\n", ofd);
	    else
		fputs("static void putch(int ch);\n", ofd);

	} else
	    putname = "putchar";
    }

    /* Are we using T_PRTI ? */
    if (node_type_counts[T_PRTI] != 0) {
	fprintf(ofd, "%s%s%s%s%s%s%s\n",
	    "static void putint(",cell_type," ch)\n"
	    "{\n"
	    "    char buf[4+3*sizeof(",cell_type,")], *sp=buf;\n"
	    "    do {\n"
	    "        *sp++ = '0' + ch % 10;\n"
	    "    } while((ch /= 10) != 0);\n"
	    "    do {\n"
	    "        ",putname,"(*--sp);\n"
	    "    } while(sp>buf);\n"
	    "}\n"
	    );
    }

    if (node_type_counts[T_INPI] != 0)
    {
	fprintf(ofd, "%s%s%s%s%s",
			"static ",cell_type," getint()"
		"\n"	"{"
		"\n"	"    int f = 1, c;"
		"\n"	"    ",cell_type," v = 0;"
		"\n"	"    while(f) {"
		"\n"	"        f = 0;"
		"\n"	"        c = getch(0);"
		"\n"	"        if(c >= '0' && c <= '9') {"
		"\n"	"            v = v*10 + c - 48;"
		"\n"	"            f = 1;"
		"\n"	"        }"
		"\n"	"    }"
		"\n"	"    return v;"
		"\n"	"}\n\n");
    }

    /* Are we using T_STOP ? */
    if (node_type_counts[T_STOP] != 0)
	fputs("const char stopped[] = \"STOP Command executed.\\n\";\n\n", ofd);

    /* Choose the mem representation and start main() for standalone */
    if (!do_run) {
	if (node_type_counts[T_MOV] == 0) {
	    if (min_pointer < 0)
		memoffset = -min_pointer;
	} else if (hard_left_limit<0) {
	    memoffset = -most_neg_maad_loop;
	}

	if (node_type_counts[T_MOV] == 0 && memoffset == 0 &&
		    !enable_trace && node_type_counts[T_DUMP] == 0) {
	    if (verbose && use_dynmem)
		fprintf(stderr, "Ignoring -dynmem as all cells are static\n");

	    if (!use_functions) {
		int i;
		fprintf(ofd, "static %s", cell_type);
		for(i=0; i<max_pointer+1; i++) {
		    fprintf(ofd, "%s", i?",":"");
		    if ((i+1)%16 ==0)
			fprintf(ofd, "\n\t");
		    else
			fprintf(ofd, " ");
		    fprintf(ofd, "%s", bijective_hexavigesimal(i+1));
		}
		fprintf(ofd, ";\n");
		tiny_tape = 1;
	    } else
		fprintf(ofd, "static %s m[%d];\n", cell_type, max_pointer+1);
	    fprintf(ofd, "int main(){\n");
	    if (enable_trace)
		fprintf(ofd, "#define mem m\n");
	} else if (!use_dynmem) {
	    fprintf(ofd, "int main(){\n");
	    /* These minor variations may change the speed of the Counter test
	     * by 45% when compiled with GCC ... scheesh! */
#if 0
	    fprintf(ofd, "  %s mem[%d];\n", cell_type, memsize+memoffset);
	    fprintf(ofd, "  register %s * m = mem;\n", cell_type);
	    fprintf(ofd, "  memset(mem, 0, sizeof(mem));\n");
#endif
#if 1
	    fprintf(ofd, "static %s mem[%d];\n", cell_type, memsize+memoffset);
	    fprintf(ofd, "  register %s * m = mem;\n", cell_type);
#endif
#if 0
	    fprintf(ofd, "  %s * mem = calloc(sizeof(*mem),%d);\n", cell_type, memsize+memoffset);
	    fprintf(ofd, "  register %s * m = mem;\n", cell_type);
#endif
#if 0
	    fprintf(ofd, "  %s mem[%d] = {0};\n", cell_type, memsize+memoffset);
	    fprintf(ofd, "  register %s * m = mem;\n", cell_type);
#endif
	    if (memoffset > 0)
		fprintf(ofd, "  m += %d;\n", memoffset);
	} else {

	    printf("#define CELL %s\n", cell_type);
	    printf("#define MINOFF (%d)\n", min_safe_mem);
	    printf("#define MAXOFF (%d)\n", max_safe_mem);
	    printf("#define MINALLOC 16\n");

	    puts( "\n"	"CELL * mem = 0;"
		"\n"	"int memsize = 0;"
		"\n"	""
		"\n"	"static CELL *"
		"\n"	"alloc_ptr(CELL *p)"
		"\n"	"{"
		"\n"	"    int amt, memoff, i, off;"
		"\n"	"    if (p >= mem && p < mem+memsize) return p;"
		"\n"	""
		"\n"	"    memoff = p-mem; off = 0;"
		"\n"	"    if (memoff<0) off = -memoff;"
		"\n"	"    else if(memoff>=memsize) off = memoff-memsize;"
		"\n"	"    if (off < memsize/16) off = memsize/8;"
		"\n"	"    amt = (off / MINALLOC + 1) * MINALLOC;"
		"\n"	"    mem = realloc(mem, (memsize+amt)*sizeof(*mem));"
		"\n"	"    if (!mem) {"
		"\n"	"        fprintf(stderr, \"Out of memory\\n\");"
		"\n"	"        exit(1);"
		"\n"	"    }"
		"\n"	"    if (memoff<0) {"
		"\n"	"        memmove(mem+amt, mem, memsize*sizeof(*mem));"
		"\n"	"        for(i=0; i<amt; i++)"
		"\n"	"            mem[i] = 0;"
		"\n"	"        memoff += amt;"
		"\n"	"    } else {"
		"\n"	"        for(i=0; i<amt; i++)"
		"\n"	"            mem[memsize+i] = 0;"
		"\n"	"    }"
		"\n"	"    memsize += amt;"
		"\n"	"    return mem+memoff;"
		"\n"	"}"
		"\n"	""
		"\n"	"static inline CELL *"
		"\n"	"move_ptr(CELL *p, int off) {"
		"\n"	"    p += off;"
		"\n"	"    if (off>=0 && p+MAXOFF >= mem+memsize)"
		"\n"	"        p = alloc_ptr(p+MAXOFF)-MAXOFF;"
		"\n"	"    if (off<=0 && p+MINOFF <= mem)"
		"\n"	"        p = alloc_ptr(p+MINOFF)-MINOFF;"
		"\n"	"    return p;"
		"\n"	"}"
		"\n"	""
		"\n"	"int main(){"
		"\n"	"  register CELL * m = move_ptr(alloc_ptr(mem),0);" );
	}

	if (node_type_counts[T_INP] != 0 || node_type_counts[T_STR] != 0 ||
	    node_type_counts[T_PRT] != 0 || node_type_counts[T_CHR] != 0) {
	    if (l_iostyle == 1) {
		if (knr_c_ok)
		    fprintf(ofd, "#if defined(__STDC__) && defined(__STDC_ISO_10646__)\n");
		fprintf(ofd, "  setlocale(LC_ALL, \"\");\n");
		if (knr_c_ok)
		    fprintf(ofd, "#endif\n");
	    }
	}

	fprintf(ofd, "  setbuf(stdout, 0);\n");
    }

    /* Start main() for runing now. */
    if (do_run) {
	fprintf(ofd, "%s%s%s",
	    "int brainfuck(void){\n"
	    "  register ", cell_type, " * m = mem;\n");
    }

    if (enable_trace || node_type_counts[T_DUMP] != 0) {
	fprintf(ofd, "  imem = m;\n");
    }

    if (!use_functions)
	fprintf(ofd, "\n");
}

void
print_lib_funcs(FILE * ofd)
{
    if (gen_putch)
    {
	if (knr_c_ok)
	    fputs(
		"\n"	"#ifdef __STDC__"
		"\n"	"static void putch(int ch)"
		"\n"	"#else"
		"\n"	"static void putch(ch) int ch;"
		"\n"	"#endif"
		"\n"	"#if defined(__STDC__) && defined(__STDC_ISO_10646__)"
		"\n"	"{"
		"\n"	"    if(ch>127)"
		"\n"	"\tprintf(\"%lc\",ch);"
		"\n"	"    else"
		"\n"	"\tputchar(ch);"
		"\n"	"}"
		"\n"	"#else"
		"\n"	"{"
		"\n"	"    putchar(ch);"
		"\n"	"}"
		"\n"	"#endif"
		"\n", ofd);
	else
	    fputs(
		"\n"	"static void putch(int ch)"
		"\n"	"{"
		"\n"	"#ifdef __STDC_ISO_10646__"
		"\n"	"    if(ch>127)"
		"\n"	"\tprintf(\"%lc\",ch);"
		"\n"	"    else"
		"\n"	"#endif"
		"\n"	"\tputchar(ch);"
		"\n"	"}"
		"\n", ofd);
    }

    if (gen_getch)
    {
	fprintf(ofd, "\n");
	if (knr_c_ok)
	{
	    fprintf(ofd, "#ifdef __STDC__\n");
	    fprintf(ofd, "static %s\n", cell_type);
	    fprintf(ofd, "getch(%s oldch)\n", cell_type);
	    fprintf(ofd, "#else\n");
	    fprintf(ofd, "static int getch(oldch) %s oldch;\n", cell_type);
	    fprintf(ofd, "#endif\n");
	} else
	    fprintf(ofd, "static %s\ngetch(%s oldch)\n", cell_type, cell_type);

	fprintf(ofd, "{\n");
	fprintf(ofd, "  int ch;\n");
	if (input_string && !do_run) {
	    char * ip = input_string;
	    fprintf(ofd, "  static char input_string[] = \"");
	    for(; *ip; ip++) {
		if (*ip >= ' ' && *ip <= '~' && *ip != '\\')
		    fputc(*ip, ofd);
		else if (*ip == '\n')
		    fprintf(ofd, "\\n");
		else
		    fprintf(ofd, "\\%03o", (unsigned char)*ip);
	    }
	    fprintf(ofd, "\", *ip = input_string;\n");
	    fprintf(ofd, "  if (*ip) { int ch = *ip++; putchar(ch); return ch; }\n");
	}
	if (iostyle == 0) {
	    fprintf(ofd, "  ch = getchar();\n");
	} else {
	    fprintf(ofd, "  do {\n");
	    if (iostyle == 1) {
		if (knr_c_ok)
		    fprintf(ofd, "#if defined(__STDC__) && defined(__STDC_ISO_10646__)\n");
		else
		    fprintf(ofd, "#ifdef __STDC_ISO_10646__\n");

		fprintf(ofd, "\tch = getwchar();\n");
		fprintf(ofd, "#else\n");
		fprintf(ofd, "\tch = getchar();\n");
		fprintf(ofd, "#endif\n");
	    } else
		fprintf(ofd, "\tch = getchar();\n");
	    fprintf(ofd, "  } while (ch == '\\r');\n");
	}

	fprintf(ofd, "  if (ch != EOF) return ch;\n");
	fprintf(ofd, "  if (EOFCELL == 2) {\n");
	fprintf(ofd, "    fprintf(stderr, \"^D\\n\");\n");
	fprintf(ofd, "    exit(0);\n");
	fprintf(ofd, "  }\n");
	fprintf(ofd, "  clearerr(stdin);\n");
	fprintf(ofd, "  return (EOFCELL<=0)?EOFCELL:oldch;\n");
	fprintf(ofd, "}\n");
    }
}

static void
print_c_body(FILE* ofd, struct bfi * n, struct bfi * e)
{
    while(n != e)
    {
	if (n->orgtype == T_END) indent--;
	switch(n->type)
	{
	case T_MOV:
	    if (!is_simple_statement) pt(ofd, indent,n);
	    if (!do_run && use_dynmem && n->count>0)
		fprintf(ofd, "m = move_ptr(m,%d);\n", n->count);
	    else if (n->count == 1)
		fprintf(ofd, "++m;\n");
	    else if (n->count == -1)
		fprintf(ofd, "--m;\n");
	    else if (n->count == INT_MIN)
		fprintf(ofd, "m -= 0x%x;\n", -n->count);
	    else if (n->count < 0)
		fprintf(ofd, "m -= %d;\n", -n->count);
	    else if (n->count > 0)
		fprintf(ofd, "m += %d;\n", n->count);
	    else
		fprintf(ofd, "; /* m += 0; */\n");
	    break;

	case T_ADD:
	    if (!is_simple_statement) pt(ofd, indent,n);

	    if (n->count == 1)
		fprintf(ofd, "++%s;\n", lval(n->offset));
	    else if (n->count == -1)
		fprintf(ofd, "--%s;\n", lval(n->offset));
	    else if (n->count == INT_MIN)
		fprintf(ofd, "%s -= 0x%x;\n", lval(n->offset), -n->count);
	    else if (n->count < 0)
		fprintf(ofd, "%s -= %d;\n", lval(n->offset), -n->count);
	    else if (n->count > 0)
		fprintf(ofd, "%s += %d;\n", lval(n->offset), n->count);
	    else
		fprintf(ofd, "; /* %s += 0; */\n", lval(n->offset));

	    if (enable_trace) {
		pt(ofd, indent,0);
		fprintf(ofd, "t(%d,%d,\"\",m+ %d)\n", n->line, n->col, n->offset);
	    }
	    break;

	case T_SET:
	    if (!is_simple_statement) pt(ofd, indent,n);
	    if (cell_size <= 0 && (n->count < -128 || n->count >= 256)) {
		fprintf(ofd, "%s = (%s) %d;\n",
		    lval(n->offset), cell_type, n->count);
	    } else
	    if (n->count == INT_MIN)
		fprintf(ofd, "%s = 0x%x;\n", lval(n->offset), n->count);
	    else
		fprintf(ofd, "%s = %d;\n", lval(n->offset), n->count);
	    if (enable_trace) {
		pt(ofd, indent,0);
		fprintf(ofd, "t(%d,%d,\"\",m+ %d)\n", n->line, n->col, n->offset);
	    }
	    break;

	case T_CALC:
	    pt(ofd, indent,n);
	    do {
		if (n->count == 0) {
		    if (n->offset == n->offset2 && n->count2 == 1)
		    {
			if (n->count3 == 1) {
			    fprintf(ofd, "%s += %s;\n",
				    lval(n->offset), rval(n->offset3));
			    break;
			}

			if (n->count3 == -1) {
			    fprintf(ofd, "%s -= %s;\n",
				    rval(n->offset), lval(n->offset3));
			    break;
			}

			if (n->count3 != 0) {
			    fprintf(ofd, "%s += %s*%d;\n",
				    rval(n->offset), lval(n->offset3), n->count3);
			    break;
			}
		    }

		    if (n->count3 == 0 && n->count2 != 0) {
			if (n->count2 == 1) {
			    fprintf(ofd, "%s = %s;\n",
				lval(n->offset), rval(n->offset2));
			} else if (n->count2 == -1) {
			    fprintf(ofd, "%s = -%s;\n",
				lval(n->offset), rval(n->offset2));
			} else {
			    fprintf(ofd, "%s = %s*%d;\n",
				lval(n->offset), rval(n->offset2), n->count2);
			}
			break;
		    }

		    if (n->count3 == 1 && n->count2 != 0) {
			fprintf(ofd, "%s = %s*%d + %s;\n",
			    lval(n->offset), rval2(n->offset2), n->count2, rval(n->offset3));
			break;
		    }
		}

		if (n->offset == n->offset2 && n->count2 == 1) {
		    if (n->count3 == 1) {
			fprintf(ofd, "%s += %s + %d;\n",
				lval(n->offset), rval(n->offset3), n->count);
			break;
		    }
		    fprintf(ofd, "%s += %s*%d + %d;\n",
			    lval(n->offset), rval(n->offset3), n->count3, n->count);
		    break;
		}

		if (n->count3 == 0) {
		    if (n->count2 == 1) {
			fprintf(ofd, "%s = %s + %d;\n",
			    lval(n->offset), rval(n->offset2), n->count);
			break;
		    }
		    if (n->count2 == -1) {
			fprintf(ofd, "%s = -%s + %d;\n",
			    lval(n->offset), rval(n->offset2), n->count);
			break;
		    }

		    fprintf(ofd, "%s = %d + %s*%d;\n",
			lval(n->offset), n->count, rval(n->offset2), n->count2);
		} else {
		    fprintf(ofd, "%s = %d + %s*%d + %s*%d; /*T_CALC*/\n",
			lval(n->offset), n->count, rval(n->offset2), n->count2,
			rval2(n->offset3), n->count3);
		}
	    } while(0);

	    if (enable_trace) {
		pt(ofd, indent,0);
		fprintf(ofd, "t(%d,%d,\"\",m+ %d)\n", n->line, n->col, n->offset);
	    }
	    break;

	case T_CALCMULT:
	    pt(ofd, indent,n);

	    if (n->count == 0 && n->count2 == 1 && n->count3 == 1)
		fprintf(ofd, "%s = %s * %s;\n",
		    lval(n->offset), rval(n->offset2), rval2(n->offset3));
	    else
		fprintf(ofd, "%s = %d + %s*%d * %s*%d; /* WARNING */\n",
		    lval(n->offset), n->count, rval(n->offset2), n->count2,
		    rval2(n->offset3), n->count3);

	    if (enable_trace) {
		pt(ofd, indent,0);
		fprintf(ofd, "t(%d,%d,\"\",m+ %d)\n", n->line, n->col, n->offset);
	    }
	    break;

	case T_LT:
	    pt(ofd, indent,n);

	    if (n->count == 0 && n->count2 == 1 && n->count3 == 1)
		fprintf(ofd, "%s += (%s < %s);\n",
		    lval(n->offset), rval(n->offset2), rval2(n->offset3));
	    else {
		fprintf(stderr, "Code gen error: "
			"%s\t"
			"%d:%d, %d:%d, %d:%d\n",
			tokennames[n->type],
			n->offset, n->count,
			n->offset2, n->count2,
			n->offset3, n->count3);
		exit(1);
	    }

	    if (enable_trace) {
		pt(ofd, indent,0);
		fprintf(ofd, "t(%d,%d,\"\",m+ %d)\n", n->line, n->col, n->offset);
	    }
	    break;

	case T_DIV:
	    if (mask_defined) {
		pt(ofd, indent,0);
		fprintf(ofd, "%s = %s;\n", lval(n->offset+1), rvalmsk(n->offset+1) );
	    }
	    pt(ofd, indent,n);
	    fprintf(ofd, "if(%s != 0) {\n", rval(n->offset+1) );
	    pt(ofd, indent+1,0);
	    fprintf(ofd, "%s = %s / %s;\n", lval(n->offset+3), rvalmsk(n->offset), rval2(n->offset+1));
	    pt(ofd, indent+1,0);
	    fprintf(ofd, "%s = %s %% %s;\n", lval(n->offset+2), rvalmsk(n->offset), rval2(n->offset+1));
	    pt(ofd, indent,0);
	    fprintf(ofd, "} else {\n");
	    pt(ofd, indent+1,0);
	    fprintf(ofd, "%s = 0;\n", lval(n->offset+3));
	    pt(ofd, indent+1,0);
	    fprintf(ofd, "%s = %s;\n", lval(n->offset+2), rval(n->offset));
	    pt(ofd, indent,0);
	    fprintf(ofd, "}\n");

	    if (enable_trace) {
		pt(ofd, indent,0);
		fprintf(ofd, "t(%d,%d,\"\",m+ %d)\n", n->line, n->col, n->offset);
	    }
	    break;

	case T_PRT:
	    if (!use_functions) {
		pt(ofd, indent,n);
		fprintf(ofd, "%s(%s);\n", putname, rvalmsk(n->offset));
	    } else {
		int pcount = 1;
		while (n->next && n->next->type == T_PRT && n->offset == n->next->offset) {
		    n = n->next;
		    pcount++;
		}
		if (pcount == 1) {
		    pt(ofd, indent,n);
		    fprintf(ofd, "%s(%s);\n", putname, rvalmsk(n->offset));
		} else if (pcount < 3) {
		    int cn;
		    for(cn=0;cn<pcount;cn++) {
			pt(ofd, indent, cn?0:n);
			fprintf(ofd, "%s(%s);\n", putname, rvalmsk(n->offset));
		    }
		} else {
		    pt(ofd, indent, n);
		    fprintf(ofd, "{\n");
		    pt(ofd, indent+1, 0);
		    fprintf(ofd, "int cn, ch = %s;\n", rvalmsk(n->offset));
		    pt(ofd, indent+1, 0);
		    fprintf(ofd, "for(cn=0;cn<%d;cn++)\n", pcount);
		    pt(ofd, indent+2, 0);
		    fprintf(ofd, "%s(ch);\n", putname);
		    pt(ofd, indent, 0);
		    fprintf(ofd, "}\n");
		}
	    }
	    break;

	case T_PRTI:
	    pt(ofd, indent,n);
	    fprintf(ofd, "putint(%s);\n", rvalmsk(n->offset));
	    break;

	case T_CHR:
	    pt(ofd, indent,n);
	    if (n->count == '\n')
		fprintf(ofd, "%s('\\n');\n", putname);
	    else
		fprintf(ofd, "%s(%d);\n", putname, n->count);
	    break;

	case T_STR:
	    {
		int j = 0;
		for(; j<n->str->length; j++) {
		    char s[256], *p = s;
		    int got_perc = 0;
		    int lastc = 0;

		    for(; j<n->str->length; j++) {
			int ch = n->str->buf[j] & 0xFF;
			int nch = 0;
			if (j+1 < n->str->length)
			    nch = n->str->buf[j+1] & 0xFF;
			lastc = ch;

			if (ch == '%') got_perc = 1;
			if (ch == '\n') { *p++ = '\\'; *p++ = 'n'; } else
			if (ch == '\r') { *p++ = '\\'; *p++ = 'r'; } else
			if (ch == '\a') { *p++ = '\\'; *p++ = 'a'; } else
			if (ch == '\b') { *p++ = '\\'; *p++ = 'b'; } else
			if (ch == '\t') { *p++ = '\\'; *p++ = 't'; } else
			if (ch == '\\') { *p++ = '\\'; *p++ = '\\'; } else
			if (ch == '"') { *p++ = '\\'; *p++ = '"'; } else
			if (ch < ' ') {
			    int c = ch & 0xFF;
			    *p++ = '\\';
			    *p++ = '0' + ((c>>6) & 0x3);
			    *p++ = '0' + ((c>>3) & 0x7);
			    *p++ = '0' + ((c   ) & 0x7);
			} else
			    *p++ = (char) /*GCC -Wconversion*/ ch;

			if ((p>s+7 && ch == '\n' && nch != '\n') ||
				p > s+sizeof(s)-16)
			    break;
		    }
		    *p = 0;

		    pt(ofd, indent,n);
		    if ((p == s+1 && *s != '\'') || (p==s+2 && lastc == '\n')) {
			fprintf(ofd, "%s('%s');\n", putname, s);
		    } else if (use_putstr) {
			fprintf(ofd, "putstr(\"%s\");\n", s);
		    } else if (lastc == '\n') {
			*--p = 0; *--p = 0;
			fprintf(ofd, "puts(\"%s\");\n", s);
		    } else if (!got_perc)
			fprintf(ofd, "printf(\"%s\");\n", s);
		    else
			fprintf(ofd, "printf(\"%%s\", \"%s\");\n", s);
		}
	    }
	    break;

	case T_INP:
	    pt(ofd, indent,n);
	    if (use_direct_getchar)
		fprintf(ofd, "%s = getchar();\n", lval(n->offset));
	    else
		fprintf(ofd, "%s = getch(%s);\n", lval(n->offset), rval(n->offset));

	    if (enable_trace) {
		pt(ofd, indent,0);
		fprintf(ofd, "t(%d,%d,\"\",m+ %d)\n", n->line, n->col, n->offset);
	    }
	    break;

	case T_INPI:
	    pt(ofd, indent,n);
	    fprintf(ofd, "%s = getint();\n", lval(n->offset));

	    if (enable_trace) {
		pt(ofd, indent,0);
		fprintf(ofd, "t(%d,%d,\"\",m+ %d)\n", n->line, n->col, n->offset);
	    }
	    break;


	case T_IF:
	    pt(ofd, indent,n);
#ifdef TRY_BRANCHLESS
	    if (n->next->next && n->next->next->jmp == n &&
		    !enable_trace && !mask_defined) {
		if (n->next->type == T_SET) {
		    fprintf(ofd, "m[%d] -= (m[%d] - %d) * !!m[%d];\n",
			n->next->offset, n->next->offset, n->next->count, n->offset);
		    n=n->jmp;
		    break;
		}
		if (n->next->type == T_CALC &&
		    n->next->count == 0 &&
		    n->next->count2 == 1 &&
		    n->next->count3 == 0
		    ) {
		    fprintf(ofd, "m[%d] -= (m[%d] - m[%d]) * !!m[%d];\n",
			n->next->offset, n->next->offset, n->next->offset2, n->offset);
		    n=n->jmp;
		    break;
		}
	    }
#endif

	    if (n->next->next && n->next->next->jmp == n && !enable_trace &&
		(   n->next->type == T_SET || n->next->type == T_ADD ||
		    n->next->type == T_STOP)) {
		fprintf(ofd, "if(%s) ", rvalmsk(n->offset));
		is_simple_statement = 1;
	    } else if (!use_goto) {
		fprintf(ofd, "if(%s) ", rvalmsk(n->offset));
		fprintf(ofd, "{\n");
	    } else {
		fprintf(ofd, "if(%s == 0) ", rvalmsk(n->offset));
		fprintf(ofd, "goto E%d;\n", n->count);
	    }
	    break;

	case T_WHL:
	{
	    int found_rail_runner = 0;

	    if (n->next->type == T_MOV &&
		n->next->next && n->next->next->jmp == n) {
		found_rail_runner = 1;
	    }

	    if (found_rail_runner && !do_run && use_dynmem && n->next->count>0) {
		pt(ofd, indent,n);
		fprintf(ofd, "while(%s) m += %d;\n",
			rvalmsk(n->offset), n->next->count);
		pt(ofd, indent,n);
		fprintf(ofd, "m = move_ptr(m,0);\n");
		n=n->jmp;
		break;
	    }

#if !defined(DISABLE_TCCLIB) && defined(__i386__)
	    /* TCCLIB generates a slow while() with two jumps in the loop,
	     * and several memory accesses. This is the assembler we would
	     * actually prefer.
	     *
	     * These prints are really ugly; I need a 'print gas in C'
	     * function if we have much more.
	     */
	    if (found_rail_runner && cell_size == 32 && libtcc_specials) {
		fprintf(ofd, "#if !defined(__i386__) || !defined(__TINYC__)\n");
		pt(ofd, indent,n);
                if (n->next->count == 1) {
                    fprintf(ofd, "while(%s) ++m;\n", rval(n->offset));
                } else if (n->next->count == -1) {
                    fprintf(ofd, "while(%s) --m;\n", rval(n->offset));
                } else if (n->next->count < 0) {
                    fprintf(ofd, "while(%s) m -= %d;\n",
                        rval(n->offset), -n->next->count);
                } else {
                    fprintf(ofd, "while(%s) m += %d;\n",
                        rval(n->offset), n->next->count);
                }
		fprintf(ofd, "#else /* Use i386 assembler */\n");
		pt(ofd, indent,n);
		fprintf(ofd, "{ /* Rail runner */\n");
		fprintf(ofd, "__asm__ __volatile__ (\n");
		fprintf(ofd, "\"mov %d(%%%%ecx),%%%%eax\\n\\t\"\n", 4 * n->offset);
		fprintf(ofd, "\"test %%%%eax,%%%%eax\\n\\t\"\n");
		fprintf(ofd, "\"je 1f\\n\\t\"\n");
		fprintf(ofd, "\"2: add $%d,%%%%ecx\\n\\t\"\n", 4* n->next->count);
		fprintf(ofd, "\"mov %d(%%%%ecx),%%%%eax\\n\\t\"\n", 4 * n->offset);
		fprintf(ofd, "\"test %%%%eax,%%%%eax\\n\\t\"\n");
		fprintf(ofd, "\"jne 2b\\n\\t\"\n");
		fprintf(ofd, "\"1:\\n\\t\"\n");
		fprintf(ofd, ": \"=c\" (m)\n");
		fprintf(ofd, ": \"0\"  (m)\n");
		fprintf(ofd, ": \"eax\"\n");
		fprintf(ofd, ");\n");
		pt(ofd, indent,n);
		fprintf(ofd, "}\n");
		fprintf(ofd, "#endif\n");
		n=n->jmp;
		break;
	    }
#endif

	    /* TCCLIB generates a slow 'strlen', libc is better, but the
	     * function call overhead is horrible.
	     */
	    if (found_rail_runner && cell_size == CHAR_BIT && libtcc_specials &&
		fixed_mask == 0 && n->next->count == 1) {
		pt(ofd, indent,n);
		fprintf(ofd, "if(%s) {\n", rval(n->offset));
		pt(ofd, indent+1,n);
		fprintf(ofd, "m++;\n");
		pt(ofd, indent+1,n);
		fprintf(ofd, "if(%s) {\n", rval(n->offset));
		pt(ofd, indent+2,n);
		fprintf(ofd, "m++;\n");
		pt(ofd, indent+2,n);
		if (n->offset)
		    fprintf(ofd, "m += strlen(m + %d);\n", n->offset);
		else
		    fprintf(ofd, "m += strlen(m);\n");
		pt(ofd, indent+1,n);
		fprintf(ofd, "}\n");
		pt(ofd, indent,n);
		fprintf(ofd, "}\n");
		n=n->jmp;
		break;
	    }
	}
	/*FALLTHROUGH*/

	case T_CMULT:
	case T_MULT:
	    if (!use_goto) {
		pt(ofd, indent,n);
		fprintf(ofd, "while(%s) ", rvalmsk(n->offset));
		if (n->next->next && n->next->next->jmp == n &&
		    n->next->type == T_MOV && !enable_trace)
		    is_simple_statement = 1;
		else
		    fprintf(ofd, "{\n");
	    } else {
		pt(ofd, indent,n);
		fprintf(ofd, "if(%s == 0) ", rvalmsk(n->offset));
		fprintf(ofd, "goto E%d;\n", n->count);
		pt(ofd, indent,0);
		fprintf(ofd, "L%d:;\n", n->count);
	    }
	    break;

	case T_END:
	case T_ENDIF:
	    if (is_simple_statement) {
		is_simple_statement = 0;
		break;
	    }
	    if (!use_goto) {
		pt(ofd, indent,n);
		fprintf(ofd, "}\n");
	    } else {
		if (n->type == T_END) {
		    pt(ofd, indent,n);
		    fprintf(ofd, "if(%s != 0) ", rvalmsk(n->jmp->offset));
		    fprintf(ofd, "goto L%d;\n", n->jmp->count);
		    pt(ofd, indent,0);
		} else
		    pt(ofd, indent,n);
		fprintf(ofd, "E%d:;\n", n->jmp->count);
	    }
	    break;

	case T_STOP:
	    if (!is_simple_statement) pt(ofd, indent,n);
	    if (use_functions)
		fprintf(ofd, "exit((fprintf(stderr, stopped),1));\n");
	    else
		fprintf(ofd, "return fprintf(stderr, stopped),1;\n");
	    break;

	case T_DUMP:
	    pt(ofd, indent,n);
	    fprintf(ofd, "t_dump(m+%d,%d,%d);\n", n->offset, n->line, n->col);
	    break;

	case T_NOP:
	    fprintf(stderr, "Warning on code generation: "
		   "%s node: ptr+%d, cnt=%d, @(%d,%d).\n",
		    tokennames[n->type],
		    n->offset, n->count, n->line, n->col);
	    fprintf(ofd, ";\n");
	    break;

	case T_CALL:
	    pt(ofd, indent,n);
	    if (n->jmp->type == T_ENDIF)
		fprintf(ofd, "if(%s) m = bf%d(m);\n", rvalmsk(n->offset), n->count);
	    else
		fprintf(ofd, "m = bf%d(m);\n", n->count);
	    n=n->jmp;
	    break;

	default:
            fprintf(stderr, "Code gen error: "
                    "%s\t"
                    "%d:%d, %d:%d, %d:%d\n",
                    tokennames[n->type],
                    n->offset, n->count,
                    n->offset2, n->count2,
                    n->offset3, n->count3);
            exit(1);
	}
	if (n->orgtype == T_WHL) indent++;
	n=n->next;
    }
}

void
print_ccode(FILE * ofd)
{
    struct bfi * n;
    int ipos = 0;

    set_cell_type();

    if (do_run || cell_type_iso || use_dynmem || enable_trace ||
	    node_type_counts[T_DUMP] != 0)
	knr_c_ok = 0;

    {
	int i = node_type_counts[T_CHR] + node_type_counts[T_STR];
	if (use_functions<0 && opt_level<=0)
	    use_functions = 0;
	if (use_functions<0 && total_nodes == i)
	    use_functions = 0;
#if defined(__GNUC__) && __GNUC__ < 3
	if (use_functions<0)
	    use_functions = (total_nodes-i >= 1000);
#else
	if (use_functions<0)
	    use_functions = (total_nodes-i >= 5000);
#endif
    }

    if (use_md5dedup<0) use_md5dedup = use_functions;

    if (verbose)
	fprintf(stderr, "Generating C Code.\n");

    if (use_dynmem && (do_run || enable_trace || node_type_counts[T_DUMP] != 0)) {
	fprintf(stderr, "Note: Option -dynmem ignored in run/debug/trace modes.\n");
	use_dynmem = 0;
    }

    indent = 0;
    is_simple_statement = 0;

    if (!use_functions)
    {
	if (!noheader)
	    print_c_header(ofd);
	else
	    fprintf(ofd, "%s bf(register %s * m)\n{\n", cell_type, cell_type);

	print_c_body(ofd, bfprog, (struct bfi *)0);

	if (!noheader) {
	    fprintf(ofd, "  return 0;\n}\n");
	    print_lib_funcs(ofd);
	} else
	    fprintf(ofd, "  return m;\n}\n");
	return;
    }

    if (!noheader) {
	print_c_header(ofd);

	fprintf(ofd, "  {\n");
	if (!knr_c_ok)
	    fprintf(ofd, "    extern void bf(register %s*);\n", cell_type);
	else
	    fprintf(ofd, "    extern void bf FP((register %s*));\n", cell_type);
	fprintf(ofd, "    bf(m);\n");
	fprintf(ofd, "  }\n");
	fprintf(ofd, "  return 0;\n}\n\n");
    }

    for(n=bfprog; n; n=n->next) {
	n->iprof = ++ipos;

	if (n->orgtype == T_END) indent--;

	if (((n->type == T_END && n->jmp->type == T_WHL) ||
	    ( n->type == T_ENDIF && n->jmp->type == T_IF)) &&
		n->iprof-n->jmp->iprof > 5) {
	    int ti = indent;

#ifndef ENABLE_MD5FUNC
	    indent = 0;

	    if (!knr_c_ok)
		fprintf(ofd, "static %s * bf%d(register %s * m)\n{\n",
		    cell_type, n->jmp->count, cell_type);
	    else
		fprintf(ofd, "static %s * bf%d FD((register %s * m),(m) register %s * m;)\n{\n",
		    cell_type, n->jmp->count, cell_type, cell_type);
	    if (n->type == T_ENDIF)
		print_c_body(ofd, n->jmp->next, n);
	    else
		print_c_body(ofd, n->jmp, n->next);
	    fprintf(ofd, "  return m;\n}\n\n");
#else
	    char * fn_code;
	    size_t fn_len;
	    FILE * codefd = open_memstream(&fn_code, &fn_len);
	    int is_dup = 0;

	    indent = 0;

	    if (n->type == T_ENDIF)
		print_c_body(codefd, n->jmp->next, n);
	    else
		print_c_body(codefd, n->jmp, n->next);
	    putc('\0', codefd); /* Make it easy */
	    fclose(codefd);

	    if (use_md5dedup)
	    {
		int i;
		int * vnum;
		char hashid[64];
		MD5_CTX mdContext;

		MD5Init (&mdContext);
		MD5Update (&mdContext, fn_code, fn_len-1);
		MD5Final (&mdContext);

		for (i = 0; i < 16; i++)
		    sprintf(hashid+i*2, "%02x", mdContext.digest[i]);

		vnum = find_entry(hashtab, hashid);
		if (vnum) {
		    is_dup = 1;
		    n->jmp->count = *vnum;
		} else {
		    vnum = malloc(sizeof*vnum);
		    *vnum = n->jmp->count;
		    set_entry(hashtab, hashid, vnum);
		}
	    }

	    if (!is_dup)
	    {
		if (!knr_c_ok)
		    fprintf(ofd, "static %s * bf%d(register %s * m)\n{\n",
			cell_type, n->jmp->count, cell_type);
		else
		    fprintf(ofd, "static %s * bf%d FD((register %s * m),(m) register %s * m;)\n{\n",
			cell_type, n->jmp->count, cell_type, cell_type);
		fprintf(ofd, "%s", fn_code);
		fprintf(ofd, "  return m;\n}\n\n");
	    }

	    free(fn_code);
#endif
	    indent = ti;

	    n->jmp->type = T_CALL;
	}

	if (n->orgtype == T_WHL) indent++;
    }

    if (!knr_c_ok)
	fprintf(ofd, "void bf(register %s * m)\n{\n", cell_type);
    else
	fprintf(ofd, "void bf FD((register %s * m),(m) register %s * m;)\n{\n", cell_type, cell_type);
    print_c_body(ofd, bfprog, (struct bfi *)0);
    fprintf(ofd, "}\n");
    if (!noheader)
	print_lib_funcs(ofd);
    return;
}

#if !defined(DISABLE_TCCLIB) || !defined(DISABLE_DLOPEN)
void
run_ccode(void)
{
#if defined(DISABLE_TCCLIB)
    use_dlopen = 1;
    run_gccode();
#elif defined(DISABLE_DLOPEN)
    run_tccode();
#else
    if (choose_runner >= 0)
	use_dlopen = choose_runner;
    else
#ifdef __TINYC__
	use_dlopen = 0;
#else
	use_dlopen = ((total_nodes < 4000) || (opt_level > 1) || (
	    cell_length > 0 &&
	    cell_length != sizeof(long)*CHAR_BIT &&
	    cell_length != sizeof(int)*CHAR_BIT &&
	    cell_length != sizeof(short)*CHAR_BIT &&
	    cell_length != sizeof(char)*CHAR_BIT));
#endif
    if (use_dlopen)
	run_gccode();
    else
	run_tccode();
#endif
}
#endif

#ifndef DISABLE_TCCLIB

static void
run_tccode(void)
{
    char * ccode;
    size_t ccodelen;

    FILE * ofd;
    TCCState *s;
    int rv;
    void * memp;

    libtcc_specials = 1;

    ofd = open_memstream(&ccode, &ccodelen);
    if (ofd == NULL) { perror("open_memstream"); exit(7); }
    print_ccode(ofd);
    putc('\0', ofd);
    fclose(ofd);

    memp = map_hugeram();

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

#if !defined(__STRICT_ANSI__) && defined(TCC_RELOCATE_AUTO)
    tcc_add_symbol(s, "getch", &getch);
    tcc_add_symbol(s, "putch", &putch);

    {
	int (*func)(void);
	int imagesize;
	void * image = 0;

	if (verbose)
	    fprintf(stderr, "Running C Code using libtcc\n");

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

	/*
	 * The ugly casting is forced by the C99 standard as a (void*) is not a
	 * valid cast for a function pointer.
	 *
	*(void **) (&func) = tcc_get_symbol(s, "main");
	 */
	func = tcc_get_symbol(s, "main");

	if (!func) {
	    fprintf(stderr, "Could not find compiled code entry point\n");
	    exit(1);
	}
	tcc_delete(s);
	free(ccode);

	start_runclock();
	func();
	finish_runclock(&run_time, &io_time);
	free(image);
    }

#else /* __STRICT_ANSI__ */

    /* If we're compiled under __STRICT_ANSI__ or version 9.26 of libtcc the
     * above code doesn't compile. This is a, rather nasty, fallback. */
    {
	static char arg0_tcclib[] = "tcclib";
	static char * args[] = {arg0_tcclib, 0};
	/*
	    Hmm, I wanted to do the above without named initialisers ...
	    so it looks like this ... but without the const problem.

	    static char * args[] = {"tcclib", 0};
	 */
	void * iso_workaround;

	/* The ugly casting is forced by the C99 standard as a (void*) is not a
	 * valid cast for a function pointer.
	 */
	*(void_func*) &iso_workaround  = (void_func) &getch;
	tcc_add_symbol(s, "getch", iso_workaround);
	*(void_func*) &iso_workaround  = (void_func) &putch;
	tcc_add_symbol(s, "putch", iso_workaround);

	if (verbose)
	    fprintf(stderr, "Running C Code using libtcc tcc_run() to link & run.\n");

	rv = tcc_run(s, 1, args);
	if (verbose && rv)
	    fprintf(stderr, "tcc_run returned %d\n", rv);
	tcc_delete(s);
	free(ccode);
    }
#endif
}
#endif


#ifndef DISABLE_DLOPEN
#define BFBASE "bfpgm"

#ifndef RTLD_LOCAL
#define RTLD_LOCAL 0	/* Very old versions don't have this. */
#endif

static void compile_and_run(void);

static char tmpdir[] = "/tmp/bfrun.XXXXXX";
static char ccode_name[sizeof(tmpdir)+16];
static char dl_name[sizeof(tmpdir)+16];
static char obj_name[sizeof(tmpdir)+16];

static void
run_gccode(void)
{
    FILE * ofd;
#if _POSIX_VERSION >= 200809L
    if( mkdtemp(tmpdir) == 0 ) {
	perror("mkdtemp()");
	exit(1);
    }
#else
#warning mkdtemp not used _POSIX_VERSION is too low, using mktemp instead
    if (mkdir(mktemp(tmpdir), 0700) < 0) {
	perror("mkdir(mktemp()");
	exit(1);
    }
#endif
    strcpy(ccode_name, tmpdir); strcat(ccode_name, "/"BFBASE".c");
    strcpy(dl_name, tmpdir); strcat(dl_name, "/"BFBASE".so");
    strcpy(obj_name, tmpdir); strcat(obj_name, "/"BFBASE".o");
    ofd = fopen(ccode_name, "w");
    print_ccode(ofd);
    fclose(ofd);
    compile_and_run();
}

/*  Needs:   cc -shared -fpic -o libfoo.so foo.c
 *  And:     dlopen() (in -ldl on linux)
 *
 *  The above command does both a compile and a link in one, these can be
 *  broken into two independent calls of the C compiler if wanted. This
 *  is useful because the combined command is not acceptable to ccache.
 *
 *  The -fpic and -fPIC options are not REQUIRED for this to work on i686;
 *  they instruct the compiler to create mostly position independent code
 *  that references global tables to find the locations of the targets
 *  of linkages between different shared objects. If the option is not
 *  used more expensive relocations will be needed to load the file,
 *  but it will not fail to load.
 *
 *  For x64 the -fpic/PIC option is required at compile time.
 *
 *  For some machines the implementation of -fPIC is more expensive
 *  than the implementation of -fpic. BUT sometimes a program cannot be
 *  compiled with -fpic because of table size limitations. For x86 they
 *  are identical.
 */

/* If we're 32 bit on a 64bit or vs.versa. we need an extra option */
#ifndef CC
#if defined(__clang__) && (__clang_major__>=3) && defined(__i386__)
#define CC "clang -m32 -fwrapv"
#elif defined(__clang__) && (__clang_major__>=3) && defined(__amd64__)
#define CC "clang -m64 -fwrapv"
#elif defined(__PCC__)
#define CC "pcc"
#elif defined(__GNUC__) && ((__GNUC__>4) || (__GNUC__==4 && __GNUC_MINOR__>=1))
#if defined(__x86_64__)
#if defined(__ILP32__)
#define CC "gcc -mx32 -fwrapv"
#else
#define CC "gcc -m64 -fwrapv"
#endif
#elif defined(__i386__)
#define CC "gcc -m32 -fwrapv"
#else
#define CC "gcc -fwrapv"
#endif
#elif defined(__GNUC__) && (__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ > 3)
#define CC "gcc -fwrapv"
#elif defined(__GNUC__)
#define CC "gcc"
#elif defined(__TINYC__)
#define CC "tcc"
#endif
#ifdef CC
#define DEFAULT_PIC 1
#else
#define CC "cc"
#endif
#endif

typedef int (*runfnp)(void);
typedef int (*getfnp)(int ch);
typedef void (*putfnp)(int ch);

static int loaddll(const char *);
static runfnp runfunc;
static void *handle;

static void
compile_and_run(void)
{
    char cmdbuf[256];
    int ret;
    const char * cc = CC;
    const char * copt = "";
    const char * pic_cmd = "";
    if (opt_level >= 2) copt = opt_level==2 ? " -O2" : " -O3";

    if (cc_cmd) cc = cc_cmd;

#ifdef DEFAULT_PIC
    if (pic_opt<0) pic_opt = DEFAULT_PIC;
#endif
    switch(pic_opt) {
    case 0: pic_cmd = ""; break;
    case 1: pic_cmd = " -fpic"; break;
    case 2: pic_cmd = " -fPIC"; break;
    }

    if (in_one) {
	if (verbose)
	    fprintf(stderr,
		"Running C Code using \"%s%s%s -shared\" and dlopen().\n",
		cc,pic_cmd,copt);

	sprintf(cmdbuf, "%s%s%s -shared -o %s %s",
		    cc, pic_cmd, copt, dl_name, ccode_name);
	ret = system(cmdbuf);
    } else {
	if (verbose)
	    fprintf(stderr,
		"Running C Code using \"%s%s%s\", link -shared and dlopen().\n",
		cc,pic_cmd,copt);

	/* Like this so that ccache has a good path and distinct compile. */
	sprintf(cmdbuf, "cd %s; %s%s%s -c -o %s %s",
		tmpdir, cc, pic_cmd, copt, BFBASE".o", BFBASE".c");
	ret = system(cmdbuf);

	if (ret != -1) {
	    sprintf(cmdbuf, "cd %s; %s%s -shared -o %s %s",
		    tmpdir, cc, pic_cmd, dl_name, BFBASE".o");
	    ret = system(cmdbuf);
	}
    }

    if (ret == -1) {
	perror("Calling C compiler failed");
	exit(1);
    }
    if (WIFEXITED(ret)) {
	if (WEXITSTATUS(ret)) {
	    fprintf(stderr, "Compile failed.\n");
	    exit(WEXITSTATUS(ret));
	}
    } else {
	if (WIFSIGNALED(ret)) {
	    if( WTERMSIG(ret) != SIGINT && WTERMSIG(ret) != SIGQUIT)
		fprintf(stderr, "Killed by SIGNAL %d.\n", WTERMSIG(ret));
	    exit(1);
	}
	perror("Abnormal exit");
	exit(1);
    }

    loaddll(dl_name);

    if (!leave_temps) {
	unlink(ccode_name);
	unlink(dl_name);
	unlink(obj_name);
	rmdir(tmpdir);
    }

#ifndef __STRICT_ANSI__
    if (verbose>1)
	fprintf(stderr, "Calling function loaded at address %p\n", (void*) runfunc);
#endif

    start_runclock();
    (*runfunc)();
    finish_runclock(&run_time, &io_time);

    dlclose(handle);
}

int
loaddll(const char * dlname)
{
    char *error;
    struct bfinit {
	runfnp run; void *memptr; putfnp bf_putch; getfnp bf_getch;
    } *bf_init;

    if (verbose>4)
	fprintf(stderr, "Loading DLL \"%s\"\n", dlname);

    /* Normally this would use RTLD_LAZY, but here it should be v.short. */
    handle = dlopen(dlname, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
	fprintf(stderr, "dlopen() failed: %s\n", dlerror());
	exit(EXIT_FAILURE);
    }

    dlerror();    /* Clear any existing error */

    if (verbose>4)
	fprintf(stderr, "Finding bf_init symbol in \"%s\"\n", dlname);
    bf_init = dlsym(handle, "bf_init");
    if ((error = dlerror()) != NULL)  {
	fprintf(stderr, "%s\n", error);
	exit(EXIT_FAILURE);
    }

    /* I'm using a singlton structure to pass values between us and the DLL
     * so I don't have to mess with the even more ugly pointer casts mandated
     * by ISO and POSIX to pass function pointers around.
     *
     * I get the pointer to the DLL function I want to call.
     * Provide pointers to two of my functions (and so don't have to link
     * with the -rdynamic flag)
     * and set the DLL's tape pointer to our huge ram allocation.
     */
    bf_init->memptr = map_hugeram();
    bf_init->bf_putch = putch;
    bf_init->bf_getch = getch;
    runfunc = bf_init->run;
    if (verbose>4)
	fprintf(stderr, "DLL loaded successfully\n");
    return 0;
}

#endif

#ifdef __STRICT_ANSI__
#ifndef DISABLE_TCCLIB
#ifdef __GNUC__
#if __GNUC__<4 || ( __GNUC__==4 && __GNUC_MINOR__<7 )
#error "This GNUC version doesn't work properly with libtcc and -std=c99 turned on."
#endif
#endif
#endif
#endif
