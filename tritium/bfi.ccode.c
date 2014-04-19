#ifdef __STRICT_ANSI__
#define _POSIX_C_SOURCE 200809UL
#if __STDC_VERSION__ < 199901L
#error This program needs at least the C99 standard.
#endif

#ifndef DISABLE_TCCLIB
#ifdef __GNUC__
#if __GNUC__<4 || ( __GNUC__==4 && __GNUC_MINOR__<7 )
#error "This GNUC version doesn't work properly with libtcc and -std=c99 turned on."
#endif
#endif
#endif

#else
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#ifndef DISABLE_TCCLIB
#include <libtcc.h>
#endif

#ifndef DISABLE_DLOPEN
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#endif

#include "bfi.tree.h"
#include "bfi.run.h"
#include "bfi.ccode.h"

static const char * putname = "putch";
static int use_direct_getchar = 0;
static int use_dynmem = 0;
static int memsize = 30000;
#if defined(DISABLE_DLOPEN)
static const int use_dlopen = 0;
#else
static int use_dlopen = 0;
static int choose_runner = -1;
static char * cc_cmd = 0;
#endif

#ifndef DISABLE_TCCLIB
void run_tccode(void);
#endif
#ifndef DISABLE_DLOPEN
void run_gccode(void);
#endif

int
checkarg_ccode(char * opt, char * arg)
{
#if !defined(DISABLE_TCCLIB)
    if (!strcmp(opt, "-ltcc")) {
	choose_runner = 0;
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
#endif
    if (!strcmp(opt, "-dynmem")) {
	use_dynmem = 1;
	return 1;
    }
    if (!strcmp(opt, "-mem")) {
	if (arg == 0 || arg[0] < '0' || arg[0] > '9') {
	    use_dynmem = 1;
	    return 1;
	} else {
	    memsize = strtol(arg,0,10);
	    use_dynmem = (memsize <= 0);
	    return 2;
	}
    }
    return 0;
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
print_c_header(FILE * ofd)
{
    int memoffset = 0;
    int l_iostyle = iostyle;

    if (cell_mask > 0 && cell_size < 8 && l_iostyle == 1) l_iostyle = 0;

    /* Hello world mode ? */
    if (!enable_trace && !do_run && total_nodes == node_type_counts[T_PRT]) {
	int okay = 1;
	int ascii_only = 1;
	struct bfi * n = bfprog;
	/* Check the string; be careful. */
	while(n && okay) {
	    if (n->type != T_PRT || n->count > '~'
		|| (n->count < ' ' && n->count != '\n' && n->count != '\033')
		)
		okay = 0;

	    if (n->type == T_PRT && (n->count <= 0 || n->count > 127))
		ascii_only = 0;
	    if (n->type == T_INP) ascii_only = 0;

	    n = n->next;
	}

	if (ascii_only) l_iostyle = 0;

	if (okay) {
	    fprintf(ofd, "#include <stdio.h>\n\n");
	    fprintf(ofd, "int main(void)\n{\n");
	    putname = "putchar";
	    return ;
	}
    }

    fprintf(ofd, "#include <stdio.h>\n");
    fprintf(ofd, "#include <stdlib.h>\n");
    if( cell_size == CHAR_BIT || use_dynmem )
	fprintf(ofd, "#include <string.h>\n");
    fprintf(ofd, "\n");

    if (cell_size == 0) {
	fprintf(ofd, "# ifdef C\n");
	fprintf(ofd, "# include <stdint.h>\n");
	fprintf(ofd, "# else\n");
	fprintf(ofd, "# define C int\n");
	fprintf(ofd, "# endif\n\n");
    }

    if (do_run) {
	if (use_dlopen) {
	    fprintf(ofd, "#define mem brainfuck_memptr\n");
	    fprintf(ofd, "%s *mem;\n", cell_type);
	    fprintf(ofd, "#define putch (*brainfuck_putch)\n");
	    fprintf(ofd, "#define getch (*brainfuck_getch)\n");
	    fprintf(ofd, "void (*brainfuck_putch)(int ch);\n");
	    fprintf(ofd, "int (*brainfuck_getch)(int ch);\n");
	    fprintf(ofd, "int brainfuck(void){\n");
	    fprintf(ofd, "  register %s * m = mem;\n", cell_type);
	} else {
	    fprintf(ofd, "extern void putch(int ch);\n");
	    fprintf(ofd, "extern int getch(int ch);\n");
	    fprintf(ofd, "extern %s mem[];\n", cell_type);
	    fprintf(ofd, "int main(void){\n");
	    fprintf(ofd, "  register %s * m = mem;\n", cell_type);
	}
    }
    else
    {
	if (node_type_counts[T_INP] != 0)
	{
	    if (l_iostyle == 2 && (eofcell == 4 || (eofcell == 2 && EOF == -1))) {
		use_direct_getchar = 1;
	    } else {
		if (l_iostyle == 1) {
		    fprintf(ofd, "#include <locale.h>\n");
		    fprintf(ofd, "#include <wchar.h>\n\n");
		}
		fprintf(ofd, "static int\n");
		fprintf(ofd, "getch(int oldch)\n");
		fprintf(ofd, "{\n");
		fprintf(ofd, "  int ch;\n");
		if (l_iostyle == 2) {
		    fprintf(ofd, "  ch = getchar();\n");
		} else {
		    fprintf(ofd, "  do {\n");
		    if (l_iostyle == 1)
			fprintf(ofd, "\tch = getwchar();\n");
		    else
			fprintf(ofd, "\tch = getchar();\n");
		    fprintf(ofd, "  } while (ch == '\\r');\n");
		}
		switch(eofcell) {
		default:
		    fprintf(ofd, "#ifndef EOFCELL\n");
		    fprintf(ofd, "  if (ch != EOF) return ch;\n");
		    fprintf(ofd, "  return oldch;\n");
		    fprintf(ofd, "#else\n");
		    fprintf(ofd, "#if EOFCELL == EOF\n");
		    fprintf(ofd, "  return ch;\n");
		    fprintf(ofd, "#else\n");
		    fprintf(ofd, "  if (ch != EOF) return ch;\n");
		    fprintf(ofd, "  return EOFCELL;\n");
		    fprintf(ofd, "#endif\n");
		    fprintf(ofd, "#endif\n");
		    break;
		case 1:
		    fprintf(ofd, "  if (ch != EOF) return ch;\n");
		    fprintf(ofd, "  return oldch;\n");
		    break;
		case 3:
		    fprintf(ofd, "  if (ch != EOF) return ch;\n");
		    fprintf(ofd, "  return 0;\n");
		    break;
		case 2:
    #if EOF != -1
		    fprintf(ofd, "  if (ch != EOF) return ch;\n");
		    fprintf(ofd, "  return -1;\n");
		    break;
    #endif
		case 4:
		    fprintf(ofd, "  return ch;\n");
		    break;
		}
		fprintf(ofd, "}\n\n");
	    }
	}

	if (node_type_counts[T_PRT] != 0) {
	    switch(l_iostyle)
	    {
	    case 0: case 2:
		putname = "putchar";
		break;
	    case 1:
		fprintf(ofd, "static void putch(int ch)\n{\n"
			    "  if(ch>127)\n"
			    "\tprintf(\"%%lc\",ch);\n"
			    "  else\n"
			    "\tputchar(ch);\n"
			    "}\n\n");
		break;
	    }
	}

	if (node_type_counts[T_MOV] == 0) {
	    if (min_pointer >= 0)
		memsize = max_pointer+1;
	    else {
		memoffset = -min_pointer;
		memsize = max_pointer-min_pointer+1;
	    }
	}
	if (hard_left_limit<0) {
	    memoffset = -most_neg_maad_loop;
	    memsize += memoffset;
	}

	if (node_type_counts[T_MOV] == 0 && memoffset == 0) {
	    fprintf(ofd, "static %s m[%d];\n", cell_type, max_pointer+1);
	    fprintf(ofd, "int main(void){\n");
	    if (enable_trace)
		fprintf(ofd, "#define mem m\n");
	} else if (!use_dynmem) {
	    fprintf(ofd, "static %s mem[%d];\n", cell_type, memsize);
	    fprintf(ofd, "int main(void){\n");

	    if (memoffset > 0)
		fprintf(ofd, "  register %s * m = mem + %d;\n",
			cell_type, memoffset);
	    else
		fprintf(ofd, "  register %s * m = mem;\n", cell_type);
	} else {

	    printf("#define CELL %s\n", cell_type);
	    printf("#define MINOFF (%d)\n", min_pointer);
	    printf("#define MAXOFF (%d)\n", max_pointer);
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
		"\n"	"    amt = (off / MINALLOC + 1) * MINALLOC;"
		"\n"	"    mem = realloc(mem, (memsize+amt)*sizeof(*mem));"
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
		"\n"	"int main(void){"
		"\n"	"  register CELL * m = move_ptr(alloc_ptr(mem),0);" );
	}

	if (node_type_counts[T_INP] != 0) {
	    fprintf(ofd, "  setbuf(stdout, 0);\n");
	    if (l_iostyle == 1)
		fprintf(ofd, "  setlocale(LC_ALL, \"\");\n");
	}
    }

    if (enable_trace)
	fputs(	"#define t(p1,p2,p3,p4) fprintf(stderr, \"P(%d,%d)=%s"
		"mem[%d]=%d%s\\n\", \\\n\tp1, p2, p3,"
		" p4-mem, (p4>=mem?*(p4):0), p4>=mem?\"\":\" SIGSEG\");\n\n",
	    ofd);
}

void
print_ccode(FILE * ofd)
{
    int indent = 0, disable_indent = 0;
    struct bfi * n = bfprog;
    int add_mask = 0;
    int found_rail_runner;
    int use_multifunc;	    /* Create multiple functions ... TODO */

    if (cell_size > 0 &&
	cell_size != sizeof(int)*CHAR_BIT &&
	cell_size != sizeof(short)*CHAR_BIT &&
	cell_size != sizeof(char)*CHAR_BIT)
	add_mask = cell_mask;

    if (verbose)
	fprintf(stderr, "Generating C Code.\n");

    /* Scan the nodes so we get an APPROXIMATE distance measure */
    for(use_multifunc=0, n = bfprog; n; n=n->next) {
	n->ipos = use_multifunc++;
	if (n->type == T_PRT && n->next && n->next->type == T_PRT)
	    use_multifunc--; /* T_PRT tokens tend to get merged */
    }
    use_multifunc = (use_multifunc>8192);

    if (!noheader) print_c_header(ofd);

    n = bfprog;
    while(n)
    {
	if (n->orgtype == T_END) indent--;
	if (add_mask > 0) {
	    switch(n->type)
	    {
	    case T_WHL:
	    case T_FOR: case T_CMULT: case T_IF:
		pt(ofd, indent, 0);
		fprintf(ofd, "m[%d] &= %d;\n", n->offset, add_mask);
	    }
	}
	switch(n->type)
	{
	case T_MOV:
	    if (!disable_indent) pt(ofd, indent,n);
	    if (!do_run && use_dynmem && n->count>0)
		fprintf(ofd, "m = move_ptr(m,%d);\n", n->count);
	    else if (n->count == 1)
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
	    if (!disable_indent) pt(ofd, indent,n);
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
	    if (!disable_indent) pt(ofd, indent,n);
	    if (cell_size <= 0 && (n->count < -128 || n->count >= 256)) {
		fprintf(ofd, "m[%d] = (%s) %d;\n",
		    n->offset, cell_type, n->count);
	    } else
	    if(n->offset == 0)
		fprintf(ofd, "*m = %d;\n", n->count);
	    else
		fprintf(ofd, "m[%d] = %d;\n", n->offset, n->count);
	    if (enable_trace) {
		pt(ofd, indent,0);
		fprintf(ofd, "t(%d,%d,\"\",m+ %d)\n", n->line, n->col, n->offset);
	    }
	    break;

	case T_CALC:
	    if (!disable_indent) pt(ofd, indent,n);
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

#define okay_for_cstr(xc) (((xc) >= ' ' && (xc) <= '~') || (xc == '\n'))

	case T_PRT:
	    if (!disable_indent) pt(ofd, indent,n);
	    if (n->count == -1) {
		if (add_mask > 0 && (add_mask < 0xFF || iostyle != 2)) {
		    fprintf(ofd, "%s(m[%d] &= %d);\n", putname, n->offset, add_mask);
		} else if (n->offset == 0)
		    fprintf(ofd, "%s(*m);\n", putname);
		else
		    fprintf(ofd, "%s(m[%d]);\n", putname, n->offset);
		break;
	    }

	    if (!okay_for_cstr(n->count)) {
		if (n->count == '\n')
		    fprintf(ofd, "%s('\\n');\n", putname);
		else
		    fprintf(ofd, "%s(%d);\n", putname, n->count);
	    }
	    else
	    {
		unsigned i = 0, j;
		int got_perc = 0;
		int lastc = 0;
		unsigned slen = 4;	/* First char + nul + ? */
		struct bfi * v = n;
		char *s, *p;
		while(v->next && v->next->type == T_PRT &&
			    okay_for_cstr(v->next->count)) {
		    v = v->next;
		    if (v->count == '%') got_perc = 1;
		    if (v->count == '\n' || v->count == '\\' || v->count == '"')
			slen++;
		    i++;
		    slen++;
		    if (v->next && v->next->count == 10)
			;
		    else if (slen > 132 || (slen>32 && v->count == '\n'))
			break;
		}
		p = s = malloc(slen);

		for(j=0; j<=i; j++) {
		    lastc = n->count;
		    if (n->count == '\n') { *p++ = '\\'; *p++ = 'n'; } else
		    if (n->count == '\\') { *p++ = '\\'; *p++ = '\\'; } else
		    if (n->count == '"') { *p++ = '\\'; *p++ = '"'; } else
			*p++ = n->count;
		    if (j!=i)
			n = n->next;
		}
		*p = 0;

		if ((p == s+1 && *s != '\'') || (p==s+2 && lastc == '\n')) {
		    fprintf(ofd, "%s('%s');\n", putname, s);
		} else if (lastc == '\n') {
		    *--p = 0; *--p = 0;
		    fprintf(ofd, "puts(\"%s\");\n", s);
		} else if (!got_perc)
		    fprintf(ofd, "printf(\"%s\");\n", s);
		else
		    fprintf(ofd, "printf(\"%%s\", \"%s\");\n", s);
		free(s);
	    }
	    break;
#undef okay_for_cstr

	case T_INP:
	    if (!disable_indent) pt(ofd, indent,n);
	    if (use_direct_getchar) {
		if (n->offset == 0)
		    fprintf(ofd, "*m = getchar();\n");
		else
		    fprintf(ofd, "m[%d] = getchar();\n", n->offset);
	    } else if (n->offset == 0)
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
	    fprintf(ofd, "if(m[%d]) ", n->offset);
	    if (n->next->next && n->next->next->jmp == n && !enable_trace)
		disable_indent = 1;
	    else
		fprintf(ofd, "{\n");
	    break;

	case T_WHL:

	    found_rail_runner = 0;
	    if (n->next->type == T_MOV &&
		n->next->next && n->next->next->jmp == n) {
		found_rail_runner = 1;
	    }

#if !defined(DISABLE_TCCLIB) && defined(__i386__)
	    /* TCCLIB generates a slow while() with two jumps in the loop,
	     * and several memory accesses. This is the assembler we would
	     * actually prefer.
	     *
	     * These prints are really ugly; I need a 'print gas in C'
	     * function if we have much more.
	     */
	    if (found_rail_runner && cell_size == 32) {
		fprintf(ofd, "#if !defined(__i386__) || !defined(__TINYC__)\n");
		pt(ofd, indent,n);
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
	    if (found_rail_runner && cell_size == CHAR_BIT &&
		add_mask <= 0 && n->next->count == 1) {
		pt(ofd, indent,n);
		fprintf(ofd, "if(m[%d]) {\n", n->offset);
		pt(ofd, indent+1,n);
		fprintf(ofd, "m++;\n");
		pt(ofd, indent+1,n);
		fprintf(ofd, "if(m[%d]) {\n", n->offset);
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

	case T_CMULT:
	case T_MULT:
	    pt(ofd, indent,n);
	    if (n->offset)
		fprintf(ofd, "while(m[%d]) ", n->offset);
	    else
		fprintf(ofd, "while(*m) ");
	    if (n->next->next && n->next->next->jmp == n && !enable_trace)
		disable_indent = 1;
	    else
		fprintf(ofd, "{\n");
	    break;

	case T_FOR:
	    pt(ofd, indent,n);
	    fprintf(ofd, "for(;m[%d];) {\n", n->offset);
	    break;

	case T_END:
	case T_ENDIF:
	    if (disable_indent) {
		disable_indent = 0;
		break;
	    }
	    pt(ofd, indent,n);
	    fprintf(ofd, "}\n");
	    break;

	case T_STOP:
	    if (!disable_indent) pt(ofd, indent,n);
	    fprintf(ofd, "return 1;\n");
	    break;

	case T_NOP:
	    fprintf(stderr, "Warning on code generation: "
	           "NOP node: ptr+%d, cnt=%d, @(%d,%d).\n",
		    n->offset, n->count, n->line, n->col);
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

    if (!noheader)
	fprintf(ofd, "  return 0;\n}\n");
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
	use_dlopen = (total_nodes < 4000);
    if (use_dlopen)
	run_gccode();
    else
	run_tccode();
#endif
}
#endif

#ifndef DISABLE_TCCLIB
typedef void (*void_func)(void);

void
run_tccode(void)
{
    char * ccode;
    size_t ccodelen;

    FILE * ofd;
    TCCState *s;
    int rv;
    void * memp;
#ifdef __STRICT_ANSI__
    void * iso_workaround;
#endif

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
     *
     * The ugly casting is forced by the C99 standard as a (void*) is not a
     * valid cast for a function pointer.
     */

#ifdef __STRICT_ANSI__
    *(void_func*) &iso_workaround  = (void_func) &getch;
    tcc_add_symbol(s, "getch", iso_workaround);
    *(void_func*) &iso_workaround  = (void_func) &putch;
    tcc_add_symbol(s, "putch", iso_workaround);
#else
    tcc_add_symbol(s, "getch", &getch);
    tcc_add_symbol(s, "putch", &putch);

#if __TCCLIB_VERSION == 0x000925
#define TCCDONE
    {
	int (*func)(void);
	int imagesize;
	void * image = 0;

	if (verbose)
	    fprintf(stderr, "Running C Code using libtcc 9.25.\n");

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

	func();
	free(image);
    }
#endif

#if __TCCLIB_VERSION == 0x000926
#define TCCDONE
    {
	int (*func)(void);

	if (verbose)
	    fprintf(stderr, "Running C Code using libtcc 9.26.\n");

	rv = tcc_relocate(s);
	if (rv) {
	    perror("tcc_relocate()");
	    fprintf(stderr, "tcc_relocate failed return value=%d\n", rv);
	    exit(1);
	}

	/*
	 * The ugly casting is forced by the C99 standard as a (void*) is not a
	 * valid cast for a function pointer.
	*(void **) (&func) = tcc_get_symbol(s, "main");
	 */
	func = tcc_get_symbol(s, "main");

	if (!func) {
	    fprintf(stderr, "Could not find compiled code entry point\n");
	    exit(1);
	}
	func();

	tcc_delete(s);
	free(ccode);
    }
#endif
#endif

#if !defined(TCCDONE)
#define TCCDONE
static char * args[] = {"tcclib", 0};
    if (verbose)
	fprintf(stderr, "Running C Code using libtcc tcc_run().\n");

    rv = tcc_run(s, 1, args);
    if (verbose && rv)
	fprintf(stderr, "tcc_run returned %d\n", rv);
    tcc_delete(s);
    free(ccode);
#endif
}
#endif


#ifndef DISABLE_DLOPEN
#define BFBASE "bfpgm"

static void compile_and_run(void);

static char tmpdir[] = "/tmp/bfrun.XXXXXX";
static char ccode_name[sizeof(tmpdir)+16];
static char dl_name[sizeof(tmpdir)+16];
static char obj_name[sizeof(tmpdir)+16];

void
run_gccode(void)
{
    FILE * ofd;
    if( mkdtemp(tmpdir) == 0 ) {
	perror("mkdtemp()");
	exit(1);
    }
    strcpy(ccode_name, tmpdir); strcat(ccode_name, "/"BFBASE".c");
    strcpy(dl_name, tmpdir); strcat(dl_name, "/"BFBASE".so");
    strcpy(obj_name, tmpdir); strcat(obj_name, "/"BFBASE".o");
    ofd = fopen(ccode_name, "w");
    print_ccode(ofd);
    fclose(ofd);
    compile_and_run();
}

/*  Needs:   gcc -shared -fPIC -o libfoo.so foo.c
    And:     -ldl

    NB: -fno-asynchronous-unwind-tables
*/

/* If we're 32 bit on a 64bit or vs.versa. we need an extra option */
#ifndef CC
#if defined(__GNUC__) && ((__GNUC__>4) || (__GNUC__==4 && __GNUC_MINOR__>=4))
#if defined(__x86_64__)
#if defined(__ILP32__)
#define CC "gcc -mx32"
#else
#define CC "gcc -m64"
#endif
#elif defined(__i386__)
#define CC "gcc -m32"
#else
#define CC "gcc"
#endif
#else
#define CC "cc"
#define DLOPEN_ABS_PATH
#endif
#endif

typedef void (*voidfnp)(void);
static int loaddll(const char *);
static voidfnp runfunc;
static void *handle;

static void
compile_and_run(void)
{
    char cmdbuf[256];
    int ret;
    const char * cc = CC;

    if (cc_cmd) cc = cc_cmd;

    if (verbose)
	fprintf(stderr,
		"Running C Code using \"%s -shared -fPIC\" and dlopen().\n",
		cc);

#ifdef DLOPEN_ABS_PATH
    sprintf(cmdbuf, "%s -shared -fPIC -o %s %s",
                cc, dl_name, ccode_name);
    ret = system(cmdbuf);
#else
    /* Like this so that ccache has a good path and distinct compile. */
    sprintf(cmdbuf, "cd %s; %s -fPIC -c -o %s %s",
	    tmpdir, cc, BFBASE".o", BFBASE".c");
    ret = system(cmdbuf);

    if (ret != -1) {
	sprintf(cmdbuf, "cd %s; %s -shared -fPIC -o %s %s",
		tmpdir, cc, dl_name, BFBASE".o");
	ret = system(cmdbuf);
    }
#endif

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

    unlink(ccode_name);
    unlink(dl_name);
    unlink(obj_name);
    rmdir(tmpdir);

    (*runfunc)();

    dlclose(handle);
}

int
loaddll(const char * dlname)
{
    char *error;
    void ** brainfuck_memptr;
    // void ** ptr;
    voidfnp *ptr;

    handle = dlopen(dlname, RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "%s\n", dlerror());
        exit(EXIT_FAILURE);
    }

    dlerror();    /* Clear any existing error */

    /* The C99 standard leaves casting from "void *" to a function
       pointer undefined.  The assignment used below is the POSIX.1-2003
       (Technical Corrigendum 1) workaround; see the Rationale for the
       POSIX specification of dlsym().
     *						-- Linux man page dlsym()
     */

    *(void **) (&runfunc) = dlsym(handle, "brainfuck");

    /* Set the memory pointer to point to our allocated huge buffer */
    brainfuck_memptr = dlsym(handle, "brainfuck_memptr");
    *brainfuck_memptr = map_hugeram();

    /* Set some manual fn pointers back here so we don't have to link
     * with the -rdynamic flag */
    ptr = dlsym(handle, "brainfuck_getch");
    *ptr = (voidfnp) &getch;
    ptr = dlsym(handle, "brainfuck_putch");
    *ptr = (voidfnp) &putch;

    if ((error = dlerror()) != NULL)  {
        fprintf(stderr, "%s\n", error);
        exit(EXIT_FAILURE);
    }
    return 0;
}

#endif
