#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#if _POSIX_VERSION < 200112L && _XOPEN_VERSION < 500
#define NO_SNPRINTF
#warning Using sprintf not snprintf
#endif

#include "bf2any.h"

static check_arg_t fn_check_arg;
static gen_code_t gen_code;
struct be_interface_s be_interface = {
    .check_arg = fn_check_arg,
    .gen_code = gen_code,
    .disable_be_optim=1,
    .hasdebug=1
};

/*
 * BF translation to BF. This isn't an identity translation because even
 * without most of the peephole optimisation the loader will still remove
 * sequences that are cancelling or that can never be run because they
 * begin with the '][' comment loop introducer.
 *
 * Then there are all the variants.
 *
 * Some of these also generate a set of C #define lines so the output
 * is compilable as C.
 */

/* Language classes */

#define C_HEADERS       0x100   /* Add C Header & footer */
#define C_DEFINES       0x200   /* #defines in the header */
#define C_NUMRLE        0x3000  /* RLE using %d in <>+- */
#define C_ADDRLE        0x2000  /* RLE using %d in +- */
#define C_MOVRLE        0x1000  /* RLE using %d in <> */

#define L_BASE          (0xFF & langclass)

#define L_WORDS         0       /* Words with spaces */

#define L_CWORDS        (L_JNWORD+C_HEADERS)
#define L_CDWORDS       (L_WORDS+C_HEADERS+C_DEFINES)

#define L_JNWORD        1       /* Words NO spaces */
#define L_CHARS         2       /* Add strings char by char. */
#define L_BF            3       /* Generated code is BF. */
#define L_BFCHARS       4       /* BF like code that needs mov_opt */
#define L_LINES         5       /* Code is not wordwrapped. */

#define L_TOKENS        0x10    /* token(token, count); */
#define L_RISBF         0x11    /* risbf(token, count); */
#define L_TINYBF        0x12    /* tinybf(token, count); */
#define L_HEADSECKS     0x13    /* headsecks(token, count); */
#define L_BFRLE         0x14    /* bfrle(token, count); */
#define L_BFXML         0x15    /* bfxml(token, count); */
#define L_UGLYBF        0x16    /* bfugly(token, count); */
#define L_MALBRAIN      0x17    /* malbrain(token, count); */
#define L_HANOILOVE     0x18    /* hanoilove(token, count); */
#define L_DOWHILE       0x19    /* bfdowhile(token, count); */
#define L_BINRLE        0x1A    /* bfbinrle(token, count); */
#define L_QQQ           0x1E    /* qqq(token, count); */

static const char bf[] = "><+-.,[]";

typedef struct {
    const char * name;
    const char * bf[8];
    const char * gen_hdr, * gen_foot;
    const char * wrap_eight_bit;
    const char * clean_tmps;
    const char * set_cell;
    const char * zero_cell;
    const char * repeat_prev;
    const char * name2;
    const char * help;
    int class, multi, bytesonly;
} trivbf;

static trivbf * trivlist[];
static trivbf bfout[], doubler_copy[], doubler_12[], bfquadz[];
static trivbf cbyte[], cint[], cbyte_rle[], cint_rle[];
static trivbf new_triv[1];

struct instruction { int ch; int count; struct instruction * next, *loop; };

static int langclass = L_BF;
static trivbf * lang = bfout;
static trivbf * c = 0;
static int linefix = EOF;
static int col = 0;
static int maxcol = -1;
static int state = 0;
static int bf_mov = 0;
static int enable_bf_mov = 0;
static int disable_optim = 0;

static int headsecksconv[] = {3, 2, 0, 1, 4, 5, 6, 7 };

static int bf_multi = 0, tmp_clean = 0, bf_space = 0;
static struct instruction *pgm = 0, *last = 0;

/* Default double and quad to the easiest to prove algorithms. */
static trivbf * doubler = doubler_copy;
static trivbf * bfquad = bfquadz;

static void set_triv_out(trivbf *, int, const char *);
static void generic_output(int ch, int count);
static void risbf(int ch, int count);
static void tinybf(int ch, int count);
static void headsecks(int ch, int count);
static void bfrle(int ch, int count);
static void bfxml(int ch, int count);
static void bfugly(int ch, int count);
static void malbrain(int ch, int count);
static void hanoilove(int ch, int count);
static void qqq(int ch, int count);
static void bfdowhile(int ch, int count);
static void bfbinrle(int ch, int count);
static void bftranslate(int ch, int count);
static void bfreprint(void);
static void bfpackprint(void);

static int
fn_check_arg(const char * arg)
{
    if (strcmp(arg, "+init") == 0) {
	if (maxcol < 0) maxcol = 72;

	if (lang && lang->bytesonly)
	    be_interface.bytesonly = 1;

	if (!lang || !lang->repeat_prev) {
	    if (bytecell) c = cbyte; else c = cint;
	} else {
	    if (bytecell) c = cbyte_rle; else c = cint_rle;
	}
	if (lang == cbyte) lang = c;

	if (!disable_optim) {
	    /* This enables BF style optimisation of '<' and '>' on OUTPUT */
	    if (L_BASE == L_BFCHARS || L_BASE == L_DOWHILE)
		enable_bf_mov = 1;
	    else if (L_BASE == L_BF && bf_multi > 1)
		enable_bf_mov = 1;
	}

	if (! ( L_BASE == L_BFRLE || L_BASE == L_HANOILOVE ||
		L_BASE == L_BINRLE || L_BASE == L_BFXML ||
		L_BASE == L_TOKENS || (langclass & C_ADDRLE) == C_ADDRLE))
	{
	    be_interface.disable_fe_optim = 1;
	}

	return 1;
    }

    if (strcmp(arg, "-#") == 0) return 1;

    if (strcmp(arg, "-c") == 0) {
	lang = cbyte; langclass = L_CWORDS; return 1;
    } else
    if (strcmp(arg, "-m") == 0) {
	disable_optim = 1; return 1;
    } else

    if (strcmp(arg, "-db") == 0 || strcmp(arg, "-double") == 0) {
	bf_multi |= 2;
	lang = doubler; langclass = L_BF; return 1;
    } else
    if (strcmp(arg, "-quad") == 0) {
	bf_multi |= 4;
	lang = bfquad; langclass = L_BF; return 1;
    } else
    if (strcmp(arg, "-multi") == 0) {
	bf_multi |= 7;
	lang = bfout; langclass = L_BF; return 1;
    } else
    if (strcmp(arg, "-dbr") == 0) {
	bf_multi |= 2 + 8;
	lang = doubler = doubler_12; langclass = L_BF; return 1;
    } else
    if (strcmp(arg, "-multi+") == 0) {
	bf_multi |= 7;
	bf_space = 1;
	lang = bfout; langclass = L_BF; return 1;
    } else

    if (strcmp(arg, "-head") == 0) {
	lang = 0; langclass = L_HEADSECKS; return 1;
    } else
    if (strcmp(arg, "-bfrle") == 0) {
	lang = 0; langclass = L_BFRLE; return 1;
    } else
    if (strcmp(arg, "-xml") == 0) {
	lang = 0; langclass = L_BFXML; return 1;
    } else
    if (strcmp(arg, "-uglybf") == 0) {
	lang = 0; langclass = L_UGLYBF; return 1;
    } else
    if (strcmp(arg, "-risbf") == 0) {
	lang = 0; langclass = L_RISBF; return 1;
    } else
    if (strcmp(arg, "-tinybf") == 0) {
	lang = 0; langclass = L_TINYBF; return 1;
    } else
    if (strcmp(arg, "-malbrain") == 0) {
	lang = 0; langclass = L_MALBRAIN; return 1;
    } else
    if (strcmp(arg, "-hanoilove") == 0) {
	lang = 0; langclass = L_HANOILOVE; return 1;
    } else
    if (strcmp(arg, "-dowhile") == 0) {
	lang = 0; langclass = L_DOWHILE; return 1;
    } else
    if (strcmp(arg, "-binrle") == 0) {
	lang = 0; langclass = L_BINRLE; return 1;
    } else
    if (strcmp(arg, "-dump") == 0) {
	lang = 0; langclass = L_TOKENS; return 1;
    } else
    if (strcmp(arg, "-qqq") == 0 || strcmp(arg, "-???") == 0) {
	lang = 0; langclass = L_QQQ; return 1;
    } else

    if (strcmp(arg, "-w") == 0) {
	maxcol = 0;
	return 1;
    } else
    if (strncmp(arg, "-w", 2) == 0 && arg[2] >= '0' && arg[2] <= '9') {
	maxcol = atol(arg+2);
	return 1;
    } else
    if (strcmp("-h", arg) ==0) {
	int i,j;
	fprintf(stderr, "%s\n",
	"\t"    "-w99    Width to line wrap at, default 72"
	"\n"
	"\n\t"  "-dump      Token dump"
	"\n\t"  "-head      Headsecks."
	"\n\t"  "-bfrle     Convert to BF RLE as used by -R."
	"\n\t"  "-xml       XML"
	"\n\t"  "-uglybf    Ugly BF"
	"\n\t"  "-risbf     RISBF"
	"\n\t"  "-tinybf    TINYBF"
	"\n\t"  "-malbrain  Malbrain translation"
	"\n\t"  "-hanoilove Hanoi Love translation"
	"\n\t"  "-dowhile   Do ... while translataion."
	"\n\t"  "-binrle    Base 2 RLE BF"
	"\n\t"  "-???       https://esolangs.org/wiki/%3F%3F%3F"
	"\n\t"  "-dbr       Like -dbl12nz with RLE of '+' and '-'."
	"\n\t"  "-multi     Combined single, double and quad with cell size."
	);
	for(i=0; trivlist[i]; i++) {
	    if (trivlist[i]->name && trivlist[i]->help)
		fprintf(stderr, "\t-%-9s %s\n",
			trivlist[i]->name, trivlist[i]->help);
	}
	fprintf(stderr, "    And also:\n");
	for(i=j=0; trivlist[i]; i++) {
	    if (trivlist[i]->name && !trivlist[i]->help) {
		if (j==0)
		    fprintf(stderr, "\t");
		else
		    fprintf(stderr, ", ");
		fprintf(stderr, "-%s", trivlist[i]->name);
		if (j==5) {
		    fprintf(stderr, "\n");
		    j=0;
		} else j++;
	    }
	}
	if (j) fprintf(stderr, "\n");
	return 1;
    } else
    if (strncmp(arg, "-triv=", 6) == 0) {
	set_triv_out(new_triv, 0, arg+6);
	return 1;
    } else
    if (strncmp(arg, "-trivj=", 7) == 0) {
	set_triv_out(new_triv, 1, arg+7);
	return 1;
    } else if (arg[0] == '-') {
	int i;
	for(i=0; trivlist[i]; i++) {
	    if ((trivlist[i]->name && !strcmp(trivlist[i]->name, arg+1)) ||
	        (trivlist[i]->name2 && !strcmp(trivlist[i]->name2, arg+1))) {
		lang = trivlist[i];
		langclass = lang->class;
		if (lang->multi) {
		    bf_multi |= lang->multi;
		    switch(lang->multi) {
		    case 2: doubler = lang ; break;
		    case 4: bfquad = lang; break;
		    }
		}
		return 1;
	    }
	}
	return 0;
    } else
	return 0;
}

static void
pc(int ch)
{
    if (enable_bf_mov) {
	if (ch == '>') bf_mov++;
	else if (ch == '<') bf_mov--;
	else {
	    enable_bf_mov = 0;
	    while (bf_mov>0) {bf_mov--; pc('>'); }
	    while (bf_mov<0) {bf_mov++; pc('<'); }
	    pc(ch);
	    enable_bf_mov = 1;
	}
	return;
    }

    if ((col>=maxcol && maxcol) || ch == '\n') {
	if (linefix != EOF) putchar(linefix);
	putchar('\n');
	col = 0;
	if (ch == ' ' || ch == '\n') ch = 0;
    }
    if (ch) {
	putchar(ch);
	if ((ch&0xC0) != 0x80) /* Count UTF-8 */
	    col++;
    }
}

