#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * C GMP translation from BF, runs at about 790,000,000 instructions per second.
 *
 * Note: This accepts the optimise flag, but strictly speaking the
 * optimisations may not be valid for bignum cells. Of course in
 * the real world bignums do have a limited range and an optimisation
 * that assumes that they wrap is not actually wrong.
 *
 * It isn't really wrong for true infinities either as plus and minus infinity
 * don't really obey the normal rules.
 *
 * And finally, the result if a bignum implementation is given an infinite job
 * is actually undefined, giving the 'right' result isn't actually wrong and
 * if you want it to crash all you have to do is turn off optimisation.
 *
 * If this is configured with byte cells the gmp library is not called.
 *
 * The tape on this one is unusual because the cells have to be init'd before
 * they are used. To allow this I've used a slower 'realloc' method to store
 * the tape with position checks on pointer movement.
 *
 * This is faster than the previous linked list implementation.
 */

int ind = 0;
#define I        printf("%*s", ind*4, "")
#define prv(s,v) printf("%*s" s "\n", ind*4, "", (v))

static void print_cstring(void);

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    if (strcmp(arg, "-savestring") == 0) return 1;
    return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	printf("#include <stdlib.h>\n"
	       "#include <stdio.h>\n"
	       "#include <string.h>\n");
	if (!bytecell)
	    printf("#include <gmp.h>\n");
	puts("");
	if (bytecell) {
	    printf("typedef unsigned char mpz_t;\n");
	    printf("#define mpz_init(X) (X) = 0\n");
	}

	printf("mpz_t * mem = 0;\n");
	printf("int memsize = 0;\n");
	printf("#define MINALLOC 16\n");
	puts("");

	printf("%s",
	    "static\n"
	    "mpz_t *\n"
	    "alloc_ptr(mpz_t *p)\n"
	    "{\n"
	    "    int amt, memoff, i, off;\n"
	    "    if (p >= mem && p < mem+memsize) return p;\n"
	    "\n"
	    "    memoff = p-mem; off = 0;\n"
	    "    if (memoff<0) off = -memoff;\n"
	    "    else if(memoff>=memsize) off = memoff-memsize;\n"
	    "    amt = off / MINALLOC;\n"
	    "    amt = (amt+1) * MINALLOC;\n"
	    "    mem = realloc(mem, (memsize+amt)*sizeof(*mem));\n"
	    "    if (memoff<0) {\n"
	    "        memmove(mem+amt, mem, memsize*sizeof(*mem));\n"
	    "        for(i=0; i<amt; i++)\n"
	    "            mpz_init(mem[i]);\n"
	    "        memoff += amt;\n"
	    "    } else {\n"
	    "        for(i=0; i<amt; i++)\n"
	    "            mpz_init(mem[memsize+i]);\n"
	    "    }\n"
	    "    memsize += amt;\n"
	    "    return mem+memoff;\n"
	    "}\n"
	);
	puts("");

#ifdef ALLOW_INLINE_KEYWORD
	printf("%s",
	    "static inline\n"
	    "mpz_t *\n"
	    "move_ptr(mpz_t *p, int off) {\n"
	    "    p += off;\n"
	    "    if ((off>0 || p >= mem) && (off<0 || p < mem+memsize)) return p;\n"
	    "    return alloc_ptr(p);\n"
	    "}\n"
	);
#else
	printf("%s",
	    "#define move_ptr(P,O) (((P)+=(O), \\\n"
	    "        ((O)>=0 && (P)>=mem+memsize) || \\\n"
	    "        ((O)<=0 && (P)<mem) ) ? \\\n"
	    "            alloc_ptr(P): (P) )\n"
	);
