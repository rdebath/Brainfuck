#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#ifndef DISABLE_TCCLIB
#include <libtcc.h>
#endif

#include "bfi.tree.h"
#include "bfi.run.h"
#include "bfi.ccode.h"

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

    if (cell_size == 0) {
	fprintf(ofd, "# ifdef C\n");
	fprintf(ofd, "# include <stdint.h>\n");
	fprintf(ofd, "# else\n");
	fprintf(ofd, "# define C int\n");
	fprintf(ofd, "# endif\n\n");
    }

    if (node_type_counts[T_INP] != 0 && !do_run)
    {
	fprintf(ofd, "static int\n");
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
	fprintf(ofd, "static void putch(int ch) { printf(\"%%lc\",ch); }\n");
	fprintf(ofd, "#else\n");
	fprintf(ofd, "static void putch(int ch) { putchar(ch); }\n");
	fprintf(ofd, "#endif\n\n");
    }

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
    if (hard_left_limit<0) {
	memoffset = -most_neg_maad_loop;
	memsize += memoffset;
	if (memsize < 65536)
	    memsize = 65536;
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
	fprintf(ofd, "  register %s * m;\n", cell_type);
	fprintf(ofd, "#endif\n");
#else
	fprintf(ofd, "  register %s * m;\n", cell_type);
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
    int indent = 0, disable_indent = 0;
    struct bfi * n = bfprog;
    char string_buffer[180], *sp = string_buffer, lastspch = 0;
    int ok_for_printf = 0, spc = 0;
    int minimal = 0;
    int add_mask = 0;
    int found_rail_runner;

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
				(n->count == '\n') || (n->count == '\t') || (n->count == '\033'));

	if (sp != string_buffer) {
	    if ((n->type != T_SET && n->type != T_ADD && n->type != T_PRT) ||
	        (n->type == T_PRT && !ok_for_printf) ||
		(sp >= string_buffer + sizeof(string_buffer) - 8) ||
		(sp > string_buffer+1 && lastspch == '\n')
	       ) {
		*sp++ = 0;
		if (enable_trace) {
		    pt(ofd, indent,0);
		    fprintf(ofd, "t(%d,%d,\"", n->line, n->col);
		    fprintf(ofd, "printf(...) ");
		    fprintf(ofd, "\",m+ %d)\n", n->offset);
		}
		pt(ofd, indent,0);
		if (strchr(string_buffer, '%') == 0 )
		    fprintf(ofd, "printf(\"%s\");\n", string_buffer);
		else
		    fprintf(ofd, "printf(\"%%s\", \"%s\");\n", string_buffer);
		sp = string_buffer; spc = 0;
	    }
	}

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

	case T_PRT:
	    spc++;
	    if ((ok_for_printf && !disable_indent) ||
			(sp != string_buffer && n->count > 0)) {
		lastspch = n->count;
		if (add_mask>0)
		    lastspch &= add_mask;
		if (lastspch == '\n') {
		    *sp++ = '\\'; *sp++ = 'n';
		} else if (lastspch == '\t') {
		    *sp++ = '\\'; *sp++ = 't';
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
	    if (!disable_indent) pt(ofd, indent,n);
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
	    if (!disable_indent) pt(ofd, indent,n);
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
	    fprintf(ofd, "if(m[%d]) ", n->offset);
	    if (n->next->next && n->next->next->jmp == n)
		disable_indent = 1;
	    else
		fprintf(ofd, "{\n");
	    break;

	case T_WHL:
#if 1
	    found_rail_runner = 0;
	    if (n->next->type == T_MOV &&
		n->next->next && n->next->next->jmp == n) {
		found_rail_runner = 1;
	    }

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

	    /* TCCLIB generates a slow 'strlen', libc is better, but the
	     * function call overhead is horrible.
	     */
	    if (found_rail_runner && cell_size == 8 && do_run &&
		add_mask <= 0 && n->next->count == 1) {
		pt(ofd, indent,n);
		fprintf(ofd, "if( m[%d] ) {\n", n->offset);
		pt(ofd, indent+1,n);
		fprintf(ofd, "m++;\n");
		pt(ofd, indent+1,n);
		fprintf(ofd, "if( m[%d] ) {\n", n->offset);
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
#endif

	case T_CMULT:
	case T_MULT:
	    pt(ofd, indent,n);
	    if (n->offset)
		fprintf(ofd, "while(m[%d]) ", n->offset);
	    else
		fprintf(ofd, "while(*m) ");
	    if (n->next->next && n->next->next->jmp == n)
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
	    if (n->prev->prev && n->prev->prev->jmp == n)
		break;

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
	if (enable_trace) {
	    pt(ofd, indent,0);
	    fprintf(ofd, "t(0,0,\"printf(...) \",m)\n");
	}
	if (strchr(string_buffer, '%') == 0 )
	    fprintf(ofd, "  printf(\"%s\");\n", string_buffer);
	else
	    fprintf(ofd, "  printf(\"%%s\", \"%s\");\n", string_buffer);
    }
    if (!noheader)
	fprintf(ofd, "  return 0;\n}\n");
}

#ifndef DISABLE_TCCLIB
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
    tcc_add_symbol(s, "getch", &getch);
    tcc_add_symbol(s, "putch", &putch);

#if __TCCLIB_VERSION == 0x000925
#define TCCDONE
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
	 *
	 * I could push it through a union, but this still works.
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

	rv = tcc_relocate(s);
	if (rv) {
	    perror("tcc_relocate()");
	    fprintf(stderr, "tcc_relocate failed return value=%d\n", rv);
	    exit(1);
	}
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

#if !defined(TCCDONE)
#define TCCDONE
static char * args[] = {"tcclib", 0};
    rv = tcc_run(s, 1, args);
    if (verbose && rv)
	fprintf(stderr, "tcc_run returned %d\n", rv);
    tcc_delete(s);
    free(ccode);
#endif
}
#endif
