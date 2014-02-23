#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

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
#define C_RLE           0x400   /* #define for RLE in the header */

#define L_BASE          (0xFF & langclass)

#define L_WORDS         0       /* Words with spaces */

#define L_CWORDS        (L_WORDS+C_HEADERS)
#define L_CDWORDS       (L_WORDS+C_HEADERS+C_DEFINES)
#define L_CRLE          (L_WORDS+C_HEADERS+C_DEFINES+C_RLE)

#define L_JNWORD        1       /* Words NO spaces */
#define L_CHARS         2       /* Add strings char by char. */

#define L_TOKENS        0x10    /* Print the tokens one per line. */
#define L_RISBF         0x11    /* while (count-->0) risbf(ch); */
#define L_HEADSECKS     0x12    /* headsecks(token, count); */
#define L_BFRLE         0x13    /* bfrle(token, count); */


static const char bf[] = "><+-.,[]";
static const char * bfout[] = { ">", "<", "+", "-", ".", ",", "[", "]" };

/* Language "C" */
static const char * cbyte[] = { "m+=1;", "m-=1;", "++*m;", "--*m;",
		   "write(1,m,1);", "read(0,m,1);", "while(*m){", "}", 0 };

static const char * cbyte_rle[] = { ";m+=1", ";m-=1", ";*m+=1", ";*m-=1",
		   ";write(1,m,1)", ";read(0,m,1)", ";while(*m){", ";}", "+1"};

static const char * cint[] = { "m+=1;", "m-=1;", "++*m;", "--*m;",
	"putchar(*m);", "{int _c=getchar();if(_c!=EOF)*m=_c;}",
	"while(*m){", "}", 0 };

static const char * cint_rle[] = { ";m+=1", ";m-=1", ";*m+=1", ";*m-=1",
	";putchar(*m)", ";{int _c=getchar();if(_c!=EOF)*m=_c;}",
	";while(*m){", ";}", "+1" };

/* Language "ook" */
static const char * ook[] =
		{"Ook. Ook?", "Ook? Ook.", "Ook. Ook.", "Ook! Ook!",
		"Ook! Ook.", "Ook. Ook!", "Ook! Ook?", "Ook? Ook!"};

/* Language "blub" */
static const char *blub[] =
		{"blub. blub?", "blub? blub.", "blub. blub.", "blub! blub!",
		"blub! blub.", "blub. blub!", "blub! blub?", "blub? blub!"};

/* Language "fuck fuck" */
static const char *f__k[] =
    {"folk", "sing", "barb", "teas", "cask", "kerb", "able", "bait"};

/* Language "pogaack" */
static const char * pogaack[] =
		{"pogack!", "pogaack!", "pogaaack!", "poock!",
		"pogaaack?", "poock?", "pogack?", "pogaack?"};

/* Language "triplet" */
static const char * trip[] =
    { "OOI", "IOO", "III", "OOO", "OIO", "IOI", "IIO", "OII" };

/* Language "Descriptive BF" */
static const char * nice[] =
    { "right", "left", "up", "down", "out", "in", "begin", "end" };

static const char * bc[] = { "r", "l", "u", "d", "o", "i", "b", "e", "x" };

/* Order should be "there", "once", "was", "a", "fish", "named", "Fred" */
static const char * fish[] =
    { "once", "there", "was", "a", "fish", "dead", "named", "Fred" };

/* Silly (er) ones. */
static const char * dotty[] =
    { "..", "::", ".:.", ".::", ":.::", ":...", ":.:.", ":..:" };

static const char * lisp2[] =
    { "((", "))", "()(", "())", ")())", ")(((", ")()(", ")(()" };

/* Language COW: Not quite as simple as some commands aren't direct replacements. */
static const char * moo[] = {"moO", "mOo", "MoO", "MOo",
		"MMMMOOMooOOOmooMMM", "OOOMoo", "MOOmoOmOo", "MoOMOomoo"};

/* BF Doubler doubles the cell size. */
static const char * doubler[] =
    {">>>>", "<<<<", ">+<+[>-]>[->>+<]<<", ">+<[>-]>[->>-<]<<-",
    ".", ">>>[-]<<<[-],",
    ">+<[>-]>[->+>[<-]<[<]>[-<+>]]<-" "[+<",
    ">+<[>-]>[->+>[<-]<[<]>[-<+>]]<-" "]<",
    ">[-]>[-]<<"};

/* Some random Chinese words */
static const char *chinese[] =
    { "右", "左", "上", "下", "出", "出", "始", "末" };