#endif

	puts("");

	printf("int\n"
	       "main(int argc, char ** argv)\n"
	       "{\n"
	    );
	ind ++;
	I; printf("register mpz_t * p;\n");
	I; printf("register int c;\n");
	if (bytecell) {
	    I; printf("register int v;\n");
	} else {
	    I; printf("mpz_t v;\n");
	    I; printf("mpz_init(v);\n");
	}
	I; printf("setbuf(stdout, 0);\n");
	I; printf("p = move_ptr(mem,0);\n");
	break;

    case '~':
	I; printf("return 0;\n}\n");
	break;

    case '=': I;
	if(bytecell)
	    printf("*p = %d;\n", count);
	else
	    printf("mpz_set_si(*p, %d);\n", count);
	break;

    case 'B': I;
	if (bytecell)
	    printf("v = *p;\n");
	else
	    printf("mpz_set(v, *p);\n");
	break;

    case 'm':
    case 'M': I;
	if (bytecell)
	    printf("*p = *p+v*%d;\n", count);
	else
	    printf("mpz_addmul_ui(*p, v, %d);\n", count);
	break;

    case 'n':
    case 'N': I;
	if (bytecell)
	    printf("*p = *p-v*%d;\n", count);
	else
	    printf("mpz_submul_ui(*p, v, %d);\n", count);
	break;

    case 's':
    case 'S': I;
	if (bytecell)
	    printf("*p = *p+v;\n");
	else
	    printf("mpz_add(*p, *p, v);\n");
	break;

    case 'Q': I;
	if (bytecell)
	    printf("if(v) *p = %d;\n", count);
	else
	    printf("if(mpz_cmp_ui(v, 0)) mpz_set_si(*p, %d);\n", count);
	break;

    case 'X': I; printf("fprintf(stderr, \"Abort: Infinite Loop.\\n\"); exit(1);\n"); break;

    case '+': I;
	if (bytecell)
	    printf("*p += %d;\n", count);
	else
	    printf("mpz_add_ui(*p, *p, %d);\n", count);
	break;

    case '-': I;
	if (bytecell)
	    printf("*p -= %d;\n", count);
	else
	    printf("mpz_sub_ui(*p, *p, %d);\n", count);
	break;

    case '<':
	I; printf("p = move_ptr(p, %d);\n", -count);
	break;

    case '>':
	I; printf("p = move_ptr(p, %d);\n", count);
	break;

    case '[':
	I;
	if(bytecell)
	    printf("while(*p){\n");
	else
	    printf("while(mpz_cmp_ui(*p, 0)){\n");
	ind++;
	break;

    case ']':
	ind--; I; printf("}\n");
	break;

    case '.': I;
	if (bytecell)
	    printf("putchar(*p);\n");
	else
	    printf("putchar(mpz_get_ui(*p));\n");
	break;

    case '"': print_cstring(); break;

    case ',':
	I; printf("c = getchar();\n");
	I; printf("if (c != EOF)\n");
	ind++; I; ind--;
	if (bytecell)
	    printf("*p = c;\n");
	else
	    printf("mpz_set_si(*p, (long)c);\n");
	break;
    }
}

static void
print_cstring(void)
{
    char * str = get_string();
    char buf[256];
    int gotnl = 0, gotperc = 0;
    size_t outlen = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || gotnl || outlen > sizeof(buf)-8))
	{
	    buf[outlen] = 0;
	    if (gotnl) {
		buf[outlen-2] = 0;
		prv("puts(\"%s\");", buf);
	    } else if (gotperc)
		prv("printf(\"%%s\",\"%s\");", buf);
	    else
		prv("printf(\"%s\");", buf);
	    gotnl = gotperc = 0; outlen = 0;
	}
	if (!*str) break;

	if (*str == '\n') gotnl = 1;
	if (*str == '%') gotperc = 1;
	if (*str >= ' ' && *str <= '~' && *str != '"' && *str != '\\') {
	    buf[outlen++] = *str;
	} else if (*str == '"' || *str == '\\') {
	    buf[outlen++] = '\\'; buf[outlen++] = *str;
	} else if (*str == '\n') {
	    buf[outlen++] = '\\'; buf[outlen++] = 'n';
	} else if (*str == '\t') {
	    buf[outlen++] = '\\'; buf[outlen++] = 't';
	} else {
	    char buf2[16];
	    int n;
	    sprintf(buf2, "\\%03o", *str & 0xFF);
	    for(n=0; buf2[n]; n++)
		buf[outlen++] = buf2[n];
	}
    }
}