static void
ps(const char * s)
{
    const char * p = s;
    int l = 0;
    if (L_BASE == L_LINES) { printf("%s", s); return; }

    while (*s) {
	if (*p == ' ' && p == s) {
	    s = ++p; l = 0;
	} else if (*p == ' ' || *p == 0) {
	    int sp = (L_BASE == L_WORDS && col != 0);
	    if (col+l+sp > maxcol && maxcol) {
		pc('\n');
		sp = 0;
	    }
	    if (sp) pc(' ');
	    printf("%.*s", (int)(p-s), s);
	    if (*p) p++;
	    col += l; s = p; l = 0;
	} else if ((*p&0xC0) != 0x80) {
	    /* Count UTF-8 codepoints. This is easy, but actually wrong */
	    l++;
	    /* So we add a tiny fix to make Chinese characters double width.
	     * It's still not right, but no longer quite as wrong */
	    if (((*p&0xFF) >= 0xE3 && (*p&0xFF) <= 0xEC) || (*p&0xFF) == 0xF0)
		l++;
	    p++;
	} else
	    p++;
    }
}

static void
pmc(const char * s)
{
    while (*s) pc(*s++);
}

static void
gen_code(int ch, int count, char * strn)
{
    if (L_BASE != L_TOKENS && L_BASE != L_BFRLE &&
	L_BASE != L_HANOILOVE && L_BASE != L_BINRLE)
    {
	if (ch == '=') {
	    if (lang==0 || (lang->set_cell==0 && lang->zero_cell == 0)) {
		gen_code('[', 1, 0);
		gen_code('-', 1, 0);
		gen_code(']', 1, 0);
		if (count>0) gen_code('+', count, 0);
		else if(count<0) gen_code('-', -count, 0);
		return;
	    } else if (lang->set_cell)
		;
	    else if(count) {
		gen_code('=', 0, 0);
		if (count>0) ch = '+';
		else { ch = '-'; count = -count; }
	    }
	}

	if (ch == '[' || ch == ']' || ch == '.' || ch == ',')
	    count = 1;
    }

    switch (L_BASE) {
    case L_WORDS:
    case L_JNWORD:
    case L_LINES:
    case L_CHARS:
    case L_BFCHARS:     generic_output(ch, count); break;

    case L_TOKENS:
	if (ch == '"' && strn) {
	    char * s = strn;
	    putchar('"');
	    for(;*s; s++)
		printf(" %d", *s & 0xFF);
	    putchar('\n');
	} else
	    printf("%c %d\n", ch, count);
	break;

    case L_RISBF:	risbf(ch, count); break;
    case L_TINYBF:	tinybf(ch, count); break;
    case L_HEADSECKS:	headsecks(ch, count); break;
    case L_BF:		bftranslate(ch, count); break;
    case L_BFRLE:	bfrle(ch, count); break;
    case L_BFXML:	bfxml(ch, count); break;
    case L_UGLYBF:	bfugly(ch, count); break;
    case L_MALBRAIN:	malbrain(ch, count); break;
    case L_HANOILOVE:	hanoilove(ch, count); break;
    case L_DOWHILE:	bfdowhile(ch, count); break;
    case L_BINRLE:	bfbinrle(ch, count); break;
    case L_QQQ:		qqq(ch, count); break;
    }

    if (ch == '~' && col) pc('\n');
}

void
generic_output(int ch, int count)
{
    if (ch == '!' && (langclass & C_HEADERS) != 0) {
	int i,j;
	if (bytecell)
	    printf("#include<unistd.h>\n");
	else
	    printf("#include<stdio.h>\n");
	if (lang && (langclass & C_DEFINES)) {
	    for (j=0; j<8; j++){
		i=(14-j)%8;
		printf("#define %s %s\n", lang->bf[i], c->bf[i]);
	    }
	    if (lang->repeat_prev) {
		printf("#define %s %s\n", lang->repeat_prev, c->repeat_prev);
		printf("#define _ ;return 0;}\n");
	    } else
		printf("#define _ return 0;}\n");
	}
	if (bytecell)
	    printf("char mem[%d];int main(){register char*m=mem;\n", tapesz);
	else
	    printf("int mem[%d];int main(){register int*m=mem;\n", tapesz);
	if (tapeinit)
	    printf("m += %d;\n", tapeinit);
    }

    if (ch == '!' && lang && lang->gen_hdr) ps(lang->gen_hdr);

    switch (L_BASE) {
    case L_WORDS:
    case L_JNWORD:
    case L_LINES:
	{
	    int v;
	    char * p;
	    if (!(p = strchr(bf,ch))) {
		if (ch != '=') break;
		if (lang->set_cell && (count != 0 || !lang->zero_cell)) {
		    char sbuf[256];
#ifndef NO_SNPRINTF
		    snprintf(sbuf, sizeof(sbuf), lang->set_cell, count);
#else
		    sprintf(sbuf, lang->set_cell, count);
#endif
		    ps(sbuf);
		} else
		    ps(lang->zero_cell);
		break;
	    }
	    v = p-bf;
	    if ((v>=0 && v<2 && (langclass & C_MOVRLE)) ||
	        (v>=2 && v<4 && (langclass & C_ADDRLE))) {
		char sbuf[256];
#ifndef NO_SNPRINTF
		snprintf(sbuf, sizeof(sbuf), lang->bf[v], count);
#else
		sprintf(sbuf, lang->bf[v], count);
#endif
		ps(sbuf);
		break;
	    }
	    if (lang->wrap_eight_bit && bytecell && (ch == '[' || ch == ']'))
		ps(lang->wrap_eight_bit);

	    while(count-->0){
		ps(lang->bf[v]);
		if (lang->repeat_prev)
		    while(count-->0) ps(lang->repeat_prev);
	    }
	}
	break;

    case L_CHARS:
    case L_BFCHARS:
	{
	    char * p;
	    if (ch == '=') pmc(lang->zero_cell);
	    if (!(p = strchr(bf,ch))) break;
	    while(count-->0) pmc(lang->bf[p-bf]);
	}
	break;
    }

    if (ch == '~' && lang && lang->gen_foot) ps(lang->gen_foot);

    if (ch == '~') {
	pc(0);
	if (langclass & C_DEFINES)
	    col += printf("%s%s", col?" ":"", "_");
	else if (langclass & C_HEADERS)
	    col += printf("%s%s", col?" ":"", "return 0;}");
    }
}

static void
set_triv_out(trivbf * l, int opt_flg, const char * arg)
{
    int i;
    char *mut, *s, sep;
    const char *ap;
    if (l->name == 0) {
	l->name = "built";
	l->class = L_WORDS;
	for (i=0; i<8; i++)
	    l->bf[i] = bfout->bf[i];
    }

    switch(opt_flg) {
    case 0: l->class = L_WORDS; break;
    case 1: l->class = L_JNWORD; break;
    }

    sep = '\001';
    s = mut = malloc(strlen(arg)+2);
    *s = 0;
    for(ap = arg;*ap;ap++) {
	if (*ap == ',') *s++ = sep;
	else if (*ap == sep) ;
	else if (*ap != '\\') *s++ = *ap;
	else if (ap[1]) {
	    ap++; *s++ = *ap;
	}
	*s = 0;
    }

    s = mut;
    for(i=0; i<8; i++) {
	char * p = strchr(s, sep);
	if (p) *p = 0;
	if (*s)
	    l->bf[i] = strdup(s);
	if (p) s = p+1; else break;
    }
    free(mut);

    lang = l;
    langclass = l->class;
    bf_multi = l->multi;
}

/******************************************************************************
 *
 *  This will output multiple copies of the input code with auto detection
 *  of the bit sizes. The autodetect is VERY easy to prove by static analysis
 *  so, in theory, only one of them should make it into the final executable.
 *
 *  NB: The bf2const routines are capable of this.
 *
 *  In addition the temps required by the double and quad conversions
 *  are explicitly zeroed after each pointer movement. This should also
 *  aid more complex forms of static analysis.
 *
 ******************************************************************************/
static void
bftranslate(int ch, int count)
{
    char * p;
    if ((p = strchr(bf,ch)) || ch == '=' || ch == '#') {
	if (bf_multi) {
	    struct instruction * n = calloc(1, sizeof*n);
	    if (!n) { perror("bf2multi"); exit(42); }

	    n->ch = ch;
	    n->count = count;
	    if (!last) pgm = n; else last->next = n;
	    last = n;
	} else if (p) {
	    if (ch == '>' || ch == '<') tmp_clean = 0;
	    else if (ch == '.' || ch == ',') ;
	    else if (!tmp_clean && lang->clean_tmps) {
		pmc(lang->clean_tmps); tmp_clean = 1;
	    }
	    while(count-->0) pmc(lang->bf[p-bf]);
	} else {
	    if (ch == '#')
		pmc("\n#\n");
	    else if (ch == '=') {
		pmc(lang->zero_cell);
		tmp_clean = 1;
	    } else
		pc(ch);
	}
	return;
    }

    if (ch == '!') {
	if (bf_multi == 1 || bf_multi == 2 || bf_multi == 4)
	    bf_multi = 0;
    }

    if (ch == '~' && bf_multi) {
	/* Note: All these cell size checks assume the cell size, if limited,
	 * is a power of two. The three below are the normal sizes, anything
	 * over 65536 is assumed to be large enough.
	 *
	 * The tests can be reordered and the compacted lump in the first
	 * section can be manually replaced by the original code if wanted.
	 */
	if ((bf_multi & 6) == 6) {
	    /* This generates 65536 to check for larger than 16bit cells. */
	    pc(0); puts("// This generates 65536 to check for larger than 16bit cells");
	    pmc("[-]>[-]++[<++++++++>-]<[>++++++++<-]>[<++++++++>-]<[>++++++++<-]>[<++++++++>-]<[");
	    pmc("[-]\n\n");

	    pc(0); puts("// This code may be replaced by the original source");
	    if (bf_space) pmc(">>>");
	    lang = bfout; bfreprint();
	    if (bf_space) pmc("<<<");
	    pc('\n'); puts("// to here");

	    pmc("\n[-]]\n\n");

	    /* I generate 256 and 65536 to test for mid sized cells.
	     * The method used can be optimised all the way to zero by bf2any's
	     * constant folding. NB: bf2any never uses 16bit cells. */
	    pc(0); puts("// This section is cell doubling for 16bit cells");
	    pmc(">[-]>[-]<<[-]++++++++[>++++++++<-]>[<++++>-]<[->+>+<<]");
	    pmc(">[<++++++++>-]<[>++++++++<-]>[<++++>-]>[<+>[-]]<<[>[-]<[-]]>[-<+>]<");

	    pmc("[[-]");
	    pmc("\n\n");

	    if (bf_space) pmc(">>");
	    lang = doubler;
	    if (bf_multi & 8) bfpackprint(); else bfreprint();
	    if (bf_space) pmc("<<");

	    pmc("\n\n");
	    pmc("[-]]\n\n");

	    pc(0); puts("// This section is cell quadrupling for 8bit cells");
	    /* This generates 256 to check for cells upto 8 bits */
	    pmc("[-]>[-]++++++++[<++++++++>-]<[>++++<-]+>[<->[-]]<[[-]");
	    pmc("\n\n");

	    if (bf_space) pmc(">>");
	    lang = bfquad; bfreprint();
	    if (bf_space) pmc("<<");

	    pmc("\n\n");
	    pmc("[-]]");
	} else if ((bf_multi & 0xF) == 10) {
	    bfpackprint();
	} else if ((bf_multi & 7) != 5) {
	    /* The two cell size checks here are independent, they can be
	     * reordered or used on their own.
	     */

	    /* This generates 256 to check for larger than byte cells. */
	    pc(0); puts("// This generates 256 to check for larger than byte cells");
	    pmc(">[-]<[-]++++++++[>++++++++<-]>[<++++>-]");
	    pmc("<[" "[-]\n\n");

	    pc(0); puts("// This code may be replaced by the original source");
	    lang = bfout; bfreprint();
	    pc('\n'); puts("// to here");

	    pmc("\n[-]]\n\n");

	    /* This generates 256 to check for cells upto 8 bits */
	    pc(0); puts("// This code runs on byte sized cells");
	    pmc("[-]>[-]++++++++[<++++++++>-]<[>++++<-]");
	    pmc("+>[<->[-]]<[[-]");

	    pmc("\n\n");

	    lang = doubler;
	    if(bf_multi&8)
		bfpackprint();
	    else
		bfreprint();

	    pmc("\n\n");
	    pmc("[-]]");
	} else {
	    int shortver = 0;
	    /* Eight bit and thirty two bit. */
	    pc(0);
	    puts("// This generates 16777216 to check for larger than 24bit cells");
	    if (shortver) {
		/* This condition is a bit more difficult to optimise and a bit
		 * smaller than the simple multiply lists above.
		 * It checks for binary cells of 24bits or less. */
		pmc("+>>++++++[-<<[->++++<]>[-<++++>]>]<");
		pmc("+<[[-]>[-]<");
		pmc("\n\n");
		lang = bfout;
		bfreprint();
		/* This is an "else" condition, the code cannot be resequenced */
		pmc("\n\n");
		pc(0); puts("// This is an else condition linked to the start of the file");
		pmc(">[-]<[-]]>[[-]<");
		pmc("\n\n");
		lang = bfquad;
		bfreprint();
		pmc("\n\n");
		pmc(">[-]]<");
	    } else {
		pmc("[-]>[-]<");
		pmc("+[>++++++++<-]>[<++++++++>-]<[>++++++++<-]>[<++++++++>-]\n");
		pmc("<[>++++++++<-]>[<++++++++>-]<[>++++++++<-]>[<++++++++>-]");
		pmc("<[[-]");
		pmc("\n\n");
		lang = bfout;
		bfreprint();
		pmc("\n\n");
		pmc("[-]]");
		pmc("\n\n");
		pmc("// This generates 16777216 to check for smaller than 24bit cells\n");
		pmc("[-]>[-]<");
		pmc("+[>++++++++<-]>[<++++++++>-]<[>++++++++<-]>[<++++++++>-]");
		pmc("<[>++++++++<-]>[<++++++++>-]<[>++++++++<-]>[<++++++++>-]");
		pmc("+<[>[-]<[-]]>[[-]<");
		pmc("\n\n");
		lang = bfquad;
		bfreprint();
		pmc("\n\n");
		pmc(">[-]]<");
	    }
	}
    }
}