/* Ρ″ */
static const char *rhoprime[] =
    { "r′λ", "R", "λR", "r′", "Ρ″", "Ιⁿ", "(", ")" };

/* https://github.com/mescam/zerolang */
static const char *zero[] =
    { "0+", "0-", "0++", "0--", "0.", "0?", "0/", "/0" };

/* Language "nyan" (may need prefix and suffix of "nyan") */
static const char *nyan[] =
    {"anna", "nana", "nnya", "nnna", "anan", "nnay", "annn", "naaa"};

static int langclass = L_CHARS;
static const char ** lang = bfout;
static const char ** c = 0;
static int linefix = EOF;
static int col = 0;
static int maxcol = 72;
static int state = 0;

static int headsecksconv[] = {3, 2, 0, 1, 4, 5, 6, 7 };

static void risbf(int ch);
static void headsecks(int ch, int count);
static void bfrle(int ch, int count);

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-#") == 0) return 1;

    if (strcmp(arg, "-c") == 0) {
	lang = cbyte; langclass = L_CWORDS; return 1;
    } else
    if (strcmp(arg, "-db") == 0 || strcmp(arg, "-double") == 0) {
	lang = doubler; langclass = L_CHARS; return 1;
    } else
    if (strcmp(arg, "-n") == 0 || strcmp(arg, "-nice") == 0) {
	lang = nice; langclass = L_CDWORDS; return 1;
    } else
    if (strcmp(arg, "-mini") == 0) {
	lang = bc; langclass = L_CDWORDS; return 1;
    } else
    if (strcmp(arg, "-f") == 0 || strcmp(arg, "-fish") == 0) {
	lang = fish; langclass = L_CDWORDS; return 1;
    } else
    if (strcmp(arg, "-trip") == 0 || strcmp(arg, "-triplet") == 0) {
	lang = trip; langclass = L_CDWORDS; return 1;
    } else
    if (strcmp(arg, "-ook") == 0) {
	lang = ook; langclass = L_WORDS; return 1;
    } else
    if (strcmp(arg, "-blub") == 0) {
	lang = blub; langclass = L_WORDS; return 1;
    } else
    if (strcmp(arg, "-moo") == 0) {
	lang = moo; langclass = L_JNWORD; return 1;
    } else
    if (strcmp(arg, "-fk") == 0) {
	lang = f__k; langclass = L_CDWORDS; return 1;
    } else
    if (strcmp(arg, "-pog") == 0 || strcmp(arg, "-pogaack") == 0) {
	lang = pogaack; langclass = L_WORDS; return 1;
    } else
    if (strcmp(arg, "-:") == 0) {
	lang = dotty; langclass = L_CHARS; return 1;
    } else
    if (strcmp(arg, "-lisp") == 0) {
	lang = lisp2; langclass = L_CHARS; return 1;
    } else
    if (strcmp(arg, "-chi") == 0 || strcmp(arg, "-chinese") == 0) {
	lang = chinese; langclass = L_JNWORD; return 1;
    } else
    if (strcmp(arg, "-head") == 0) {
	lang = 0; langclass = L_HEADSECKS; return 1;
    } else
    if (strcmp(arg, "-bfrle") == 0) {
	lang = 0; langclass = L_BFRLE; return 1;
    } else
    if (strcmp(arg, "-rho") == 0 || strcmp(arg, "-rhoprime") == 0) {
	lang = rhoprime; langclass = L_JNWORD; return 1;
    } else
    if (strcmp(arg, "-zero") == 0) {
	lang = zero; langclass = L_WORDS; return 1;
    } else
    if (strcmp(arg, "-nyan") == 0) {
	lang = nyan; langclass = L_JNWORD; return 1;
    } else

    if (strcmp(arg, "-risbf") == 0) {
	lang = 0; langclass = L_RISBF; return 1;
    } else
    if (strcmp(arg, "-rle") == 0) {
	lang = bc; langclass = L_CRLE; return 1;
    } else
    if (strcmp(arg, "-dump") == 0) {
	lang = 0; langclass = L_TOKENS; return 1;
    } else

    if (strcmp(arg, "-O") == 0 && L_BASE == L_TOKENS) {
	return 1;
    } else
    if (strncmp(arg, "-w", 2) == 0 && arg[2] >= '0' && arg[2] <= '9') {
	maxcol = atol(arg+2);
	return 1;
    } else
    if (strcmp("-h", arg) ==0) {
	fprintf(stderr, "%s\n",
	"\t"    "-w99    Width to line wrap after, default 72"
	"\n\t"  "-rho    The original 1964 Ρ″ by Corrado Böhm (Rho double prime)"
	"\n\t"  "-double BF to BF translation, cell size doubler."
	"\n\t"  "-c      Plain C"
	"\n\t"  "-rle    Odd RLE C translation"
	"\n\t"  "-nice   Nice memorable C translation."
	"\n\t"  "-mini   Compact C translation."
	"\n\t"  "-double BF to BF translation, cell size doubler."
	"\n\t"  "-fish   There once was a (dead) fish named Fred"
	"\n\t"  "-trip   Triplet like translation"
	"\n\t"  "-ook    Ook!"
	"\n\t"  "-blub   Blub!"
	"\n\t"  "-moo    Cow -- http://www.frank-buss.de/cow.html"
	"\n\t"  "-fk     fuck fuck"
	"\n\t"  "-head   Headsecks."
	"\n\t"  "-:      Dotty"
	"\n\t"  "-lisp   Lisp Zero"
	"\n\t"  "-risbf  RISBF"
	"\n\t"  "-dump   Token dump"
	"\n\t"  "-pog    Pogaack."
	"\n\t"  "-chi    In chinese."
	"\n\t"  "-zero   'zerolang' from mescam on github"
	"\n\t"  "-nyan   'nyan-script' from tommyschaefer on github"
	);
	return 1;
    } else
	return 0;
}

