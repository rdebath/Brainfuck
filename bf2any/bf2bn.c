#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * OpenSSL Bignum translation from BF, runs at about 800,000,000 instructions per second.
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
 * The tape on this one is unusual because the cells have to be init'd before
 * they are used. To allow this I've used a slower 'realloc' method to store
 * the tape with position checks on pointer movement.
 */

static int ind = 0;
#define I        printf("%*s", ind*4, "")
#define prv(s,v) printf("%*s" s "\n", ind*4, "", (v))

static void print_cstring(void);
static int use_macro = 0;
static int bpc = 0;

static check_arg_t fn_check_arg;
struct be_interface_s be_interface = {.check_arg=fn_check_arg,.nobytecell=1};

static int
fn_check_arg(const char * arg)
{
    if (strcmp(arg, "-mac") == 0) {
	use_macro = 1;
	return 1;
    }
    if (strncmp(arg, "-b", 2) == 0 && arg[2] > '0' && arg[2] <= '9') {
        bpc = strtol(arg+2, 0, 10);
	if (bpc < 32) {
	    fprintf(stderr, "Cell size must be a minimum of 32 bits\n");
	    exit(1);
	}
        return 1;
    }
    if (strcmp("-h", arg) ==0) {
        fprintf(stderr, "%s\n",
        "\t"    "-b256   Limit to <N> bits per cell (Must be >=32 bits)"
	);
	return 1;
    }

    return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	printf("#include <stdlib.h>\n"
	       "#include <stdio.h>\n"
	       "#include <string.h>\n"
	       "#include <openssl/bn.h>\n");
	puts("");
	printf("BIGNUM ** mem = 0;\n");
	printf("int memsize = 0;\n");
	printf("#define MINALLOC 16\n");
	puts("");

	printf("#ifndef BPC\n");
	printf("#define BPC %d\n", bpc);
	printf("#endif\n");
	printf("#if BPC > 0\n");
	printf("#define BN_mask_BPC(c) BN_mask_bits(c, BPC)\n");
	printf("#else\n");
	printf("#define BN_mask_BPC(c)\n");
	printf("#endif\n");
	puts("");

	printf("%s",
	    "static\n"
	    "BIGNUM **\n"
	    "alloc_ptr(BIGNUM **p)\n"
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
	    "            mem[i] = BN_new();\n"
	    "        memoff += amt;\n"
	    "    } else {\n"
	    "        for(i=0; i<amt; i++)\n"
	    "            mem[memsize+i] = BN_new();\n"
	    "    }\n"
	    "    memsize += amt;\n"
	    "    return mem+memoff;\n"
	    "}\n"
	);
	puts("");

	/* BN is a bit weird about getting ints from a bignum, so while I'm
	 * messing about here I'll stick in a UTF-8 encoder too.
	 */
	printf("%s\n",
		"void"
	"\n"	"putchar8(BIGNUM * chr)"
	"\n"	"{"
	"\n"	"    int input_chr;"
	"\n"	"    BIGNUM * t2 = BN_new();"
	"\n"	"    input_chr = BN_get_word(chr);"
	"\n"	"    BN_zero(t2);"
	"\n"	"    if(BN_cmp(chr,t2) < 0) input_chr = -(input_chr&255);"
	"\n"	"    BN_free(t2);"
	"\n"
	"\n"	"    if (input_chr < 0x80) {            /* one-byte char */"
	"\n"	"        putchar(input_chr);"
	"\n"	"    } else if (input_chr < 0x800) {    /* two-byte char */"
	"\n"	"        putchar(0xC0 | (0x1F & (input_chr >>  6)));"
	"\n"	"        putchar(0x80 | (0x3F & (input_chr      )));"
	"\n"	"    } else if (input_chr < 0x10000) {  /* three-byte char */"
	"\n"	"        putchar(0xE0 | (0x0F & (input_chr >> 12)));"
	"\n"	"        putchar(0x80 | (0x3F & (input_chr >>  6)));"
	"\n"	"        putchar(0x80 | (0x3F & (input_chr      )));"
	"\n"	"    } else if (input_chr < 0x200000) { /* four-byte char */"
	"\n"	"        putchar(0xF0 | (0x07 & (input_chr >> 18)));"
	"\n"	"        putchar(0x80 | (0x3F & (input_chr >> 12)));"
	"\n"	"        putchar(0x80 | (0x3F & (input_chr >>  6)));"
	"\n"	"        putchar(0x80 | (0x3F & (input_chr      )));"
	"\n"	"    }"
	"\n"	"}"
	);

	puts("");

	if (!use_macro) {
	    /* #include rant about different semantics of inline keyword */
	    printf("%s",
		"static inline\n"
		"BIGNUM **\n"
		"move_ptr(BIGNUM **p, int off) {\n"
		"    p += off;\n"
		"    if ((off>0 || p >= mem) && (off<0 || p < mem+memsize)) return p;\n"
		"    return alloc_ptr(p);\n"
		"}\n"
	    );
	} else {
	    printf("%s",
		"#define move_ptr(P,O) (((P)+=(O), \\\n"
		"        ((O)>=0 && (P)>=mem+memsize) || \\\n"
		"        ((O)<=0 && (P)<mem) ) ? \\\n"
		"            alloc_ptr(P): (P) )\n"
	    );
	}

	puts("");

	printf("int\n"
	       "main(int argc, char ** argv)\n"
	       "{\n"
	    );
	ind ++;
	I; printf("register BIGNUM ** p;\n");
	I; printf("register int c;\n");
	I; printf("BIGNUM *v = BN_new();\n");
	I; printf("BIGNUM *t1 = BN_new();\n");
	I; printf("setbuf(stdout, 0);\n");
	I; printf("p = move_ptr(mem,0);\n");
	break;

    case '~': I; printf("return 0;\n}\n"); break;

    case '=':
	if (count > 0) {
	    I; printf("BN_set_word(*p, %d);\n", count);
	} else if (count <= 0) {
	    I; printf("BN_zero(*p);\n");
	    if (count < 0) {
		I; printf("BN_sub_word(*p, %d);\n", -count);
	    }
	}
	break;

    case 'B':
	I; printf("BN_mask_BPC(*p);\n");
	I; printf("BN_copy(v, *p);\n");
	break;

    case 'm':
    case 'M':
	if (count != 1) {
	    I; printf("BN_copy(t1, v);\n");
	    I; printf("BN_mul_word(t1, %d);\n", count);
	    I; printf("BN_add(*p, *p, t1);\n");
	} else {
	    I; printf("BN_add(*p, *p, v);\n");
	}
	break;

    case 'n':
    case 'N':
	if (count != 1) {
	    I; printf("BN_copy(t1, v);\n");
	    I; printf("BN_mul_word(t1, %d);\n", count);
	    I; printf("BN_sub(*p, *p, t1);\n");
	} else {
	    I; printf("BN_sub(*p, *p, v);\n");
	}
	break;

    case 's':
    case 'S': I; printf("BN_add(*p, *p, v);\n"); break;

    case 'Q':
	I; printf("if(!BN_is_zero(v)) {\n");
	ind++;
	if (count > 0) {
	    I; printf("BN_set_word(*p, %d);\n", count);
	} else if (count <= 0) {
	    I; printf("BN_zero(*p);\n");
	    if (count < 0) {
		I; printf("BN_sub_word(*p, %d);\n", -count);
	    }
	}
	ind--;
	I; printf("}\n");
	break;

    case 'X': I; printf("fprintf(stderr, \"Abort: Infinite Loop.\\n\"); exit(1);\n"); break;

    case '+': I; printf("BN_add_word(*p, %d);\n", count); break;
    case '-': I; printf("BN_sub_word(*p, %d);\n", count); break;
    case '<': I; printf("p = move_ptr(p, %d);\n", -count); break;
    case '>': I; printf("p = move_ptr(p, %d);\n", count); break;

    case '[':
	I; printf("BN_mask_BPC(*p);\n");
	I; printf("while(!BN_is_zero(*p)){\n");
	ind++;
	break;

    case ']':
	I; printf("BN_mask_BPC(*p);\n");
	ind--;
	I; printf("}\n");
	break;

    case 'I':
	I; printf("BN_mask_BPC(*p);\n");
	I; printf("if(!BN_is_zero(*p)){\n");
	ind++;
	break;

    case 'E':
	ind--;
	I; printf("}\n");
	break;

    case '.': I; printf("putchar8(*p);\n"); break;

    case '"': print_cstring(); break;
    case ',':
	I; printf("c = getchar();\n");
	I; printf("if (c != EOF)\n");
	ind++;
	I; printf("BN_set_word(*p, c);\n");
	ind--;
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