static void
bfreprint(void)
{
    struct instruction * n = pgm;
    tmp_clean = 0;
    for(; n; n=n->next) {
	int ch = n->ch;
	int count = n->count;
	char * p;
	if ((p = strchr(bf,ch))) {
	    if (ch == '>' || ch == '<') tmp_clean = 0;
	    else if (ch == '.' || ch == ',') ;
	    else if (!tmp_clean && lang->clean_tmps) {
		pmc(lang->clean_tmps); tmp_clean = 1;
	    }
	    while(count-->0) pmc(lang->bf[p-bf]);
	} else {
	    if (ch == '#')
		pmc("\n#\n");
	    else if (ch == '=') {
		pmc(lang->zero_cell);
		tmp_clean = 1;
	    } else
		pc(ch);
	}
    }
}

static void
bfpackprint(void)
{
    static char bestfactor[] = {
	 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	 4, 4, 3, 3, 4, 3, 3,20, 4, 5, 5, 3, 4, 4, 5, 5,
	 4, 4,21, 5, 6, 6, 6,21, 5, 5, 6, 6, 4, 5, 5,22,
	 6, 7, 5, 5, 4,22, 6, 5, 7, 7, 7,22, 6, 6,23, 7,
	 8, 8, 6, 6, 6,23, 7, 7, 8, 8, 8, 5,23, 7, 6,24,
	 8, 9, 9,23, 7, 7, 7,24, 8, 8, 9, 7, 7, 7,24,24,
	 8, 8, 7, 9,10,10,10,24, 8, 7, 7,25, 9, 9,10,10,
	 8, 8, 8,25,25, 9, 9,26,10,11,11,11,25,25, 9, 9,
	 8,26,10,10,11,11,25, 9, 8, 8,26,26,10,10,27,11,
	12,12,12,12,26,26,10,10, 8, 9,11,11,12,12,12,26,
	10,10, 9, 9,27,11,11,28,12,13,10, 9, 9, 9,27,27,
	11,11,11,28,12,12,13,13,13,27,27,11,11, 9,10,28,
	12,12,29,13,14,14,11,11,10,10,28,28,12,12,12,29,
	13,11,14,14,14,14,28,28,12,12,12,27,11,13,13,30,
	14,15,15,28,12,12,10,11,11,29,13,13,13,30,14,14,
	15,15,11,11,11,29,29,13,13,13,30,30,14,14,31,15};

    struct instruction * n = pgm;

    for(; n; n=n->next) {
	int ch = n->ch;
	int count = n->count;
	char * p;
	if ((p = strchr(bf,ch))) {
	    if (count>1 && (ch == '+' || ch == '-')) {
		while(count) {
		    int cnt = count;
		    if (cnt>255) cnt = 255;
		    count-=cnt;
		    pmc(">>>>>");
		    if (bestfactor[cnt] == 1) {
			while(cnt-->0) pc('+');
		    } else {
			int a,b,t,i;
			t = bestfactor[cnt]; a = (0xF&t); t = !!(t&0x10);
			b = cnt/a+t;
			pc('>');
			for(i=0;i<a;i++) pc('+');
			pmc("[-<");
			for(i=0;i<b;i++) pc('+');
			pmc(">]<");
			t = cnt - a*b;

			for(i=0;i<t;i++) pc('+');
			for(i=0;i<-t;i++) pc('-');
		    }
		    pmc("[-<<<<<");
		    pmc(doubler_12->bf[p-bf]);
		    pmc(">>>>>]<<<<<");
		}
	    } else
		while(count-->0) pmc(doubler_12->bf[p-bf]);
	} else {
	    if (ch == '#') pc('\n');
	    if (ch == '=')
		pmc(lang->zero_cell);
	    else
		pc(ch);
	    if (ch == '#') pc('\n');
	}
    }
}

#if defined(__GNUC__) && (__GNUC__ > 4)
/* Only for idiots */
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif

/******************************************************************************/
/*       Now some small, but not completely trivial translations.             */
/******************************************************************************/

static void
risbf(int ch, int count)
{
    while(count-->0) switch(ch) {
    case '>':
	if (state!=1) pc('*'); state=1;
	pc('+');
	break;
    case '<':
	if (state!=1) pc('*'); state=1;
	pc('-');
	break;
    case '+':
	if (state!=0) pc('*'); state=0;
	pc('+');
	break;
    case '-':
	if (state!=0) pc('*'); state=0;
	pc('-');
	break;
    case '.': pc('/'); pc('/'); break;
    case ',': pc('/'); pc('*'); break;
    case '[': pc('/'); pc('+'); break;
    case ']': pc('/'); pc('-'); break;
    }
}

static void
tinybf(int ch, int count)
{
    while(count-->0) switch(ch) {
    case '>':
	if (state!=0) pc('='); state=0;
	pc('>');
	break;
    case '<':
	if (state!=1) pc('='); state=1;
	pc('>');
	break;
    case '+':
	if (state!=0) pc('='); state=0;
	pc('+');
	break;
    case '-':
	if (state!=1) pc('='); state=1;
	pc('+');
	break;
    case '.': pc('='); pc('='); break;
    case ',':
	if (state!=0) pc('='); state=0;
	pc('+');
	break;
    case '[':
	if (state!=0) pc('='); state=0;
	pc('|');
	break;
    case ']':
	if (state!=1) pc('='); state=1;
	pc('|');
	break;
    }
}

static void
headsecks(int ch, int count)
{
    char * p;
    int rset;
    static int rnd = 1;

    if (ch == '!') { linefix=';'; if(maxcol>1) maxcol--; }
    if (! (p = strchr(bf,ch))) return;

    while(count-->0) {
	rnd = rnd * 75 + ch + count;
	rnd = rnd + rnd / 65537;
	rnd &= 0x7FFFFFFF;

	ch = headsecksconv[p-bf];
	rset = rnd % 6;
	if (rset & 1) rset += 8;
	rset /= 2;
	rset *= 8;
	if (ch == 0) rset += 8;
	pc('A' -1 + ch + rset);
    }
}

static void
bfrle(int ch, int count)
{
    char * p;
    char buf[64];

    if (ch == '=') {
	pc('=');
	if (count == 0) return;
	if (count>0) {
	    ch = '+';
	} else {
	    count = -count;
	    ch = '-';
	}
    }

    if (! (p = strchr(bf,ch))) return;

    if (count > 2 && (p-bf) < 4) {
#ifndef NO_SNPRINTF
	snprintf(buf, sizeof(buf), "%d%c", count, ch);
#else
	sprintf(buf, "%d%c", count, ch);
#endif
	ps(buf);
    } else if (count < 2)
	pc(ch);
    else while (count-- > 0)
	pc(ch);
}

static void
bfbinrle(int ch, int count)
{
    char buf[sizeof(int)*8+3];
    char * p = buf;
    unsigned int ucount = count;

    if (ch == '=') {
	pc(ch);
	state = 0;
	ch = '+';
	if (count == 0) return;
    } else if (! (p = strchr(bf,ch))) return;

    if (ch == state) {
	if (ch == '+') { pc('-'); ucount++; }
	if (ch == '-') { pc('+'); ucount++; }
	if (ch == '>') { pc('<'); ucount++; }
	if (ch == '<') { pc('>'); ucount++; }
    }

    state = ch;
    if (count > 1 && (p-bf) < 4) {
	p = buf;
	while (ucount) {
	    if (ucount & 1)
		*p++ = ch;
	    else
		*p++ = '*';
	    ucount /= 2;
	}
	while(p>buf) {
	    p--;
	    pc(*p);
	}
    } else if (count < 2)
	pc(ch);
    else while (count-- > 0)
	pc(ch);
}

static void
bfxml(int ch, int count)
{
    char * p;
static const char * bfxml_1[] =
    {	"<ptrinc", "<ptrdec", "<inc", "<dec",
	"<print", "<read", "<while>", "</while>" };

    if (! (p = strchr(bf,ch))) {
	if (ch == '!') {
	    puts("<?xml version=\"1.0\"?>");
	    printf("<fuck bits='%d' wrap='Y'>",
		    (int)(bytecell?8:sizeof(int)*8));
	    printf("<tapes><tape/></tapes>\n");
	} else if (ch == '~')
	    printf("</fuck>\n");
	return;
    }

    if ((p-bf) >= 6) {
	if (ch == ']' && state>0) state--;
	if(state>0) printf("%*s", state*2, "");
	printf("%s\n", bfxml_1[p-bf]);
	if (ch == '[') state++;
    } else {
	if(state>0) printf("%*s", state*2, "");
	if (count > 1) {
	    printf("%s by='%d'/>\n", bfxml_1[p-bf], count);
	} else {
	    printf("%s/>\n", bfxml_1[p-bf]);
	}
    }
}

static void
bfugly(int ch, int count)
{
    char * p;
    int usebs = 0;
    int stars = 0;
    char och = ch;

    if (! (p = strchr(bf,ch))) return;
    switch(ch)
    {
    case ']': och = '['; usebs = 1; break;
    case '-': och = '+'; usebs = 1; break;
    case '<': och = '>'; usebs = 1; break;
    case ',': och = '.'; usebs = 1; break;
    }

    while(count) {
	if (count & 1) {
	    int i;
	    if (usebs) pc('\\');
	    if (!usebs && stars == 1)
		pc(och);
	    else for(i=0; i<stars; i++)
		pc('*');
	    pc(och);
	}
	count >>= 1;
	stars++;
    }
}

