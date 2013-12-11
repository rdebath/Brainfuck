#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
 * C GMP translation from BF, runs at about 590,000,000 instructions per second.
 *
 * Note: This accepts the optimise flag, but strictly speaking the
 * optimisations may not be valid for bignum cells. Of course in
 * the real world bignums do have a limited range and an optimisation
 * that assumes that they wrap is not actually wrong.
 *
 * It isn't really wrong for true infinities either as plus and minus infinity
 * don't really obey the normal rules.
 *
 * And finally, the result if a bignum implemetation is given an infinite job
 * is actually undefined, giving the 'right' result isn't actually wrong and
 * if you want it to crash all you have to do is turn off optimisation.
 *
 * If this is configured with byte cells the gmp library is not called.
 *
 * This on is unusual for me in that the tape is a linked list, this means I
 * have to make the tape infinite in both directions because the tape offset
 * will be checked before the condition on the 'm', 'n' and 's' tokens.
 * Therefor there's no point having distinct 'M' vs 'm' tokens.
 */

extern int bytecell;

int do_input = 0;
int ind = 0;
#define I printf("%*s", ind*4, "")

int
check_arg(char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	printf("#include <stdlib.h>\n"
	       "#include <stdio.h>\n");
	if (!bytecell)
	    printf("#include <gmp.h>\n");
	printf("\n");
	if (bytecell)
	    printf("struct mem { struct mem *p, *n; unsigned char v; };\n");
	else
	    printf("struct mem { struct mem *p, *n; mpz_t v; };\n");
	printf("\n");

	printf("void new_n(struct mem *p) {\n");
	printf("    p->n = calloc(1, sizeof*p);\n");
	printf("    p->n->p = p;\n");
	if (!bytecell) {
	    printf("    p = p->n;\n");
	    printf("    mpz_init(p->v);\n");
	}
	printf("}\n\n");

	printf("void new_p(struct mem *p) {\n");
	printf("    p->p = calloc(1, sizeof*p);\n");
	printf("    p->p->n = p;\n");
	if (!bytecell) {
	    printf("    p = p->p;\n");
	    printf("    mpz_init(p->v);\n");
	}
	printf("}\n\n");

	printf("int\n"
	       "main(int argc, char ** argv)\n"
	       "{\n"
	    );
	ind ++;
	I; printf("register struct mem * p;\n");
	I; printf("register int c;\n");
	if (bytecell) {
	    I; printf("register int v;\n");
	} else {
	    I; printf("mpz_t v;\n");
	    I; printf("mpz_init(v);\n");
	}
	I; printf("setbuf(stdout, 0);\n");
	I; printf("p = calloc(1, sizeof*p);\n");
	if (!bytecell) {
	    I; printf("mpz_init(p->v);\n");
	}
	break;

    case '~':
	I; printf("return 0;\n}\n");
	break;

    case '=': I;
	if(bytecell)
	    printf("p->v = %d;\n", count);
	else
	    printf("mpz_set_si(p->v, %d);\n", count);
	break;

    case 'B': I;
	if (bytecell)
	    printf("v = p->v;\n");
	else
	    printf("mpz_set(v, p->v);\n");
	break;

    case 'm':
    case 'M': I;
	if (bytecell)
	    printf("p->v = p->v+v*%d;\n", count);
	else
	    printf("mpz_addmul_ui(p->v, v, %d);\n", count);
	break;

    case 'n':
    case 'N': I;
	if (bytecell)
	    printf("p->v = p->v-v*%d;\n", count);
	else
	    printf("mpz_submul_ui(p->v, v, %d);\n", count);
	break;

    case 's':
    case 'S': I;
	if (bytecell)
	    printf("p->v = p->v+v;\n");
	else
	    printf("mpz_add(p->v, p->v, v);\n");
	break;

    case 'Q': I;
	if (bytecell)
	    printf("if(v) p->v = %d;\n", count);
	else
	    printf("if(mpz_cmp_ui(v, 0)) mpz_set_si(p->v, %d);\n", count);
	break;

    case 'X': I; printf("fprintf(stderr, \"Abort: Infinite Loop.\\n\"); exit(1);\n"); break;

    case '+': I;
	if (bytecell)
	    printf("p->v += %d;\n", count);
	else
	    printf("mpz_add_ui(p->v, p->v, %d);\n", count);
	break;

    case '-': I;
	if (bytecell)
	    printf("p->v -= %d;\n", count);
	else
	    printf("mpz_sub_ui(p->v, p->v, %d);\n", count);
	break;

    case '<':
	if (count > 1) {
	    I; printf("for(c=0; c<%d; c++) {\n", count);
	    ind++;
	}
	I; printf("if (p->p == 0) new_p(p);\n");
	I; printf("p=p->p;\n");
	if (count > 1) {
	    ind--;
	    I; printf("}\n");
	}
	break;

    case '>':
	if (count > 1) {
	    I; printf("for(c=0; c<%d; c++) {\n", count);
	    ind++;
	}
	I; printf("if (p->n == 0) new_n(p);\n");
	I; printf("p=p->n;\n");
	if (count > 1) {
	    ind--;
	    I; printf("}\n");
	}
	break;

    case '[':
	I;
	if(bytecell)
	    printf("while(p->v){\n");
	else
	    printf("while(mpz_cmp_ui(p->v, 0)){\n");
	ind++;
	break;

    case ']':
	ind--; I; printf("}\n");
	break;

    case '.': I;
	if (bytecell)
	    printf("putchar(p->v);\n");
	else
	    printf("putchar(mpz_get_ui(p->v));\n");
	break;

    case ',':
	I; printf("c = getchar();\n");
	I; printf("if (c != EOF)\n");
	ind++; I; ind--;
	if (bytecell)
	    printf("p->v = c;\n");
	else
	    printf("mpz_set_si(p->v, (long)c);\n");
	break;
    }
}
