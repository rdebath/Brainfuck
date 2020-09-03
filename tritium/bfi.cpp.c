#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "bfi.tree.h"
#include "bfi.run.h"
#include "bfi.cpp.h"

/*
 * This outputs the tree as a flat language.
 *
 * This can easily be used as macro invocations by any macro assembler or
 * anything that can be translated in a similar way. The C preprocessor
 * has be used successfully to translate it into several different
 * scripting languages (except Python).
 *
 *  bfi -A pgm.b | gcc -include macro.rb.cpp -E -P - > pgm.out
 *
 * The default header generates some C++ code.
 */
void
print_cpp(void)
{
    struct bfi * n = bfprog;
    const char * cpp_type = "Cell";	/* Boost library uses "C" */
    /* Boost library segfaults if defined bitsize is over about 250000 */
    const unsigned max_cpp_bits = 250000;

    if (!noheader) {
	int add_mask = 0, unknown_type = 0;

	if (cell_size != sizeof(int)*CHAR_BIT &&
	    cell_size != sizeof(short)*CHAR_BIT &&
	    cell_size != sizeof(char)*CHAR_BIT &&
	    cell_mask > 0) {
	    add_mask = 1;
	}
	unknown_type = (strcmp(cell_type, "C") == 0);
	if (!unknown_type) cpp_type = cell_type;

	puts("#ifndef bf_start");
	puts("#include <stdlib.h>");
	puts("#include <unistd.h>");
	if (cell_type_iso || unknown_type)
	    puts("#include <stdint.h>");

	if (unknown_type) {
	    puts("#ifdef __cplusplus");
	    puts("#define register");
	    puts("#include <boost/multiprecision/cpp_int.hpp>");
	    puts("  using namespace boost::multiprecision;");
	    /* Use the Boost predefined uint123_t types or manufacture
	     * a new one of exactly the right number of bits. */

	    /* BEWARE: The signed types are not compliant with the C,
	     * C++ or POSIX standards. For the C and C++ standards they
	     * are one bit too large for their advertised size and in
	     * addition the POSIX standard requires that the types use
	     * twos-complement operations.
	     */
	    if ( cell_length > 0 && cell_length != 128 &&
		 cell_length != 256 && cell_length != 512 &&
		 cell_length != 1024 && cell_length <= max_cpp_bits) {
		printf("%s%d%s%d%s%d%s\n",
		    "  typedef number<cpp_int_backend<", cell_length,
		    ", ", cell_length, ",\n"
		    "  unsigned_magnitude, unchecked, void> >"
		    " uint", cell_length,"_t;"
		);
	    }
	    puts("#endif");

	    printf("#ifndef %s\n", cpp_type);
	    puts("#ifdef __cplusplus");
	    if (cell_length > max_cpp_bits)
		printf("#define %s cpp_int\n", cpp_type);
	    else
		printf("#define %s uint%d_t\n", cpp_type,
		    cell_length?cell_length:1024);
	    puts("#else");
	    printf("#define %s uintmax_t\n", cpp_type);
	    puts("#endif");
	    puts("#endif");
	} else {
	    puts("#ifdef __cplusplus");
	    puts("#define register");
	    puts("#endif");
	}

	puts("#define bf_end() return 0;}");
	printf("%s%s%s\n",
	    "#define bf_start(msz,moff,bits) ",cpp_type," mem[msz+moff]; \\");

	if (node_type_counts[T_CHR] || node_type_counts[T_PRT])
	    printf("  void putch(%s c) {char s=(int)c; write(1,&s,1);} \\\n", cpp_type);

	if (node_type_counts[T_CHR])
	    puts("  void putstr(const char *s) {const char *p=s; while(*p)p++; write(1,s,p-s);} \\");

	printf("%s%s%s\n",
	    "  int main(void){register ",cpp_type,"*p=mem+moff;");

	if (enable_trace)
	    puts("#define posn(l,c) /* Line l Column c */");

	/* See:  opt_regen_mov */
	if (node_type_counts[T_MOV])
	    puts("#define ptradd(x) p += x;");

	if (node_type_counts[T_ADD])
	    puts("#define add_i(x,y) p[x] += y;");

	if (node_type_counts[T_SET])
	    printf("#define set_i(x,y) p[x] = (%s)y;\n", cpp_type);

	/* See:  opt_no_calc */
	if (node_type_counts[T_CALC])
	    printf("%s\n",
		"#define add_c(x,y) p[x] += p[y];"
	"\n"	"#define add_cm(x,y,z) p[x] += p[y] * z;"
	"\n"	"#define set_c(x,y) p[x] = p[y];"
	"\n"	"#define set_cm(x,y,z) p[x] = p[y] * z;"
	"\n"	"#define add_cmi(o1,o2,o3,o4) p[o1] += p[o2] * o3 + o4;"
	"\n"	"#define set_cmi(o1,o2,o3,o4) p[o1] = p[o2] * o3 + o4;"
	"\n"	"#define set_tmi(o1,o2,o3,o4,o5,o6) p[o1] = o2 + p[o3] * o4 + p[o5] * o6;");

	if (node_type_counts[T_CALCMULT])
	    puts("#define multcell(x,y,z) p[x] = p[y] * p[z];");

	if (node_type_counts[T_LT])
	    puts("#define lessthan(x,y,z) p[x] += (p[y] < p[z]); /* unsigned! */");

	if (node_type_counts[T_DIV])
	    printf("#define divtok(x) %s%s%s%s\n",
			"/* unsigned div! */\\",
		"\n"	"  if (p[x+1] != 0) {\\"
		"\n"	"    ",cpp_type," N = p[x+0], D = p[x+1];\\"
		"\n"	"    p[x+3] = N/D;\\"
		"\n"	"    p[x+2] = N%D;\\"
		"\n"	"  } else {\\"
		"\n"	"    p[x+3] = 0;\\"
		"\n"	"    p[x+2] = p[x+0];\\"
		"\n"	"  }");

	if(node_type_counts[T_MULT] || node_type_counts[T_CMULT]
	    || node_type_counts[T_WHL]) {
	    if (add_mask) {
		printf("#define lp_start(x,y,c) "
			"if((p[x]&=0x%x)==0) goto E##y; S##y:\n", cell_mask);
		printf("#define lp_end(x,y) "
			"if(p[x]&=0x%x) goto S##y; E##y:\n", cell_mask);
	    } else {
		puts("#define lp_start(x,y,c) if(p[x]==0) goto E##y; S##y:");
		puts("#define lp_end(x,y) if(p[x]) goto S##y; E##y:");
	    }
	}

	/* See:  opt_no_endif */
	if (node_type_counts[T_IF]) {
	    if (add_mask)
		printf("#define if_start(x,y) "
			"if((p[x]&=0x%x)==0) goto E##y;\n", cell_mask);
	    else
		puts("#define if_start(x,y) if(p[x]==0) goto E##y;");
	    puts("#define if_end(x) E##x:");
	}

	if (node_type_counts[T_STOP])
	    puts("#define bf_err() exit(1);");

	if (node_type_counts[T_DUMP])
	    puts("#define bf_dump(l,c) /* Hmmm */");

	if (node_type_counts[T_CHR]) {
	    printf("%s\n",
		"#define outchar(x) putch(x);"
	"\n"	"#define outstr(x) putstr(x);");
	}
	if (node_type_counts[T_PRT])
	    puts("#define outcell(x) putch(p[x]);");

	if (node_type_counts[T_INP]) {
	    switch(eofcell)
	    {
	    case 0:
	    case 1:
		puts("#define inpchar(x) "
		     "{unsigned char a[1]; if(read(0,a,1)>0) p[x]= *a;}");
		break;
	    case 4:
#if EOF != -1
		printf("#define inpchar(x) "
		     "{unsigned char a[1]; if(read(0,a,1)>0) p[x]= *a;"
		     "else p[x]= %d;}", EOF);
		break;
#endif
	    case 2:
		puts("#define inpchar(x) "
		     "{unsigned char a[1]; if(read(0,a,1)>0) p[x]= *a;"
		     "else p[x]= -1;}");
		break;
	    case 3:
		puts("#define inpchar(x) "
		     "{unsigned char a[1]; if(read(0,a,1)>0) p[x]= *a;"
		     "else p[x]= 0;}");
		break;
	    }
	}

	if (node_type_counts[T_INPI] || node_type_counts[T_PRTI]) {

	    puts("#ifdef __cplusplus");

	    /* Beware: mixing iostream with read() & write() */
	    if (node_type_counts[T_INPI])
		puts("#define getdecimalcell(x) std::cin >> p[x];");

	    if (node_type_counts[T_PRTI])
		puts("#define putdecimalcell(x) std::cout << p[x] << std::flush;");

	    puts("#else");

	    /* Beware: You must supply integer I/O externs for C */
	    if (node_type_counts[T_INPI]) {
		puts("#define getdecimalcell(x) getdeccell(p+x);");
		printf("  extern void getdeccell(%s * cellv);\n", cpp_type);
	    }

	    if (node_type_counts[T_PRTI]) {
		puts("#define putdecimalcell(x) putdeccell(p[x]);");
		printf("  extern void putdeccell(%s cellv);\n", cpp_type);
	    }

	    puts("#endif");
	}

	puts("#endif");
	puts(" /* ############################### CUT "
	     "HERE ############################### */");
    }

    printf("bf_start(%d,%d,%d)\n", memsize, -most_neg_maad_loop, cell_size);
    while(n)
    {
	if (enable_trace)
	    printf("posn(%d,%3d)\n", n->line, n->col);
	switch(n->type)
	{
	case T_MOV:
	    if (n->count)
		printf("ptradd(%d)\n", n->count);
	    break;

	case T_ADD:
	    if (n->count)
		printf("add_i(%d,%d)\n", n->offset, n->count);
	    break;

	case T_SET:
	    printf("set_i(%d,%d)\n", n->offset, n->count);
	    break;

	case T_CALC:
	    if (n->count == 0) {
		if (n->offset == n->offset2 && n->count2 == 1 && n->count3 == 1) {
		    printf("add_c(%d,%d)\n", n->offset, n->offset3);
		    break;
		}

		if (n->offset == n->offset2 && n->count2 == 1) {
		    printf("add_cm(%d,%d,%d)\n", n->offset, n->offset3, n->count3);
		    break;
		}

		if (n->count3 == 0) {
		    if (n->count2 == 1)
			printf("set_c(%d,%d)\n", n->offset, n->offset2);
		    else
			printf("set_cm(%d,%d,%d)\n", n->offset, n->offset2, n->count2);
		    break;
		}
	    }

	    if (n->offset == n->offset2 && n->count2 == 1) {
		printf("add_cmi(%d,%d,%d,%d)\n",
			n->offset, n->offset3, n->count3, n->count);
		break;
	    }

	    if (n->count3 == 0) {
		printf("set_cmi(%d,%d,%d,%d)\n",
		    n->offset, n->offset2, n->count2, n->count);
	    } else {
		printf("set_tmi(%d,%d,%d,%d,%d,%d)\n",
		    n->offset, n->count, n->offset2, n->count2,
		    n->offset3, n->count3);
	    }
	    break;

	case T_CALCMULT:
	    if (n->count == 0 && n->count2 == 1 && n->count3 == 1) {
		printf("multcell(%d,%d,%d)\n",
		    n->offset, n->offset2, n->offset3);
	    } else {
		printf("multcell_ex(%d,%d,%d,%d,%d,%d)\n",
		    n->offset, n->count, n->offset2, n->count2,
		    n->offset3, n->count3);
	    }
	    break;

	case T_LT:
	    if (n->count == 0 && n->count2 == 1 && n->count3 == 1) {
		printf("lessthan(%d,%d,%d)\n",
		    n->offset, n->offset2, n->offset3);
	    } else {
		printf("lessthan_ex(%d,%d,%d,%d,%d,%d)\n",
		    n->offset, n->count, n->offset2, n->count2,
		    n->offset3, n->count3);
	    }
	    break;

#define okay_for_cstr(xc) \
                    ( (xc) >= ' ' && (xc) <= '~' && \
                      (xc) != '\\' && (xc) != '"' \
                    )

	case T_DIV:
	    printf("divtok(%d)\n", n->offset);
	    break;

	case T_PRT:
	    printf("outcell(%d)\n", n->offset);
	    break;

	case T_CHR:
	    if (!okay_for_cstr(n->count) ||
		    !n->next || n->next->type != T_CHR) {

		printf("outchar(%d)\n", n->count);
	    } else {
		unsigned i = 0, j;
		struct bfi * v = n;
		char *s, *p;
		while(v->next && v->next->type == T_CHR &&
			    okay_for_cstr(v->next->count)) {
		    v = v->next;
		    i++;
		}
		p = s = malloc(i+2);

		for(j=0; j<i; j++) {
		    *p++ = (char) /*GCC -Wconversion*/ n->count;
		    n = n->next;
		}
		*p++ = (char) /*GCC -Wconversion*/ n->count;
		*p++ = 0;

		printf("outstr(\"%s\")\n", s);
		free(s);
	    }
	    break;
#undef okay_for_cstr

	case T_INP:
	    printf("inpchar(%d)\n", n->offset);
	    break;

	case T_PRTI:
	    printf("putdecimalcell(%d)\n", n->offset);
	    break;

	case T_INPI:
	    printf("getdecimalcell(%d)\n", n->offset);
	    break;

	case T_IF:
	    printf("if_start(%d,%d)\n", n->offset, n->count);
	    break;

	case T_ENDIF:
	    printf("if_end(%d)\n", n->jmp->count);
	    break;

	case T_MULT: case T_CMULT:
	case T_WHL:
	    printf("lp_start(%d,%d,%s)\n",
		    n->offset, n->count, tokennames[n->type]);
	    break;

	case T_END:
	    printf("lp_end(%d,%d)\n",
		    n->jmp->offset, n->jmp->count);
	    break;

	case T_STOP:
	    printf("bf_err()\n");
	    break;

	case T_DUMP:
	    printf("bf_dump(%d,%d)\n", n->line, n->col);
	    break;

	case T_NOP:
	    fprintf(stderr, "Warning on code generation: "
	           "%s node: ptr+%d, cnt=%d, @(%d,%d).\n",
		    tokennames[n->type],
		    n->offset, n->count, n->line, n->col);
	    break;

	default:
	    printf("%s(%d,%d,%d,%d,%d,%d)\n",
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
    printf("bf_end()\n");
}