static void
malbrain(int ch, int count)
{
    char * p;

    if (! (p = strchr(bf,ch))) return;
    while (p-bf != state) {
	pc('>');
	state = ((state + 1) &7);
    }
    while (count-->0) pc('.');
}

static void
hanoilove(int ch, int count)
{
    /*	Plain translation:

	>       ..,...'...
	<       .,.'..
	+       ,.;'...
	-       .,...`.'...
	.       .,'"'...
	,       .,",'...
	[       ...'..,'...:
	]       ...,!...;.
    */

    /*  This is a somewhat better translation. */
    if (ch == '=') {
	while(state != 0) {state = (state+1)%4; pc('.');}
	pc(',');
	pc('`');
	ch = '+';
	if (count < 0) {
	    count = -count;
	    ch = '-';
	}
    } else if (ch == '[' || ch == ']' || ch == '.' || ch == ',')
	count = 1;

    while (count-->0) switch (ch) {
    case '#':
	pc('#');
	break;
    case '>':
	while(state != 1) {state = (state+1)%4; pc('.');}
	pc('\'');
	while(state != 2) {state = (state+1)%4; pc('.');}
	pc(',');
	break;
    case '<':
	while(state != 2) {state = (state+1)%4; pc('.');}
	pc('\'');
	while(state != 1) {state = (state+1)%4; pc('.');}
	pc(',');
	break;
    case '+':
    case '-':
	if (count > 22) {
	    int maxbit, v;

	    while(state != 1) {state = (state+1)%4; pc('.');}
	    pc('\''); /* Save reg */
	    while(state != 0) {state = (state+1)%4; pc('.');}

	    for(v=count+1,maxbit=0; v; v>>=1,maxbit++)
		;
	    maxbit = (1<<(maxbit-2));

	    /* reg = 2 */
	    pc(','); pc(';');

	    for(v=count+1; maxbit; ) {
		if (v & maxbit) pc(';');
		maxbit >>= 1;
		if (!maxbit) break;
		pc('\''); pc(';');
	    }
	    pc('\''); /* Save count */

	    while(state != 1) {state = (state+1)%4; pc('.');}
	    pc(','); /* pop saved reg */

	    count = 0;
	}
	while(state != 0) {state = (state+1)%4; pc('.');}

	if (ch == '+')
	    pc(';');
	else
	    pc('`');
	break;
    case '.':
	/* Beware the '"' command may be interpreted using the next physical
	   character not the next command character. Also the relative
	   priority of the stack D special case and the I/O special case
	   isn't documented.
	 */
	while(state == 3) {state = (state+1)%4; pc('.');}
	ps("\"'");
	break;
    case ',':
	while(state == 3) {state = (state+1)%4; pc('.');}
	ps("\",");
	break;
    case '[':
	while(state != 3) {state = (state+1)%4; pc('.');}
	pc('\'');
	pc(':');
	break;
    case ']':
	while(state != 3) {state = (state+1)%4; pc('.');}
	pc(',');
	pc('!');
	pc(';');
	break;
    }
}

/*
 * This is a translation written by http://esolangs.org/wiki/User:Int-e
 * here ...
 * http://esolangs.org/wiki/Talk:Brainfuck#Would_BF_still_be_TC_with_do-while_loops.3F
 *
 * This translation takes normal BF with traditional "While" loops and converts
 * it to run on an interpreter with "DO ...WHILE tape[p]<>0" style loops.
 * That is loops that run at least once.
 *
 * Obviously this doesn't convert I/O, however, the direct conversion is
 * normally sufficient for most simple programs. The output will print NUL
 * bytes for all "." commands that are skipped; these can be easily ignored.
 * And the "," command is sufficient for simple programs like the original
 * "primes" program.
 *
 * The six core commands convert completely which would appear to prove
 * the Turing completeness of the DoWhile variant.
 */

static void
bfdowhile(int ch, int count)
{
    char * p;

    if ((p = strchr(bf,ch)) || ch == '#') {
	struct instruction * n = calloc(1, sizeof*n);
	if (!n) { perror("bf2dowhile"); exit(42); }

	n->ch = ch;
	n->count = count;
	if (!last) pgm = n; else last->next = n;
	last = n;
	return;
    }

    if (ch == '~') {
	int stack_depth = 0, max_depth = 0;
	int i;
	struct instruction * n;

	for(n=pgm; n; n=n->next) {
	    if (n->ch == '[') {
		stack_depth++;
		if (stack_depth > max_depth) max_depth = stack_depth;
	    }
	    if (n->ch == ']') stack_depth--;
	}

	if (stack_depth != 0) fprintf(stderr, "Data structure error\n");

	while(max_depth-->0) pc('>');
	pmc(">>+>>+");

	for(n=pgm; n; n=n->next) {
	    int lcount = n->count;
	    while(lcount-->0) {
		int doloop = 0;

		if (lcount > 0 && (n->ch == '+' || n->ch == '-')) {
		    int v;
		    pmc(">>>>");
		    for(v=0; v<=lcount; v++) pc('+');
		    pmc("[-<<<<");
		    doloop = 1;
		}

		switch(n->ch) {

		case '+':
		    pmc("+[>>+<<-]>>[<+<+>>-]<-<-");
		    break;
		case '-':
		    pmc("+[>>+<<-]>+>[<-<+>>-]<<-");
		    break;
		case '>':
		    pmc(">>>>+<<<<<<[>>]>>>>+[-]<<+[<<+>>-]<<-");
		    break;
		case '<':
		    pmc("+[>>+<<-]>>->>+[<<]>>>>+[-]<<-<<<<");
		    break;

		case '[':
		    stack_depth++;

		    pmc("+[>>+<<-]>>[>>+<<<<<<[<<]");
		    for(i=0;i<stack_depth; i++) pc('<');
		    pc('+');
		    for(i=0;i<stack_depth; i++) pc('>');
		    pmc(">>[>>]>>-]>>-<<<");
		    pmc("+[>+[<<+>>-]>>+[<<+>>-]>>+<<<<-<<->-]>+[-]>>>>[<<<<<+>>>>>-]<<<<<-<");
		    pmc("[");
		    break;

		case ']':
		    pmc("+[>>>>+<<<<-]>>>>-<<<");
		    pmc("+[>+[<<+>>-]>>+[<<+>>-]>>+<<<<-<<->-]>+[-]>>>>[<<<<<+>>>>>-]<<<<<-<");
		    pmc("]");

		    pmc("+[<<]");
		    for(i=0;i<stack_depth; i++) pc('<');
		    pmc("[");
		    for(i=0;i<stack_depth; i++) pc('>');
		    pmc(">>[>>]<<+[<<]");
		    for(i=0;i<stack_depth; i++) pc('<');
		    pmc("-]");
		    for(i=0;i<stack_depth; i++) pc('>');
		    pmc(">>[>>]<<--");

		    stack_depth--;
		    break;

		case '.':
		    pmc(">.<");
		    break;

		case ',':
		    pmc(">,<");
		    break;

		case '#':
		    pmc(">#<");
		    break;
		}

		if (doloop) {
		    pmc(">>>>]<<<<");
		    break;
		}
	    }
	}
    }
}

void
qqq(int ch, int count)
{
    char * p;

    if (! (p = strchr(bf,ch))) return;

    switch(ch)
    {
    case '>': ch = ';'; break;
    case '<': ch = '-'; break;
    case '+': ch = '.'; break;
    case '-': ch = ','; break;
    case '.': ch = '!'; break;
    case ',': ch = '?'; break;
    case '[': if (state!=0) {pc('\''); state=!state;} ch = '"'; break;
    case ']': if (state==0) {pc('\''); state=!state;} ch = '"'; break;
    }
    while (count-->0) pc(ch);
}

/******************************************************************************/
/*       Finally the list of completely trivial translations.                 */
/******************************************************************************/

static trivbf bfout[1] = {{
    .name = "single",
    .class = L_BF,
    .multi = 1,
    .bf = { ">", "<", "+", "-", ".", ",", "[", "]"},
    .zero_cell = "[-]",
    .help = "BF to BF translation.",
}};

static trivbf rhoprime[1] = {{
    /* Ρ″
       Corrado Böhm. On a family of Turing machines and
       the related programming language. International
       Computation Centre Bulletin, 3:187-94, July 1964.
    */
    .name = "rho",
    .class = L_JNWORD,
    .bf = { "r′λ", "R", "λR", "r′", "Ρ″", "Ιⁿ", "(", ")" },
    .name2 = "rhoprime",
    .help = "The original 1964 Ρ″ by Corrado Böhm (Rho double prime)"
}};

static trivbf cbyte[1] = {{
    /* Language "C" */
    .name = "c",
    .class = L_CWORDS,
    .bf = { "m+=1;", "m-=1;", "++*m;", "--*m;",
    "write(1,m,1);", "read(0,m,1);", "while(*m) {", "}" },
    .help = "Plain C"
}};

static trivbf cint[1] = {{
    .name = "cint",
    .class = L_CWORDS,
    .bf = { "m+=1;", "m-=1;", "++*m;", "--*m;",
        "putchar(*m);", "{int (_c) = getchar(); if(_c!=EOF) *m=_c; }",
        "while(*m) {", "}" },
    .help = "Plain C int cells"
}};

static trivbf cbyte_rle[1] = {{
    .name = "crle",
    .class = L_CWORDS,
    .bf = {";m+=1", ";m-=1", ";*m+=1", ";*m-=1",
	";write(1,m,1)", ";read(0,m,1)", ";while(*m){", ";}" },
    .repeat_prev = "+1",
    .help = "Plain C with rle"
}};

static trivbf cint_rle[1] = {{
    .name = "cintrle",
    .class = L_CWORDS,
    .bf = { ";m+=1", ";m-=1", ";*m+=1", ";*m-=1",
        ";putchar(*m)", ";{int _c=getchar();if(_c!=EOF)*m=_c;}",
        ";while(*m){", ";}" },
    .repeat_prev = "+1",
    .help = "Plain C with int cells and rle"
}};

/* Language "ook" */
static trivbf ook[1] = {{
    .name = "ook",
    .class = L_WORDS,
    .bf = {"Ook. Ook?", "Ook? Ook.", "Ook. Ook.", "Ook! Ook!",
    "Ook! Ook.", "Ook. Ook!", "Ook! Ook?", "Ook? Ook!"},
    .help = "Ook! -- http://esolangs.org/wiki/Ook!"
}};

/**************************************************************************/

/* Language "blub" */
static trivbf blub[1] = {{
    .name = "blub",
    .class = L_WORDS,
    .bf = {"blub. blub?", "blub? blub.", "blub. blub.", "blub! blub!",
    "blub! blub.", "blub. blub!", "blub! blub?", "blub? blub!"},
    .help = "Blub -- http://esolangs.org/wiki/Blub",
}};

/* Language "fuck fuck" */
static trivbf f__k[1] = {{
    .name = "fk",
    .class = L_CDWORDS,
    .bf = {"folk", "sing", "barb", "teas", "cask", "kerb", "able", "bait"},
    .help = "Fuck fuck -- https://esolangs.org/wiki/Fuckfuck",
}};

/* Language "fuck fuck" With "RLE" */
static trivbf f__krle[1] = {{
    .name = "fk!",
    .class = L_JNWORD,
    .bf = {"f**k", "s**g", "b**b", "t**s", "c**k", "k**b", "a**e", "b**t"},
    .repeat_prev = "!",
    .help = "Fuck fuck including RLE",
}};

/* Language "pogaack" */
static trivbf pogaack[1] = {{
    .name = "pogaack", .name2 = "pog",
    .class = L_WORDS,
    .bf = {"pogack!", "pogaack!", "pogaaack!", "poock!",
    "pogaaack?", "poock?", "pogack?", "pogaack?"},
    .repeat_prev = "pock!",
    .help = "Pogaack -- https://esolangs.org/wiki/POGAACK",
}};