static void
pc(int ch)
{
    if (col>=maxcol && maxcol) {
	if (linefix != EOF) putchar(linefix);
	putchar('\n');
	col = 0;
	if (ch == ' ') ch = 0;
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
    if (L_BASE == L_WORDS && col != 0) pc(' '); else pc(0);

    while (*s) {
	putchar(*s);
	if ((*s&0xC0) != 0x80) /* Count UTF-8 */
	    col++;
	s++;
    }
}

void
outcmd(int ch, int count)
{
    char * p;

    if (ch == '!') {
	if (!(langclass & C_RLE)) {
	    if (bytecell) c = cbyte; else c = cint;
	} else {
	    if (bytecell) c = cbyte_rle; else c = cint_rle;
	}
	if (lang == cbyte) lang = c;
    }

    if (ch == '!' && (langclass & C_HEADERS) != 0) {
	int i;
	if (bytecell)
	    printf("#include<unistd.h>\n");
	else
	    printf("#include<stdio.h>\n");
	if (langclass & C_DEFINES) {
	    for (i=0; i<8; i++)
		printf("#define %s %s\n", lang[i], c[i]);
	    if (langclass & C_RLE) {
		printf("#define %s %s\n", lang[8], c[8]);
		printf("#define _ ;return 0;}\n");
	    } else
		printf("#define _ return 0;}\n");
	}
	if (bytecell)
	    printf("char mem[30000];int main(){register char*m=mem;\n");
	else
	    printf("int mem[30000];int main(){register int*m=mem;\n");
    }

    switch (L_BASE) {
    case L_WORDS:
    case L_JNWORD:
	if (!(p = strchr(bf,ch))) break;
	while(count-->0){
	    ps(lang[p-bf]);
	    if (langclass & C_RLE)
		while(count-->0) ps(lang[8]);
	}
	break;

    case L_CHARS:
	if (!(p = strchr(bf,ch))) break;
	while(count-->0){
	    const char * l = lang[p-bf];
	    while (*l)
		pc(*l++);
	}
	break;

    case L_TOKENS:	printf("%c %d\n", ch, count); col = 0; break;
    case L_RISBF:	while (count-->0) risbf(ch); break;
    case L_HEADSECKS:	headsecks(ch, count); break;
    case L_BFRLE:	bfrle(ch, count); break;
    }

    if (ch == '~') {
	pc(0);
	if (langclass & C_DEFINES)
	    col += printf("%s%s", col?" ":"", "_");
	else if (langclass & C_HEADERS)
	    col += printf("%s%s", col?" ":"", "return 0;}");
	if(col) {
	    if (linefix != EOF) putchar(linefix);
	    putchar('\n');
	}
    }
}

static void
risbf(int ch)
{
    switch(ch) {
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
headsecks(int ch, int count)
{
    char * p;
    int rset;
    static int rnd = 1;

    if (ch == '!') linefix=';';
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

    if (! (p = strchr(bf,ch))) return;

    if (count > 2 && (p-bf) < 4) {
	pc(0);
	col += printf("%d%c", count, ch);
    } else while (count-- > 0)
	pc(ch);
}