/* Language "triplet" */
static trivbf trip[1] = {{
    .name = "triplet", .name2= "trip",
    .class = L_JNWORD,
    .bf = { "001", "100", "111", "000", "010", "101", "110", "011" },
    .help = "Triplet -- http://esolangs.org/wiki/Triplet",
}};

/* Language "Descriptive BF" */
static trivbf nicebf[1] = {{
    .name = "nice", .name2 = "n",
    .class = L_CDWORDS,
    .bf = { "right", "left", "up", "down", "out", "in", "begin", "end" },
    .help = "Nice memorable C translation.",
}};

static trivbf bc[1] = {{
    .name = "mini",
    .class = L_CDWORDS,
    .bf = { "r", "l", "u", "d", "o", "i", "b", "e"},
    .help = "Compact C translation.",
}};

static trivbf bc_rle[1] = {{
    .name = "rle",
    .class = L_CDWORDS,
    .bf = { "r", "l", "u", "d", "o", "i", "b", "e"},
    .repeat_prev = "x" ,
    .help = "Odd RLE C translation",
}};

/* Order should be "there", "once", "was", "a", "fish", "named", "Fred" */
static trivbf fish[1] = {{
    .name = "fish",
    .class = L_CDWORDS,
    .bf = { "once", "there", "was", "a", "fish", "dead", "named", "Fred" },
    .help = "There once was a (dead) fish named Fred",
}};

/* Silly (er) ones. */
static trivbf dotty[1] = {{
    .name = ":", .name2="dotty",
    .class = L_CHARS,
    .bf = { "..", "::", ".:.", ".::", ":.::", ":...", ":.:.", ":..:" },
    .help = "Dotty",
}};

static trivbf lisp0[1] = {{
    .name = "lisp0", .name2 = "lisp",
    .class = L_CHARS,
    .bf = { "((", "))", "()(", "())", ")())", ")(((", ")()(", ")(()" },
    .help = "Lisp Zero",
}};

static trivbf bewbs[1] = {{
    .name = "bewbs",
    .class = L_WORDS,
    .bf = { "(.)(.)", "(.){.}", "(.)[.]", "[.](.)",
      "[.][.]", "{.}{.}", "{.}[.]", "[.]{.}" },
    .help = "Bewbs -- https://gist.github.com/rdebath/12498c2c798a71d59f59",
}};

/* Language COW: Not quite as simple as some commands aren't direct replacements. */
static trivbf moo[1] = {{
    .name = "moo", .name2= "cow",
    .class = L_JNWORD,
    .bf = {"moO", "mOo", "MoO", "MOo",
    "MMM MOO Moo OOO moo MMM", "OOO Moo", "MOO moO mOo", "MoO MOo moo"},
    .zero_cell = "OOO",
    .help = "Cow -- http://www.frank-buss.de/cow.html",
}};

/* Some random Chinese words */
static trivbf chinese[1] = {{
    .name = "chinese", .name2 = "chi",
    .class = L_JNWORD,
    .bf = { "右", "左", "上", "下", "出", "里", "始", "末" },
    .help = "Some random Chinese characters.",
}};

/* Some random Chinese words 2 */
static trivbf chinese2[1] = {{
    .name = "chinese2", .name2 = "chi2",
    .class = L_CDWORDS,
    .bf = { "右", "左", "上", "下", "出", "里", "始", "末" },
    .help = "Some random Chinese characters.",
}};

/* https://github.com/mescam/zerolang */
static trivbf zero[1] = {{
    .name = "zero",
    .class = L_WORDS,
    .bf = { "0+", "0-", "0++", "0--", "0.", "0?", "0/", "/0" },
    .help = "'zerolang' from mescam on github",
}};

/* Language: yolang; http://yolang.org/ */
static trivbf yolang[1] = {{
    .name = "yo",
    .class = L_JNWORD,
    .bf = { "yo", "YO", "Yo!", "Yo?", "YO!", "yo?", "yo!", "YO?" },
    .help = "'yolang' http://yolang.org/",
}};

/* Language: けいおんfuck; https://gist.github.com/wasabili/562178 */
static trivbf k_on_fuck[1] = {{
    .name = "kon",
    .class = L_JNWORD,
    .bf = { "うんうんうん", "うんうんたん", "うんたんうん", "うんたんたん",
      "たんうんうん", "たんうんたん", "たんたんうん", "たんたんたん" },
    .help = "K-on fuck -- https://gist.github.com/wasabili/562178",
}};

/* Language: Petooh; https://github.com/Ky6uk/PETOOH */
static trivbf petooh[1] = {{
    .name = "petooh",
    .class = L_WORDS,
    .bf = { "Kudah", "kudah", "Ko", "kO", "Kukarek", "kukarek", "Kud", "kud" },
    .help = "Petooh -- https://github.com/Ky6uk/PETOOH",
}};

/* Some random Arabic letters */
static trivbf arabic[1] = {{
    .name = "arabic", .name2 = "ara",
    .class = L_JNWORD,
    .bf = { "ش", "س", "ث", "ت", "ص", "ض", "ق", "ف" },
    .help = "Some random Arabic letters",
}};

/* dc(1) using an array and a pointer in another variable */
static trivbf dc1[1] = {{
    .name = "dc1", .name2 = "dc",
    .class = L_JNWORD+C_NUMRLE,
    .bf = {   "lp%d+sp", "lp%d-sp", "lp;a%d+lp:a", "lp;a%d-lp:a",
    "lp;aaP", "", "[lp;a0=q", "lbx]dSbxLbs."},
    .gen_hdr = "0sp[q]sq",
    .wrap_eight_bit = "lp;a256%lp:a",
    .zero_cell = "0lp:a",
    .help = "DC using an array and a pointer variable.",
}};

/* dc(1) using an array and a pointer on the main stack */
static trivbf dc2[1] = {{
    .name = "dc2",
    .class = L_JNWORD+C_NUMRLE,
    .bf = { "%d+", "%d-", "dd;a%d+r:a", "dd;a%d-r:a",
    "d;aaP", "", "[d;a0=q", "lbx]dSbxLbs."},
    .gen_hdr = "0[q]sq",
    .help = "DC using an array with the pointer on the stack (not V7).",
}};

/* dc(1) using the main stack and the stack 'L' as the tape */
static trivbf dc3[1] = {{
    .name = "dc3",
    .class = L_JNWORD+C_ADDRLE,
    .bf = { "SLz0=z", "LL", "%d+", "%d-",
    "daP", "", "[lmxd0=q", "lbx]dSbxLbs."},
    .gen_hdr = "[256+]sM[256%d0>M]sm [q]sq[0]szz",
    .help = "DC using the tape in the main stack and a second one.",
}};

/* dc(1) using two unbounded variables. */
static trivbf dc4[1] = {{
    .name = "dc4",
    .class = L_JNWORD+C_ADDRLE,
    .bf = {"lr256*+sr lld256/sl lmx", "ll256*+sl lrd256/sr lmx",
    "%d+ lmx", "%d- lmx", "daP", "", "[lmxd0=q", "lbx]dSbxLbs."},
    .gen_hdr = "[256+]sM[256%d0>M]sm [q]sq 0dsldsr",
    .help = "DC using two unbounded variables to store the tape.",
    .bytesonly = 1,
}};

/* Language "nyan" https://github.com/tommyschaefer/nyan-script */
static trivbf nyan[1] = {{
    .name = "nyan",
    .class = L_JNWORD,
    .bf = {"anna", "nana", "nnya", "nnna", "anan", "nnay", "annn", "naaa"},
    .gen_hdr = "nyan", "nyan",
    .help = "'nyan-script' https://github.com/tommyschaefer/nyan-script",
}};

/* Language @! .. http://esolangs.org/wiki/@tention! */
static trivbf atpling[1] = {{
    .name = "@!",
    .class = L_JNWORD,
    .bf = {"@I^;", "@E^;", "@B^;", "@C^;", "$XA^<;", "&XA^>;", "XA^[", "];"},
    .gen_hdr = "D@=; T2=; Q(x{TTT*=})=; 8Q^; T{D0<}; Q,; T,; A(D!x-{D~}`)=;"
     "X0=; I(XX1+=)=; E(XX1-=)=; B(XA^XA^1+=)=; C(XA^XA^1-=)=;",
    .help = "@! from http://esolangs.org/wiki/@tention!",
}};

/* Language Cupid */
static trivbf cupid[1] = {{
    .name = "cupid", .name2 = "cp",
    .class = L_JNWORD,
    .bf = { "->", "<-", "><", "<>", "<<", ">>", "-<", ">-" },
    .help = "Cupid from http://esolangs.org/wiki/Cupid"
}};

/* Language Ternary */
static trivbf ternary[1] = {{
    .name = "ternary",
    .class = L_JNWORD,
    .bf = { "01", "00", "11", "10", "20", "21", "02", "12" },
    .help = "Ternary from http://esolangs.org/wiki/Ternary",
}};

/* Language pikalang -- https://github.com/skj3gg/pikalang */
static trivbf pikalang[1] = {{
    .name = "pika", .name2 = "pikalang",
    .class = L_CDWORDS,
    .bf = {"pipi", "pichu", "pi", "ka", "pikachu", "pikapi", "pika", "chu" },
    .help = "Pikalang from https://github.com/skj3gg/pikalang",
}};

/* Language "spoon" */
static trivbf spoon[1] = {{
    .name = "spoon",
    .class = L_CHARS,
    .bf = { "010", "011", "1", "000", "001010", "0010110", "00100", "0011"},
    .gen_foot = "00101111",
    .help = "Language spoon http://esolangs.org/wiki/spoon",
}};

/* Language "trollscript" */
static trivbf troll[1] = {{
    .name = "troll",
    .class = L_JNWORD,
    .bf = { "ooo", "ool", "olo", "oll", "loo", "lol", "llo", "lll"},
    .gen_hdr = "Tro", "ll.",
    .help = "Trollscript -- https://github.com/tombell/trollscript",
}};

/* Language "Roadrunner" */
static trivbf roadrunner[1] = {{
    .name = "roadrun", .name2 = "meep",
    .class = L_CDWORDS,
    .bf = { "meeP", "Meep", "mEEp", "MeeP", "MEEP", "meep", "mEEP", "MEEp" },
    .help = "Roadrunner -- https://esolangs.org/wiki/Roadrunner",
}};

/* Language "XiFuck" */
static trivbf xifucklang[1] = {{
    .name = "xifuck",
    .class = L_CDWORDS,
    .bf = { "修宪", "连任", "习近平", "大撒币", "登基", "包皇", "萨格尔王", "宽衣"},

    .help = "XiFuck -- https://github.com/backup53/XiFuck",
}};

/* Language "CGALang" */
static trivbf cgalang[1] = {{
    .name = "cga",
    .class = L_CDWORDS,
    .bf = { "文明", "和谐", "爱国", "自由", "诚信", "友善", "富强", "民主"},
    /* *p+=5 敬业, *p-=5 平等 */
    .help = "CGALang -- https://gloomyghost.com/CGALang/",
}};

/* Language "brainfuq" */
static trivbf brainfuq[1] = {{
    .name = "brainfuq",
    .class = L_CDWORDS,
    .bf = { "👉", "👈", "👆", "👇", "🌚", "🌝", "🌜", "🌛"},
    .help = "Brainfuq -- https://github.com/allenx/Brainfuq",
}};

/* Language "emojifuck" */
static trivbf emojifuck[1] = {{
    .name = "emoji",
    .class = L_JNWORD,
    .bf = { "👉", "👈", "👍", "👎", "🙊", "🙉", "⏩", "⏪"},
    .help = "Emojifuck -- https://github.com/kbarrette/emojifuck",
}};

/* Language "trigrams" */
static trivbf trigram[1] = {{
    .name = "trigram",
    .class = L_JNWORD,
    .bf = { "☰", "☱", "☲", "☳", "☴", "☵", "☶", "☷"},
    .help = "Trivial BF substitution using Trigrams",
}};

/* Language "babylang" */
static trivbf babylang[1] = {{
    .name = "babylang",
    .class = L_WORDS,
    .bf = { "gaga", "gugu", "aaag", "uuug", "guuu", "gaaa", "gagu", "guga"},
    .zero_cell = "unga",
    .help = "Babylang -- https://esolangs.org/wiki/Babylang",
}};

/* Language "tmcc" */
static trivbf tmcc[1] = {{
    .name = "tmcc",
    .class = L_JNWORD,
    .bf = { "しの", "さばにき", "SMD", "にゃんこ", "ゲェジ", "がりん", "NEMO", "キュゥ"},
    .help = "Tmcc -- https://github.com/k-mrm/tmcc-bf",
}};

/* Language "heartfuck" */
static trivbf heart[1] = {{
    .name = "heart",
    .class = L_JNWORD,
    .bf = { "💗", "💜", "💖", "❤", "💌", "❣", "💛", "💙"},
    .help = "heart -- https://github.com/heartfuck/heartfuck",
}};

/* Language brainbool: http://esolangs.org/wiki/Brainbool */
static trivbf brainbool[1] = {{
    .name = "brainbool",
    .class = L_BFCHARS,
    .bf = {">>>>>>>>>", "<<<<<<<<<",
     ">[>]+<[+<]>>>>>>>>>[+]<<<<<<<<<",
     ">>>>>>>>>+<<<<<<<<+[>+]<[<]>>>>>>>>>[+]<<<<<<<<<",
     ">.>.>.>.>.>.>.>.<<<<<<<<",
     ">,>,>,>,>,>,>,>,<<<<<<<<",
     ">>>>>>>>>+<<<<<<<<+[>+]<[<]>>>>>>>>>[+<<<<<<<<[>]+<[+<]",
     ">>>>>>>>>+<<<<<<<<+[>+]<[<]>>>>>>>>>]<[+<]",
     },
     .help = "Brainbool -- http://esolangs.org/wiki/Brainbool",
    .bytesonly = 1,
}};

/* BF Doubler doubles the cell size. */
/* 12 cost, cells in LXXH order, with tmpzero */
static trivbf doubler_12[1] = {{
    .name = "dbl12",
    .class = L_BF,
    .multi = 2,
    .bf = {">>>>", "<<<<", ">+<+[>-]>[->>+<]<<", ">+<[>-]>[->>-<]<<-",
    ".", "[-]>[-]>[-]>[-]<<<,",
    ">+<[>-]>[->+>[<-]<[<]>[-<+>]]<-" "[+<",
    ">+<[>-]>[->+>[<-]<[<]>[-<+>]]<-" "]<"},
    .zero_cell = "[-]>[-]>[-]>[-]<<<",
    .clean_tmps = ">[-]>[-]<<",
}};

/* Copy cell cost, cells in LXXH order, with tmpzero */
static trivbf doubler_copy_LXXH[1] = {{
    .name = "dblcp12",
    .class = L_BF,
    .multi = 2,
    .bf = {
    ">>>>", "<<<<",
    ">>+<<+[>>-<<[->+<]]>[-<+>]>[->+<]<<",
    ">>+<<[>>-<<[->+<]]>[-<+>]>[->-<]<<-",
    ".", "[-]>[-]>[-]>[-]<<<,",
    "[>+<[->>+<<]]>>[-<<+>>]>[<<+>>[-<+>]]<[->+<]<[[-]<",
    "[>+<[->>+<<]]>>[-<<+>>]>[<<+>>[-<+>]]<[->+<]<]<"},
    .zero_cell = "[-]>[-]>[-]>[-]<<<",
    .clean_tmps = ">[-]>[-]<<"
}};

/* 12 cost, cells in LXXH order, no tmpzero */
static trivbf doubler_12nz[1] = {{
    .name = "dbl12nz",
    .class = L_BF,
    .multi = 2,
    .bf = {">>>>", "<<<<", ">+<+[>-]>[->>+<]<<", ">+<[>-]>[->>-<]<<-",
    ".", ">>>[-]<<<[-],",
    ">+<[>-]>[->+>[<-]<[<]>[-<+>]]<-" "[+<",
    ">+<[>-]>[->+>[<-]<[<]>[-<+>]]<-" "]<"
    },
    .zero_cell = ">>>[-]<<<[-]",
}};

/* 12 cost, cells in HXXL order, uses "[>]", no tmpzero */
static trivbf doubler_12r[1] = {{
    .name = "dbl12r",
    .class = L_BF,
    .multi = 2,
    .bf = {">>>>", "<<<<", ">>+>+[<-]<[-<<+>]<", ">>+>[<-]<[-<<->]>>-<<<",
    ">>>.<<<", "[-]>>>[-],<<<",
    ">>+>[<-]<[-<+<[>-]>[>]<[->+<]]>-" "[+<<",
    ">>+>[<-]<[-<+<[>-]>[>]<[->+<]]>-" "]<<"
    },
    .zero_cell = ">>>[-]<<<[-]",
}};

/* 17 cost, cells in XXLHXX order uses [>], no tmpzero */
static trivbf doubler_17a[1] = {{
    .name = "dbl17a",
    .class = L_BF,
    .multi = 2,
    .bf = {
    ">>>>", "<<<<",
    ">>+[>>+<<<]>>>[>]<[-<->]<+<<<", ">>[>>+<<<]>>>[>]<[-<+>]<-<-<<",
    ">>.<<", ">>>[-]<[-],<<",
    ">>[>>+<<<]>>>[>]<[->+<]<[<<+>>>]<<<[<]>>>>>[-<<<<+>>>>]<<<<" "[[-]<",
    ">>[>>+<<<]>>>[>]<[->+<]<[<<+>>>]<<<[<]>>>>>[-<<<<+>>>>]<<<<" "]<"
    },
    .zero_cell = ">>>[-]<[-]<<",
}};

/* 17 cost, cells in XXLHXX order, no tmpzero */
static trivbf doubler_17b[1] = {{
    .name = "dbl17b",
    .class = L_BF,
    .multi = 2,
    .bf = {
    ">>>>", "<<<<",
    ">>+[>>+>]<[<<<]>>>[-<->]<+<<<", ">>[>>+>]<[<<<]>>>[-<+>]<-<-<<",
    ">>.<<", ">>>[-]<[-],<<",
    ">>[>>+>]<[<<<]>>>[->+<]<[<<+>>>]<<<[<]>>>>>[-<<<<+>>>>]<<<<" "[[-]<",
    ">>[>>+>]<[<<<]>>>[->+<]<[<<+>>>]<<<[<]>>>>>[-<<<<+>>>>]<<<<" "]<"
    },
    .zero_cell = ">>>[-]<[-]<<",
}};

/* 12 cost, cells in LXXMNXXH order, with tmpzero */
static trivbf bfquad_12[1] = {{
    .name = "quad12",
    .class = L_BF,
    .multi = 4,
    .bf = {
    ">>>>>>>>", "<<<<<<<<",
    ">+<+[>-]>[->+>+[<-]<[->>>+<+[>-]>[->>+<]<<<<<]>]<<",
    ">+<[>-]>[->+>[<-]<[->>>+<[>-]>[->>-<]<<-<<<]>>-<]<<-",
    ".", "[-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]<<<<<<<,",
    ">+<[>-]>[->+>[<-]<[->>>+<[>-]>[->+>[<-]<[<]>[-<+>]]<<<<<]>]>>>-[+<<<<<",
    ">+<[>-]>[->+>[<-]<[->>>+<[>-]>[->+>[<-]<[<]>[-<+>]]<<<<<]>]>>>-]<<<<<"
    },
    .zero_cell = "[-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]<<<<<<<",
    .clean_tmps = ">[-]>[-]>>>[-]>[-]<<<<<<"
}};

/* 12 cost, cells in LXXMNXXH order, no tmpzero */
static trivbf bfquad_12nz[1] = {{
    .name = "quad12nz",
    .class = L_BF,
    .multi = 4,
    .bf = {
    ">>>>>>>>", "<<<<<<<<",
    ">+<+[>-]>[->+>+[<-]<[->>>+<+[>-]>[->>+<]<<<<<]>]<<",
    ">+<[>-]>[->+>[<-]<[->>>+<[>-]>[->>-<]<<-<<<]>>-<]<<-",
    ".", "[-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]<<<<<<<,",
    ">+<[>-]>[->+>[<-]<[->>>+<[>-]>[->+>[<-]<[<]>[-<+>]]<<<<<]>]>>>-[+<<<<<",
    ">+<[>-]>[->+>[<-]<[->>>+<[>-]>[->+>[<-]<[<]>[-<+>]]<<<<<]>]>>>-]<<<<<"
    }
}};

/* Copy cell cost, cells in XLHX order, with tmpzero */
static trivbf doubler_copy[1] = {{
    .name = "dblcopy", .name2 = "dblcp",
    .class = L_BF,
    .multi = 2,
    .bf = {
    ">>>", "<<<",
    "+>+[<->[->>+<<]]>>[-<<+>>]<<<[->>+<<]",
    "+>[<->[->>+<<]]>>[-<<+>>]<<<[->>-<<]>-<",
    ">.<", "[-]>[-]>[-]>[-]<<,<",
    ">[<+>[->>+<<]]>>[-<<+>>]<[<<+>>[->+<]]>[-<+>]<<<" "[[-]",
    ">[<+>[->>+<<]]>>[-<<+>>]<[<<+>>[->+<]]>[-<+>]<<<" "]"},
    .zero_cell = "[-]>[-]>[-]>[-]<<<",
    .clean_tmps = "[-]>>>[-]<<<"
}};

/* Copy cell cost, cells in XLHX order, no tmpzero */
static trivbf doubler_copynz[1] = {{
    .name = "dblcpnz",
    .class = L_BF,
    .multi = 2,
    .bf = {
    ">>>", "<<<",
    "+>+[<->[->>+<<]]>>[-<<+>>]<<<[->>+<<]",
    "+>[<->[->>+<<]]>>[-<<+>>]<<<[->>-<<]>-<",
    ">.<", ">[-]>[-]<,<",
    ">[<+>[->>+<<]]>>[-<<+>>]<[<<+>>[->+<]]>[-<+>]<<<" "[[-]",
    ">[<+>[->>+<<]]>>[-<<+>>]<[<<+>>[->+<]]>[-<+>]<<<" "]"
    },
    .zero_cell = ">[-]>[-]<<",
}};

/* Copy cell cost(+), cells in XLHX..X order, no tmpzero */
/* http://esolangs.org/wiki/Brainfuck_bitwidth_conversions  */
static trivbf doubler_esolang[1] = {{
    .name = "dbleso",
    .class = L_BF,
    .multi = 2,
    .bf = {
    ">>>",
    "<<<",
    ">+" "[<+>>>+<<-]<[>+<-]+>>>[<<<->>>[-]]<<<[-" ">>+<<" "]",
    ">[<+>>>+<<-]<[>+<-]+>>>[<<<->>>[-]]<<<[-" ">>-<<" "]>-<",
    ">.<",
    ">[-]>[-]<,<",
    ">[>>+>>>+<<<<<-]>>>>>[<<<<<+>>>>>-]<<<" "[[-]<<<+>>>]<"
	  "[>+>>>+<<<<-]>>>>[<<<<+>>>>-]<<<" "[[-]<<<+>>>]<<<" "[[-]",
    ">[>>+>>>+<<<<<-]>>>>>[<<<<<+>>>>>-]<<<" "[[-]<<<+>>>]<"
	  "[>+>>>+<<<<-]>>>>[<<<<+>>>>-]<<<" "[[-]<<<+>>>]<<<" "]"
    },
    .zero_cell = ">[-]>[-]<<",
}};

/* Copy cell cost, cells in XLMNHX order, with tmpzero */
static trivbf bfquadz[1] = {{
    .name = "quad",
    .class = L_BF,
    .multi = 4,
    .bf = {
    ">>>>>", "<<<<<",

    "+>+[<->[->>>>+<<<<]]>>>>[-<<<<+>>>>]<<<<<[>>+[<<->>[->>>+<<<]]>>>[-<<<+"
    ">>>]<<<<<[>>>+[<<<->>>[->>+<<]]>>[-<<+>>]<<<<<[->>>>+<<<<]]]",

    "+>[<->[->>>>+<<<<]]>>>>[-<<<<+>>>>]<<<<<[>>[<<->>[->>>+<<<]]>>>[-<<<+>>>"
    "]<<<<<[>>>[<<<->>>[->>+<<]]>>[-<<+>>]<<<<<[->>>>-<<<<]>>>-<<<]>>-<<]>-<",

    ">.<", "[-]>[-]>[-]>[-]>[-]>[-]<<<<,<",

    ">[<+>[->>>>+<<<<]]>>>>[-<<<<+>>>>]" "<<<[<<+>>[->>>+<<<]]>>>[-<<<+>>>]"
    "<<[<<<+>>>[->>+<<]]>>[-<<+>>]" "<[<<<<+>>>>[->+<]]>[-<+>]" "<<<<<[[-]",

    ">[<+>[->>>>+<<<<]]>>>>[-<<<<+>>>>]" "<<<[<<+>>[->>>+<<<]]>>>[-<<<+>>>]"
    "<<[<<<+>>>[->>+<<]]>>[-<<+>>]" "<[<<<<+>>>>[->+<]]>[-<+>]" "<<<<<]"},

    .zero_cell = "[-]>[-]>[-]>[-]>[-]>[-]<<<<<",
    .clean_tmps = "[-]>>>>>[-]<<<<<",
    .help = "BF to BF translation, cell size double doubler."
}};

/* Copy cell cost, cells in XLMNHX order, no tmpzero */
static trivbf bfquadnz[1] = {{
    .name = "quadnz",
    .class = L_BF,
    .multi = 4,
    .bf = {
    ">>>>>", "<<<<<",

    "+>+[<->[->>>>+<<<<]]>>>>[-<<<<+>>>>]<<<<<[>>+[<<->>[->>>+<<<]]>>>[-<<<+"
    ">>>]<<<<<[>>>+[<<<->>>[->>+<<]]>>[-<<+>>]<<<<<[->>>>+<<<<]]]",

    "+>[<->[->>>>+<<<<]]>>>>[-<<<<+>>>>]<<<<<[>>[<<->>[->>>+<<<]]>>>[-<<<+>>>"
    "]<<<<<[>>>[<<<->>>[->>+<<]]>>[-<<+>>]<<<<<[->>>>-<<<<]>>>-<<<]>>-<<]>-<",

    ">.<", ">[-]>[-]>[-]>[-]<<<,<",

    ">[<+>[->>>>+<<<<]]>>>>[-<<<<+>>>>]" "<<<[<<+>>[->>>+<<<]]>>>[-<<<+>>>]"
    "<<[<<<+>>>[->>+<<]]>>[-<<+>>]" "<[<<<<+>>>>[->+<]]>[-<+>]" "<<<<<[[-]",

    ">[<+>[->>>>+<<<<]]>>>>[-<<<<+>>>>]" "<<<[<<+>>[->>>+<<<]]>>>[-<<<+>>>]"
    "<<[<<<+>>>[->>+<<]]>>[-<<+>>]" "<[<<<<+>>>>[->+<]]>[-<+>]" "<<<<<]"
    },
    .zero_cell = ">[-]>[-]>[-]>[-]<<<<",
    .help = "BF to BF translation, cell size *4. (no tmp wipe)"
}};

static trivbf bitbf8[1] = {{
    .name = "bit8",
    .class = L_BFCHARS,
    .help = "BF to BF translation, set cell size to 8 bits.",

    .bf = {
	">>>>>>>>>", "<<<<<<<<<",
	">[>]+<[-<]>>>>>>>>>[-]<<<<<<<<<",
	">>>>>>>>>+<<<<<<<<-[++>-]<[<]>>>>>>>>>[-]<<<<<<<<<",
	">>>>>>>>.<.<.<.<.<.<.<.<", // MSB first, OK?
	">>>>>>>>,<,<,<,<,<,<,<,<",
	">>>>>>>>>+<<<<<<<<-[++>-]<[<]>>>>>>>>>"	// DEC
	    "[-<<<<<<<<<"				// Check CF & zero
	    ">[>]+<[-<]",				// INC, won't wrap
	">>>>>>>>>+<<<<<<<<-[++>-]<[<]>>>>>>>>>"	// DEC
	    "]"						// Check CF
	    "<[-<]"					// INC back to zero
    }
}};

static trivbf bitbf[1] = {{
    .name = "bit",
    .class = L_BFCHARS,
    .help = "BF to BF translation, set cell size to 1 bit.",

    .bf = {
	">>", "<<",
	">+<[>-<-]>[<+>-]<",	// Careful to use only values 0 and 1
	">+<[>-<-]>[<+>-]<",	// '-' is identical to '+'
	"[->+>>+<<<]>>>[-<<<+>>>]++++++[-<<++++++++>>]<<.[-]<", ",",
	"[", "]"
    }
}};

static trivbf octbf[1] = {{
    .name = "octbf",
    .class = L_BFCHARS,
    .help = "BF to BF translation, set cell size *8.",

    .bf = {
	">>>>>>>>>", "<<<<<<<<<",

	"+>+[<->[->>>>>>>>+<<<<<<<<]]>>>>>>>>[-<<<<<<<<+>>>>>>>>]<<<<<<<<"
	"<[>>+[<<->>[->>>>>>>+<<<<<<<]]>>>>>>>[-<<<<<<<+>>>>>>>]<<<<<<<<<"
	"[>>>+[<<<->>>[->>>>>>+<<<<<<]]>>>>>>[-<<<<<<+>>>>>>]<<<<<<<<<[>>"
	">>+[<<<<->>>>[->>>>>+<<<<<]]>>>>>[-<<<<<+>>>>>]<<<<<<<<<[>>>>>+["
	"<<<<<->>>>>[->>>>+<<<<]]>>>>[-<<<<+>>>>]<<<<<<<<<[>>>>>>+[<<<<<<"
	"->>>>>>[->>>+<<<]]>>>[-<<<+>>>]<<<<<<<<<[>>>>>>>+[<<<<<<<->>>>>>"
	">[->>+<<]]>>[-<<+>>]<<<<<<<<<[->>>>>>>>+<<<<<<<<]]]]]]]",

	"+>[<->[->>>>>>>>+<<<<<<<<]]>>>>>>>>[-<<<<<<<<+>>>>>>>>]<<<<<<<<<"
	"[>>[<<->>[->>>>>>>+<<<<<<<]]>>>>>>>[-<<<<<<<+>>>>>>>]<<<<<<<<<[>"
	">>[<<<->>>[->>>>>>+<<<<<<]]>>>>>>[-<<<<<<+>>>>>>]<<<<<<<<<[>>>>["
	"<<<<->>>>[->>>>>+<<<<<]]>>>>>[-<<<<<+>>>>>]<<<<<<<<<[>>>>>[<<<<<"
	"->>>>>[->>>>+<<<<]]>>>>[-<<<<+>>>>]<<<<<<<<<[>>>>>>[<<<<<<->>>>>"
	">[->>>+<<<]]>>>[-<<<+>>>]<<<<<<<<<[>>>>>>>[<<<<<<<->>>>>>>[->>+<"
	"<]]>>[-<<+>>]<<<<<<<<<[->>>>>>>>-<<<<<<<<]>>>>>>>-<<<<<<<]>>>>>>"
	"-<<<<<<]>>>>>-<<<<<]>>>>-<<<<]>>>-<<<]>>-<<]>-<",

	">.<", ">[-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]<<<<<<<,<",

	">[<+>[->>>>>>>>+<<<<<<<<]]>>>>>>>>[-<<<<<<<<+>>>>>>>>]<<<<<<<[<<"
	"+>>[->>>>>>>+<<<<<<<]]>>>>>>>[-<<<<<<<+>>>>>>>]<<<<<<[<<<+>>>[->"
	">>>>>+<<<<<<]]>>>>>>[-<<<<<<+>>>>>>]<<<<<[<<<<+>>>>[->>>>>+<<<<<"
	"]]>>>>>[-<<<<<+>>>>>]<<<<[<<<<<+>>>>>[->>>>+<<<<]]>>>>[-<<<<+>>>"
	">]<<<[<<<<<<+>>>>>>[->>>+<<<]]>>>[-<<<+>>>]<<[<<<<<<<+>>>>>>>[->"
	">+<<]]>>[-<<+>>]<[<<<<<<<<+>>>>>>>>[->+<]]>[-<+>]<<<<<<<<<[[-]",

	">[<+>[->>>>>>>>+<<<<<<<<]]>>>>>>>>[-<<<<<<<<+>>>>>>>>]<<<<<<<[<<"
	"+>>[->>>>>>>+<<<<<<<]]>>>>>>>[-<<<<<<<+>>>>>>>]<<<<<<[<<<+>>>[->"
	">>>>>+<<<<<<]]>>>>>>[-<<<<<<+>>>>>>]<<<<<[<<<<+>>>>[->>>>>+<<<<<"
	"]]>>>>>[-<<<<<+>>>>>]<<<<[<<<<<+>>>>>[->>>>+<<<<]]>>>>[-<<<<+>>>"
	">]<<<[<<<<<<+>>>>>>[->>>+<<<]]>>>[-<<<+>>>]<<[<<<<<<<+>>>>>>>[->"
	">+<<]]>>[-<<+>>]<[<<<<<<<<+>>>>>>>>[->+<]]>[-<+>]<<<<<<<<<]",


    },
    .zero_cell = "[-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]<<<<<<<<<",
    .clean_tmps = ">>>>>>>>>[-]<<<<<<<<<[-]",
}};

/*
 * Clojure translation from BF, runs at about 140,000 instructions per second.
 */
static trivbf clojure[1] = {{
    .name = "clojure",
    .class = L_LINES+C_NUMRLE,
    .gen_hdr =
	";; converted to Clojure, template from:"
"\n"	";; https://github.com/takano32/brainfuck/blob/master/bf2clj.clj"
"\n"
"\n"	"(def pointer 0)"
"\n"	"(defn memoryname [] (str \"m\" pointer))"
"\n"	"(defn get-value-from-memory [] (eval (symbol (memoryname))))"
"\n"	"(defn set-memory? [] (boolean (resolve (symbol (memoryname)))))"
"\n"	"(defn set-or-zero [] (if (set-memory?) (get-value-from-memory) 0))"
"\n"	"(defn set-pointer [set-value] (intern *ns* (symbol (memoryname)) set-value))"
"\n"	"(defn inc-memory [] (set-pointer (+ (set-or-zero) 1)))"
"\n"	"(defn dec-memory [] (set-pointer (- (set-or-zero) 1)))"
"\n"	"(defn inc-pointer [] (def pointer (+ pointer 1)))"
"\n"	"(defn dec-pointer [] (def pointer (- pointer 1)))"
"\n",
    .bf = {
    "(def pointer (+ pointer %d))\n",
    "(def pointer (- pointer %d))\n",
    "(set-pointer (+ (set-or-zero) %d))\n",
    "(set-pointer (- (set-or-zero) %d))\n",
    "(print (char (mod (set-or-zero) 256))) (flush)\n",
    "(let [ch (int (. System/in read))] (if-not (= ch -1) (do (set-pointer ch))))\n",
    "((fn [] "
	"(loop [] "
	    "(if (= (set-or-zero) 0) nil (do\n",
    "(recur))))))\n"
    },
    .wrap_eight_bit = "(set-pointer (mod (set-or-zero) 256))\n",
    .set_cell = "(set-pointer %d)\n",
    .help = "Clojure translation from BF",
}};

static trivbf nasm[1] = {{
    .name = "nasm",
    .class = L_LINES+C_NUMRLE,
    .gen_hdr =
	"; asmsyntax=nasm"
"\n"	"; nasm brainfuck.s && chmod +x brainfuck"
"\n"
"\n"	"BITS 32"
"\n"
"\n"	"memsize equ     0x100000"
"\n"	"orgaddr equ     0x08048000"
"\n"	"        org     orgaddr"
"\n"
"\n"	"; A nice legal ELF header here, bit short, but that's okay."
"\n"	"; Based on the headers from ..."
"\n"	"; http://www.muppetlabs.com/~breadbox/software/tiny/teensy.html"
"\n"	"; but with RX and RW segments. See also ..."
"\n"	"; http://blog.markloiseau.com/2012/05/tiny-64-bit-elf-executables"
"\n"
"\n"	"ehdr:                                   ; Elf32_Ehdr"
"\n"	"        db      0x7F, \"ELF\", 1, 1, 1, 0 ;   e_ident"
"\n"	"        times 8 db      0"
"\n"	"        dw      2                       ;   e_type"
"\n"	"        dw      3                       ;   e_machine"
"\n"	"        dd      1                       ;   e_version"
"\n"	"        dd      _start                  ;   e_entry"
"\n"	"        dd      phdr - $$               ;   e_phoff"
"\n"	"        dd      0                       ;   e_shoff"
"\n"	"        dd      0                       ;   e_flags"
"\n"	"        dw      ehdrsize                ;   e_ehsize"
"\n"	"        dw      phdrsize                ;   e_phentsize"
"\n"	"        dw      2                       ;   e_phnum"
"\n"	"        dw      40                      ;   e_shentsize"
"\n"	"        dw      0                       ;   e_shnum"
"\n"	"        dw      0                       ;   e_shstrndx"
"\n"
"\n"	"ehdrsize        equ     $ - ehdr"
"\n"
"\n"	"phdr:                                   ; Elf32_Phdr"
"\n"	"        dd      1                       ;   p_type"
"\n"	"        dd      0                       ;   p_offset"
"\n"	"        dd      $$                      ;   p_vaddr"
"\n"	"        dd      0                       ;   p_paddr"
"\n"	"        dd      filesize                ;   p_filesz"
"\n"	"        dd      ramsize                 ;   p_memsz"
"\n"	"        dd      5                       ;   p_flags"
"\n"	"        dd      0x1000                  ;   p_align"
"\n"
"\n"	"phdrsize        equ     $ - phdr"
"\n"
"\n"	"        dd      1                       ;   p_type"
"\n"	"        dd      0                       ;   p_offset"
"\n"	"        dd      section..bss.start      ;   p_vaddr"
"\n"	"        dd      0                       ;   p_paddr"
"\n"	"        dd      0                       ;   p_filesz"
"\n"	"        dd      memsize                 ;   p_memsz"
"\n"	"        dd      6                       ;   p_flags"
"\n"	"        dd      0x1000                  ;   p_align"
"\n"
"\n"	"        section .text"
"\n"	"        section .bftext align=64"
"\n"	"        section .bftail align=1"
"\n"
"\n"	"exit_prog:"
"\n"	"        mov     bl, 0           ; Exit status"
"\n"	"        xor     eax, eax"
"\n"	"        inc     eax             ; syscall 1, exit"
"\n"	"        int     0x80            ; exit(0)"
"\n"
"\n"	"        section .bflast align=1"
"\n"	"        section .bss align=4096"
"\n"	"ramsize equ     section..bss.start-orgaddr"
"\n"	"filesize equ    section..bflast.start-orgaddr"
"\n"	"mem:"
"\n"
"\n"	"%macro while 0"
"\n"
"\n"	"    %push   while"
"\n"	"    cmp dh, byte [ecx]"
"\n"	"    jz    %$endw"
"\n"	"    %$begin:"
"\n"
"\n"	"%endmacro"
"\n"
"\n"	"%macro endwhile 0"
"\n"
"\n"	"    cmp dh, byte [ecx]"
"\n"	"    jnz    %$begin"
"\n"	"    %$endw:"
"\n"	"    %pop"
"\n"	"%endmacro"
"\n"
"\n"	"        section .bftext"
"\n"	"_start:"
"\n"	"        xor     eax, eax        ; EAX = 0 ;don't change high bits."
"\n"	"        cdq                     ; EDX = 0 ;sign bit of EAX"
"\n"	"        inc     edx             ; EDX = 1 ;ARG4 for system calls"
"\n"	"        mov     ecx,mem"
"\n",

    .bf = {
	"add ecx, %d\n",
	"sub ecx, %d\n",
	"add byte [ecx], %d\n",
	"sub byte [ecx], %d\n",
	"mov al, 4\nmov ebx, edx\nint 0x80\n",
	"mov al, 3\nxor ebx, ebx\nint 0x80\n",
	"while\n",
	"endwhile\n"
	},
    .set_cell = "mov byte [ecx], %d\n",
    .help = "Nasm x86 assembler",
    .bytesonly = 1
}};

static trivbf nasmmagic[1] = {{
    .name = "nasmqmagic", .name2 = "qmagic",
    .class = L_LINES+C_NUMRLE,
    .gen_hdr =
	"; asmsyntax=nasm"
"\n"	"; nasm brainfuck.s && chmod +x brainfuck"
"\n"
"\n"	"BITS 32"
"\n"	"memsize equ     1048576"
"\n"
"\n"	"orgaddr equ     0x0001000"
"\n"	"        org     orgaddr"
"\n"
"\n"	"        dd      0x006400CC      ; a_info"
"\n"	"        dd      a_text          ; a_text"
"\n"	"        dd      a_data          ; a_data"
"\n"	"        dd      a_bss           ; a_bss"
"\n"	"        dd      0               ; a_syms"
"\n"	"        dd      _start          ; a_entry"
"\n"	"        dd      0               ; a_trsize"
"\n"	"        dd      0               ; a_drsize"
"\n"
"\n"	"%macro while 0"
"\n"
"\n"	"    %push   while"
"\n"	"    cmp dh, byte [ecx]"
"\n"	"    jz    %$endw"
"\n"	"    %$begin:"
"\n"
"\n"	"%endmacro"
"\n"
"\n"	"%macro endwhile 0"
"\n"
"\n"	"    cmp dh, byte [ecx]"
"\n"	"    jnz    %$begin"
"\n"	"    %$endw:"
"\n"	"    %pop"
"\n"	"%endmacro"
"\n"
"\n"	"        section .text"
"\n"	"        section .rodata"
"\n"	"        section .textlib align=32"
"\n"	"        section .bftext align=32"
"\n"	"        section .bftail align=1"
"\n"
"\n"	"exit_prog:"
"\n"	"        mov     bl, 0           ; Exit status"
"\n"	"        xor     eax, eax"
"\n"	"        inc     eax             ; syscall 1, exit"
"\n"	"        int     0x80            ; exit(0)"
"\n"
"\n"	"        section .data align=4096"
"\n"	"        db      0xFF"
"\n"
"\n"	"datapad equ     4096-(($-$$)&4095)"
"\n"	"a_text  equ     $$-orgaddr"
"\n"	"a_data  equ     $-$$"
"\n"	"a_bss   equ     memsize+datapad"
"\n"	"        section .datapad align=1 nobits"
"\n"	"        resb    datapad"
"\n"
"\n"	"        section .bss align=4096"
"\n"	"mem:"
"\n"	"        section .bftext"
"\n"	"_start:"
"\n"	"        xor     eax, eax        ; EAX = 0 ;don't change high bits."
"\n"	"        cdq                     ; EDX = 0 ;sign bit of EAX"
"\n"	"        inc     edx             ; EDX = 1 ;ARG4 for system calls"
"\n"	"        mov     ecx,mem"
"\n",
    .bf = {
	"add ecx, %d\n",
	"sub ecx, %d\n",
	"add byte [ecx], %d\n",
	"sub byte [ecx], %d\n",
	"mov al, 4\nmov ebx, edx\nint 0x80\n",
	"mov al, 3\nxor ebx, ebx\nint 0x80\n",
	"while\n",
	"endwhile\n"
	},
    .set_cell = "mov byte [ecx], %d\n",
    .help = "Nasm x86 assembler",
    .bytesonly = 1
}};

static trivbf matlab[1] = {{
    .name = "matlab", .name2="octave",
    .class = L_LINES+C_NUMRLE,
    .bf = {
	"x = x + %d;\n",
	"x = x - %d;\n",
	"A(x) = bitand(A(x) + %d, 255);\n",
	"A(x) = bitand(A(x) + 256 - %d, 255);\n",
	"fprintf(A(x));\n",
	"getch;\n",
	"while A(x) ~= 0,\n",
	"end;\n"},
    .set_cell = "A(x) = %d;\n",
    .gen_hdr =
	"function bf\n"
	"A = char(zeros(2^16, 1));\n"
	"x = 1;\n"
	"l = -1;\n"
	"y = [];\n",
	"function getch\n"
	"  if l == -1\n"
	"    y=input('','s');\n"
	"    l = 0;\n"
	"  endif\n"
	"  if l >= length(y)\n"
	"    l = -1;\n"
	"    A(x) = 10;\n"
	"  else\n"
	"    l = l + 1;\n"
	"    A(x) = y(l);\n"
	"  endif\n"
	"endfunction\n"
	"endfunction\n",
    .help = "Matlab or Octave translation",
    .bytesonly = 1
}};

/* Language "I use Arch btw" */
static trivbf iuab[1] = {{
    .name = "iuab",
    .class = L_WORDS,
    .bf = { "i", "use", "arch", "linux", "btw", "by", "the", "way" },
    .help = "https://github.com/OverMighty/i-use-arch-btw",
    /* .hashdebug = "gentoo" */
}};

/* Language "BCDFuck" */
static trivbf bcdfuck[1] = {{
    .name = "BCDFuck", .name2 = "bcdfuck",
    .class = L_CHARS,
    .bf = { "0", "A", "1A", "8A7A", "E", "D", "B", "C" },
    .help = "https://esolangs.org/wiki/BCDFuck",
    /* Humm, this has cmds 2..9 which add to the 4bit cell, this table can't
     * cope with that form of RLE. Also BF normally assumes >=8bit cells. */
}};

/* Language "BCDFuck" bytewide. */
static trivbf bcdbyte[1] = {{
    .name = "bcdbyte",
    .class = L_CHARS,
    .bf = { "0000", "AAAA",
	"01AA1AB08A7AC0B8A701AACAA",
	"01AAB08A7AC0B8A708A7AACAA8A7A",
	"000EAAAE", "000DAAAD",
	"01AAB08A7AC0B8A71BA8A7ACABAC0B8A7AA1CCA8A7AB1AA",
	"01AAB08A7AC0B8A71BA8A7ACABAC0B8A7AA1CCA8A7ACA" },
    .help = "Bytewide conversion of https://esolangs.org/wiki/BCDFuck",
    .zero_cell = "000B1ACAAAB1AC",
    .bytesonly = 1,
}};

static trivbf * trivlist[] = {

    rhoprime, cbyte, ook, blub, f__k, f__krle, pogaack, trip, nicebf,
    bc, bc_rle, fish, dotty, lisp0, bewbs, moo, chinese, chinese2,
    zero, yolang, k_on_fuck, petooh, arabic, dc1, dc2, dc3, dc4,
    nyan, atpling, cupid, ternary, pikalang, spoon, troll, roadrunner,
    brainbool, clojure, nasm, nasmmagic, cgalang, brainfuq, emojifuck,
    trigram, cbyte_rle, babylang, matlab, tmcc, heart, iuab, bcdfuck,
    bcdbyte, xifucklang,

    bfout, doubler_12, doubler_copy_LXXH, doubler_12nz, doubler_12r,
    doubler_17a, doubler_17b, doubler_copy, doubler_copynz, octbf,
    doubler_esolang, bfquadz, bfquadnz, bfquad_12, bfquad_12nz,
    bitbf, bitbf8,

0};
