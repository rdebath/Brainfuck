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
#define GEN_RLE         0x400   /* Use RLE token */
#define GEN_HEADER      0x800   /* Generic header and footer. */
#define C_NUMRLE        0x3000  /* RLE using %d in <>+- */
#define C_ADDRLE        0x2000  /* RLE using %d in +- */
#define C_MOVRLE        0x1000  /* RLE using %d in <> */

#define L_BASE          (0xFF & langclass)

#define L_WORDS         0       /* Words with spaces */

#define L_CWORDS        (L_JNWORD+C_HEADERS)
#define L_CDWORDS       (L_WORDS+C_HEADERS+C_DEFINES)
#define L_CRLE          (L_WORDS+C_HEADERS+C_DEFINES+GEN_RLE)

#define L_JNWORD        1       /* Words NO spaces */
#define L_CHARS         2       /* Add strings char by char. */
#define L_BF            3       /* Generated code is BF. */

#define L_TOKENS        0x10    /* Print the tokens one per line. */
#define L_RISBF         0x11    /* risbf(token, count); */
#define L_HEADSECKS     0x12    /* headsecks(token, count); */
#define L_BFRLE         0x13    /* bfrle(token, count); */
#define L_BFXML         0x14    /* bfxml(token, count); */
#define L_UGLYBF        0x15    /* bfugly(token, count); */
#define L_MALBRAIN      0x16    /* malbrain(token, count); */
#define L_HANOILOVE     0x17    /* hanoilove(token, count); */

static const char bf[] = "><+-.,[]";
static const char * bfout[] = { ">", "<", "+", "-", ".", ",", "[", "]", 0 };

/* Language "C" */
static const char * cbyte[] = { "m+=1;", "m-=1;", "++*m;", "--*m;",
		   "write(1,m,1);", "read(0,m,1);", "while(*m) {", "}", 0 };

static const char * cbyte_rle[] = { ";m+=1", ";m-=1", ";*m+=1", ";*m-=1",
		   ";write(1,m,1)", ";read(0,m,1)", ";while(*m){", ";}", "+1"};

static const char * cint[] = { "m+=1;", "m-=1;", "++*m;", "--*m;",
	"putchar(*m);", "{int (_c) = getchar(); if(_c!=EOF) *m=_c; }",
	"while(*m) {", "}", 0 };

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
		"pogaaack?", "poock?", "pogack?", "pogaack?", "pock!"};

/* Language "triplet" */
static const char * trip[] =
    { "001", "100", "111", "000", "010", "101", "110", "011" };

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

static const char * bewbs[] =
    { "(.)(.)", "(.){.}", "(.)[.]", "[.](.)",
      "[.][.]", "{.}{.}", "{.}[.]", "[.]{.}" };

/* Language COW: Not quite as simple as some commands aren't direct replacements. */
static const char * moo[] = {"moO", "mOo", "MoO", "MOo",
	    "MMM MOO Moo OOO moo MMM", "OOO Moo", "MOO moO mOo", "MoO MOo moo"};

/* Some random Chinese words */
static const char *chinese[] =
    { "右", "左", "上", "下", "出", "里", "始", "末" };

/* Ρ″ */
static const char *rhoprime[] =
    { "r′λ", "R", "λR", "r′", "Ρ″", "Ιⁿ", "(", ")" };

/* https://github.com/mescam/zerolang */
static const char *zero[] =
    { "0+", "0-", "0++", "0--", "0.", "0?", "0/", "/0" };

/* Language: yolang; http://yolang.org/ */
static const char *yolang[] =
    { "yo", "YO", "Yo!", "Yo?", "YO!", "yo?", "yo!", "YO?", 0 };

/* Language: けいおんfuck; https://gist.github.com/wasabili/562178 */
static const char *k_on_fuck[] =
    { "うんうんうん", "うんうんたん", "うんたんうん", "うんたんたん",
      "たんうんうん", "たんうんたん", "たんたんうん", "たんたんたん", 0 };

/* dc(1) using an array and a pointer in another variable */
static const char *dc1[] =
{   "lp%d+sp", "lp%d-sp", "lp;a%d+lp:a", "lp;a%d-lp:a",
    "lp;aaP", "", "[lp;a0=q", "lbx]dSbxLbs.",
    "0sp[q]sq", "" };

/* dc(1) using an array and a pointer on the main stack */
static const char *dc2[] =
{ "%d+", "%d-", "dd;a%d+r:a", "dd;a%d-r:a", "d;aaP", "", "[d;a0=q", "lbx]dSbxLbs.",
  "0[q]sq", "" };

/* dc(1) using the main stack and the stack 'L' as the tape */
static const char *dc3[] =
{   "SLz0=z", "LL", "%d+", "%d-", "daP", "", "[lmxd0=q", "lbx]dSbxLbs.",
    "[256+]sM[256%d0>M]sm [q]sq[0]szz", "" };

/* dc(1) using two unbounded variables. */
static const char *dc4[] =
{   "lr256*+sr lld256/sl lmx", "ll256*+sl lrd256/sr lmx",
    "%d+ lmx", "%d- lmx", "daP", "", "[lmxd0=q", "lbx]dSbxLbs.",
    "[256+]sM[256%d0>M]sm [q]sq 0dsldsr", "" };

/* Language "nyan" https://github.com/tommyschaefer/nyan-script */
static const char *nyan[] =
    {"anna", "nana", "nnya", "nnna", "anan", "nnay", "annn", "naaa",
     "nyan", "nyan" };

/* Language XMLfuck */
static const char * bfxml_1[] =
    {	"<ptrinc", "<ptrdec", "<inc", "<dec",
	"<print", "<read", "<while>", "</while>", 0 };

/* Language @! .. http://esolangs.org/wiki/@tention! */
static const char *atpling[] =
    {"@I^;", "@E^;", "@B^;", "@C^;", "$XA^<;", "&XA^>;", "XA^[", "];",
     "D@=; T2=; Q(x{TTT*=})=; 8Q^; T{D0<}; Q,; T,; A(D!x-{D~}`)=;"
     "X0=; I(XX1+=)=; E(XX1-=)=; B(XA^XA^1+=)=; C(XA^XA^1-=)=;", "" };

/* Language Cupid */
static const char * cupid[] = { "->", "<-", "><", "<>", "<<", ">>", "-<", ">-", 0 };

/* Language pikalang -- https://github.com/skj3gg/pikalang */
static const char * pikalang[] =
    {"pipi", "pichu", "pi", "ka", "pikachu", "pikapi", "pika", "chu", 0 };

/* BF Doubler doubles the cell size. */
/* 12 cost, cells in LXXH order, with tmpzero */
static const char * doubler_12[] =
    {">>>>", "<<<<", ">+<+[>-]>[->>+<]<<", ">+<[>-]>[->>-<]<<-",
    ".", ">>>[-]<<<[-],",
    ">+<[>-]>[->+>[<-]<[<]>[-<+>]]<-" "[+<",
    ">+<[>-]>[->+>[<-]<[<]>[-<+>]]<-" "]<",
    ">[-]>[-]<<"};

/* Copy cell cost, cells in LXXH order, with tmpzero */
static const char * doubler_copy_LXXH[] =
    {
    ">>>>", "<<<<",
    ">>+<<+[>>-<<[->+<]]>[-<+>]>[->+<]<<",
    ">>+<<[>>-<<[->+<]]>[-<+>]>[->-<]<<-",
    ".", ">>>[-]<<<[-],",
    "[>+<[->>+<<]]>>[-<<+>>]>[<<+>>[-<+>]]<[->+<]<[[-]<",
    "[>+<[->>+<<]]>>[-<<+>>]>[<<+>>[-<+>]]<[->+<]<]<",
    ">[-]>[-]<<"
    };

/* 12 cost, cells in LXXH order */
static const char * doubler_12nz[] =
    {">>>>", "<<<<", ">+<+[>-]>[->>+<]<<", ">+<[>-]>[->>-<]<<-",
    ".", ">>>[-]<<<[-],",
    ">+<[>-]>[->+>[<-]<[<]>[-<+>]]<-" "[+<",
    ">+<[>-]>[->+>[<-]<[<]>[-<+>]]<-" "]<",
    0};

/* 12 cost, cells in LXXH order, uses "[>]" */
static const char * doubler_12r[] =
    {">>>>", "<<<<", ">>+>+[<-]<[-<<+>]<", ">>+>[<-]<[-<<->]>>-<<<",
    ">>>.<<<", "[-]>>>[-],<<<",
    ">>+>[<-]<[-<+<[>-]>[>]<[->+<]]>-" "[+<<",
    ">>+>[<-]<[-<+<[>-]>[>]<[->+<]]>-" "]<<",
    0};

/* 17 cost, cells in XXLHXX order uses [>] */
static const char * doubler_17a[] =
    {
    ">>>>", "<<<<",
    ">>+[>>+<<<]>>>[>]<[-<->]<+<<<", ">>[>>+<<<]>>>[>]<[-<+>]<-<-<<",
    ">>.<<", ">>>[-]<[-],<<",
    ">>[>>+<<<]>>>[>]<[->+<]<[<<+>>>]<<<[<]>>>>>[-<<<<+>>>>]<<<<" "[[-]<",
    ">>[>>+<<<]>>>[>]<[->+<]<[<<+>>>]<<<[<]>>>>>[-<<<<+>>>>]<<<<" "]<",
    0};

/* 17 cost, cells in XXLHXX order */
static const char * doubler_17b[] =
    {
    ">>>>", "<<<<",
    ">>+[>>+>]<[<<<]>>>[-<->]<+<<<", ">>[>>+>]<[<<<]>>>[-<+>]<-<-<<",
    ">>.<<", ">>>[-]<[-],<<",
    ">>[>>+>]<[<<<]>>>[->+<]<[<<+>>>]<<<[<]>>>>>[-<<<<+>>>>]<<<<" "[[-]<",
    ">>[>>+>]<[<<<]>>>[->+<]<[<<+>>>]<<<[<]>>>>>[-<<<<+>>>>]<<<<" "]<",
    0};

/* Copy cell cost, cells in XLHX order, with tmpzero */
static const char * doubler_copy[] =
    {
    ">>>", "<<<",
    "+>+[<->[->>+<<]]>>[-<<+>>]<<<[->>+<<]",
    "+>[<->[->>+<<]]>>[-<<+>>]<<<[->>-<<]>-<",
    ">.<", ">[-]>[-]<,<",
    ">[<+>[->>+<<]]>>[-<<+>>]<[<<+>>[->+<]]>[-<+>]<<<" "[[-]",
    ">[<+>[->>+<<]]>>[-<<+>>]<[<<+>>[->+<]]>[-<+>]<<<" "]",
    "[-]>>>[-]<<<"
    };

/* Copy cell cost, cells in XLHX order */
static const char * doubler_copynz[] =
    {
    ">>>", "<<<",
    "+>+[<->[->>+<<]]>>[-<<+>>]<<<[->>+<<]",
    "+>[<->[->>+<<]]>>[-<<+>>]<<<[->>-<<]>-<",
    ">.<", ">[-]>[-]<,<",
    ">[<+>[->>+<<]]>>[-<<+>>]<[<<+>>[->+<]]>[-<+>]<<<" "[[-]",
    ">[<+>[->>+<<]]>>[-<<+>>]<[<<+>>[->+<]]>[-<+>]<<<" "]",
    0};

/* Copy cell cost(+), cells in XLHX..X order */
/* http://esolangs.org/wiki/Brainfuck_bitwidth_conversions  */
static const char * doubler_esolang[] =
    {
    ">>>",
    "<<<",
    ">+" "[<+>>>+<<-]<[>+<-]+>>>[<<<->>>[-]]<<<[-" ">>+<<" "]",
    ">[<+>>>+<<-]<[>+<-]+>>>[<<<->>>[-]]<<<[-" ">>-<<" "]>-<",
    ">.<",
    ">[-]>[-]<,<",
    ">[>>+>>>+<<<<<-]>>>>>[<<<<<+>>>>>-]<<<" "[[-]<<<+>>>]<"
	  "[>+>>>+<<<<-]>>>>[<<<<+>>>>-]<<<" "[[-]<<<+>>>]<<<" "[[-]",
    ">[>>+>>>+<<<<<-]>>>>>[<<<<<+>>>>>-]<<<" "[[-]<<<+>>>]<"
	  "[>+>>>+<<<<-]>>>>[<<<<+>>>>-]<<<" "[[-]<<<+>>>]<<<" "]",
    0};

/* Copy cell cost, cells in XLMNHX order */
static const char * bfquadz[] = {
    ">>>>>", "<<<<<",

    "+>+[<->[->>>>+<<<<]]>>>>[-<<<<+>>>>]<<<<<[>>+[<<->>[->>>+<<<]]>>>[-<<<+"
    ">>>]<<<<<[>>>+[<<<->>>[->>+<<]]>>[-<<+>>]<<<<<[->>>>+<<<<]]]",

    "+>[<->[->>>>+<<<<]]>>>>[-<<<<+>>>>]<<<<<[>>[<<->>[->>>+<<<]]>>>[-<<<+>>>"
    "]<<<<<[>>>[<<<->>>[->>+<<]]>>[-<<+>>]<<<<<[->>>>-<<<<]>>>-<<<]>>-<<]>-<",

    ">.<", ">[-]>[-]>[-]>[-]<<<,<",

    ">[<+>[->>>>+<<<<]]>>>>[-<<<<+>>>>]" "<<<[<<+>>[->>>+<<<]]>>>[-<<<+>>>]"
    "<<[<<<+>>>[->>+<<]]>>[-<<+>>]" "<[<<<<+>>>>[->+<]]>[-<+>]" "<<<<<[[-]",

    ">[<+>[->>>>+<<<<]]>>>>[-<<<<+>>>>]" "<<<[<<+>>[->>>+<<<]]>>>[-<<<+>>>]"
    "<<[<<<+>>>[->>+<<]]>>[-<<+>>]" "<[<<<<+>>>>[->+<]]>[-<+>]" "<<<<<]",

    "[-]>>>>>[-]<<<<<"
    };

/* Copy cell cost, cells in XLMNHX order */
static const char * bfquadnz[] = {
    ">>>>>", "<<<<<",

    "+>+[<->[->>>>+<<<<]]>>>>[-<<<<+>>>>]<<<<<[>>+[<<->>[->>>+<<<]]>>>[-<<<+"
    ">>>]<<<<<[>>>+[<<<->>>[->>+<<]]>>[-<<+>>]<<<<<[->>>>+<<<<]]]",

    "+>[<->[->>>>+<<<<]]>>>>[-<<<<+>>>>]<<<<<[>>[<<->>[->>>+<<<]]>>>[-<<<+>>>"
    "]<<<<<[>>>[<<<->>>[->>+<<]]>>[-<<+>>]<<<<<[->>>>-<<<<]>>>-<<<]>>-<<]>-<",

    ">.<", ">[-]>[-]>[-]>[-]<<<,<",

    ">[<+>[->>>>+<<<<]]>>>>[-<<<<+>>>>]" "<<<[<<+>>[->>>+<<<]]>>>[-<<<+>>>]"
    "<<[<<<+>>>[->>+<<]]>>[-<<+>>]" "<[<<<<+>>>>[->+<]]>[-<+>]" "<<<<<[[-]",

    ">[<+>[->>>>+<<<<]]>>>>[-<<<<+>>>>]" "<<<[<<+>>[->>>+<<<]]>>>[-<<<+>>>]"
    "<<[<<<+>>>[->>+<<]]>>[-<<+>>]" "<[<<<<+>>>>[->+<]]>[-<+>]" "<<<<<]",

    0
    };

static int langclass = L_BF;
static const char ** lang = bfout;
static const char ** c = 0;
static int linefix = EOF;
static int col = 0;
static int maxcol = 72;
static int state = 0;
static int bf_mov = 0;

static int headsecksconv[] = {3, 2, 0, 1, 4, 5, 6, 7 };

static int bf_multi = 0, tmp_clean = 1;
struct instruction { int ch; int count; struct instruction * next; } *pgm = 0, *last = 0;
/* Default double and quad to the easiest to prove algorithms. */
const char ** doubler = doubler_copy;
const char ** bfquad = bfquadz;

static void risbf(int ch, int count);
static void headsecks(int ch, int count);
static void bfrle(int ch, int count);
static void bfxml(int ch, int count);
static void bfugly(int ch, int count);
static void malbrain(int ch, int count);
static void hanoilove(int ch, int count);
static void bftranslate(int ch, int count);
static void bfreprint(void);

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-#") == 0) return 1;
    if (strcmp(arg, "-no-default-opt") == 0) return 1;

    if (strcmp(arg, "-c") == 0) {
	lang = cbyte; langclass = L_CWORDS; return 1;
    } else

    if (strcmp(arg, "-single") == 0) {
	bf_multi |= 1;
	lang = bfout; langclass = L_BF; return 1;
    } else
    if (strcmp(arg, "-db") == 0 || strcmp(arg, "-double") == 0) {
	bf_multi |= 2;
	lang = doubler; langclass = L_BF; return 1;
    } else
    if (strcmp(arg, "-quad") == 0) {
	bf_multi |= 4;
	lang = bfquad; langclass = L_BF; return 1;
    } else
    if (strcmp(arg, "-quadnz") == 0) {
	bf_multi |= 4;
	lang = bfquad = bfquadnz; langclass = L_BF; return 1;
    } else
    if (strcmp(arg, "-quadz") == 0) {
	bf_multi |= 4;
	lang = bfquad = bfquadz; langclass = L_BF; return 1;
    } else
    if (strcmp(arg, "-dbl12") == 0) {
	lang = doubler = doubler_12;
	bf_multi |= 2;
	langclass = L_BF; return 1;
    } else
    if (strcmp(arg, "-dbl12r") == 0) {
	lang = doubler = doubler_12r;
	bf_multi |= 2;
	langclass = L_BF; return 1;
    } else
    if (strcmp(arg, "-dbl12nz") == 0) {
	lang = doubler = doubler_12nz;
	bf_multi |= 2;
	langclass = L_BF; return 1;
    } else
    if (strcmp(arg, "-dbl17a") == 0) {
	lang = doubler = doubler_17a;
	bf_multi |= 2;
	langclass = L_BF; return 1;
    } else
    if (strcmp(arg, "-dbl17b") == 0) {
	lang = doubler = doubler_17b;
	bf_multi |= 2;
	langclass = L_BF; return 1;
    } else
    if (strcmp(arg, "-dblcopy") == 0) {
	lang = doubler = doubler_copy;
	bf_multi |= 2;
	langclass = L_BF; return 1;
    } else
    if (strcmp(arg, "-dblcpnz") == 0) {
	lang = doubler = doubler_copynz;
	bf_multi |= 2;
	langclass = L_BF; return 1;
    } else
    if (strcmp(arg, "-dblcp12") == 0) {
	lang = doubler = doubler_copy_LXXH;
	bf_multi |= 2;
	langclass = L_BF; return 1;
    } else
    if (strcmp(arg, "-dbleso") == 0) {
	lang = doubler = doubler_esolang;
	bf_multi |= 2;
	langclass = L_BF; return 1;
    } else

    if (strcmp(arg, "-n") == 0 || strcmp(arg, "-nice") == 0) {
	lang = nice; langclass = L_CDWORDS; return 1;
    } else
    if (strcmp(arg, "-mini") == 0) {
	lang = bc; langclass = L_CDWORDS; return 1;
    } else
    if (strcmp(arg, "-fish") == 0) {
	lang = fish; langclass = L_CDWORDS; return 1;
    } else
    if (strcmp(arg, "-trip") == 0 || strcmp(arg, "-triplet") == 0) {
	lang = trip; langclass = L_JNWORD; return 1;
    } else
    if (strcmp(arg, "-ook") == 0) {
	lang = ook; langclass = L_WORDS; return 1;
    } else
    if (strcmp(arg, "-blub") == 0) {
	lang = blub; langclass = L_WORDS; return 1;
    } else
    if (strcmp(arg, "-moo") == 0 || strcmp(arg, "-cow") == 0) {
	lang = moo; langclass = L_JNWORD; return 1;
    } else
    if (strcmp(arg, "-fk") == 0) {
	lang = f__k; langclass = L_CDWORDS; return 1;
    } else
    if (strcmp(arg, "-pog") == 0 || strcmp(arg, "-pogaack") == 0) {
	lang = pogaack; langclass = L_WORDS+GEN_RLE; return 1;
    } else
    if (strcmp(arg, "-:") == 0) {
	lang = dotty; langclass = L_CHARS; return 1;
    } else
    if (strcmp(arg, "-lisp") == 0) {
	lang = lisp2; langclass = L_CHARS; return 1;
    } else
    if (strcmp(arg, "-bewbs") == 0) {
	lang = bewbs; langclass = L_JNWORD; return 1;
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
    if (strcmp(arg, "-yo") == 0) {
	lang = yolang; langclass = L_JNWORD; return 1;
    } else
    if (strcmp(arg, "-kon") == 0) {
	lang = k_on_fuck; langclass = L_JNWORD; return 1;
    } else
    if (strcmp(arg, "-nyan") == 0) {
	lang = nyan; langclass = L_JNWORD+GEN_HEADER; return 1;
    } else
    if (strcmp(arg, "-xml") == 0) {
	lang = 0; langclass = L_BFXML; return 1;
    } else
    if (strcmp(arg, "-uglybf") == 0) {
	lang = 0; langclass = L_UGLYBF; return 1;
    } else
    if (strcmp(arg, "-@!") == 0) {
	lang = atpling; langclass = L_JNWORD+GEN_HEADER; return 1;
    } else
    if (strcmp(arg, "-cp") == 0 || strcmp(arg, "-cupid") == 0) {
	lang = cupid; langclass = L_JNWORD; return 1;
    } else
    if (strcmp(arg, "-pika") == 0) {
	lang = pikalang; langclass = L_CDWORDS; return 1;
    } else
    if (strcmp(arg, "-dc1") == 0 || strcmp(arg, "-dc") == 0) {
	lang = dc1; langclass = L_JNWORD+GEN_HEADER+C_NUMRLE; return 1;
    } else
    if (strcmp(arg, "-dc2") == 0) {
	lang = dc2; langclass = L_JNWORD+GEN_HEADER+C_NUMRLE; return 1;
    } else
    if (strcmp(arg, "-dc3") == 0) {
	lang = dc3; langclass = L_JNWORD+GEN_HEADER+C_ADDRLE; return 1;
    } else
    if (strcmp(arg, "-dc4") == 0) {
	lang = dc4; langclass = L_JNWORD+GEN_HEADER+C_ADDRLE; return 1;
    } else

    if (strcmp(arg, "-risbf") == 0) {
	lang = 0; langclass = L_RISBF; return 1;
    } else
    if (strcmp(arg, "-rle") == 0) {
	lang = bc; langclass = L_CRLE; return 1;
    } else
    if (strcmp(arg, "-malbrain") == 0) {
	lang = 0; langclass = L_MALBRAIN; return 1;
    } else
    if (strcmp(arg, "-hanoilove") == 0) {
	lang = 0; langclass = L_HANOILOVE; return 1;
    } else
    if (strcmp(arg, "-dump") == 0) {
	lang = 0; langclass = L_TOKENS; return 1;
    } else

    if (strncmp(arg, "-w", 2) == 0 && arg[2] >= '0' && arg[2] <= '9') {
	maxcol = atol(arg+2);
	return 1;
    } else
    if (strcmp("-h", arg) ==0) {
	fprintf(stderr, "%s\n",
	"\t"    "-w99    Width to line wrap after, default 72"
	"\n\t"  "-rho    The original 1964 Ρ″ by Corrado Böhm (Rho double prime)"
	"\n\t"  "-c      Plain C"
	"\n\t"  "-rle    Odd RLE C translation"
	"\n\t"  "-nice   Nice memorable C translation."
	"\n\t"  "-mini   Compact C translation."
	"\n\t"  "-fish   There once was a (dead) fish named Fred"
	"\n\t"  "-trip   Triplet -- http://esolangs.org/wiki/Triplet"
	"\n\t"  "-ook    Ook! -- http://esolangs.org/wiki/Ook!"
	"\n\t"  "-blub   Blub -- http://esolangs.org/wiki/Blub"
	"\n\t"  "-moo    Cow -- http://www.frank-buss.de/cow.html"
	"\n\t"  "-fk     fuck fuck"
	"\n\t"  "-head   Headsecks."
	"\n\t"  "-bfrle  Convert to BF RLE as used by -R."
	"\n\t"  "-:      Dotty"
	"\n\t"  "-lisp   Lisp Zero"
	"\n\t"  "-bewbs  BEWBS"
	"\n\t"  "-risbf  RISBF"
	"\n\t"  "-xml    XML"
	"\n\t"  "-uglybf Ugly BF"
	"\n\t"  "-dump   Token dump"
	"\n\t"  "-pog    Pogaack."
	"\n\t"  "-chi    In chinese."
	"\n\t"  "-zero   'zerolang' from mescam on github"
	"\n\t"  "-yo     'yolang' http://yolang.org/"
	"\n\t"  "-kon    K-on fuck."
	"\n\t"  "-nyan   'nyan-script' from tommyschaefer on github"
	"\n\t"  "-@!     @! from http://esolangs.org/wiki/@tention!"
	"\n\t"  "-pika   Pikalang from https://github.com/skj3gg/pikalang"
	"\n\t"  "-cupid  Cupid from http://esolangs.org/wiki/Cupid"
	"\n\t"  "-malbrain Malbrain translation"
	"\n\t"  "-hanoilove Hanoi Love translation"
	"\n\t"  "-dc     Convert to dc(1) using the first of below."
	"\n\t"  "-dc1      Use an array and a pointer variable."
	"\n\t"  "-dc2      Use an array with the pointer on the stack (not V7)."
	"\n\t"  "-dc3      Store the tape in the main stack and a second one."
	"\n\t"  "-dc4      Use two unbounded variables to store the tape."
	"\n\t"  ""
	"\n\t"  "-single BF to BF translation."
	"\n\t"  "-double BF to BF translation, cell size doubler."
	"\n\t"  "-quad   BF to BF translation, cell size double doubler."
	"\n\t"  "        These can be combined and will autodetect cell size."
	"\n\t"  "        There are also several different variations ..."
	"\n\t"  "-dbl12 -dbl12nz -dbl17a -dbl17b -dblcpnz -dblcopy -dblcp12"
	"\n\t"  "-dbleso -quadz -quadnz"
	);
	return 1;
    } else
	return 0;
}

static void
pc(int ch)
{
    if (L_BASE == L_BF && !keep_dead_code) {
	if (ch == '>') bf_mov++;
	else if (ch == '<') bf_mov--;
	else {
	    int tmp = langclass;
	    langclass = L_CHARS;
	    while (bf_mov>0) {bf_mov--; pc('>'); }
	    while (bf_mov<0) {bf_mov++; pc('<'); }
	    pc(ch);
	    langclass = tmp;
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
    if (L_BASE == L_WORDS && col != 0) pc(' '); else pc(0);

    while (*s) {
	if (*s == ' ') {
	    if (L_BASE == L_WORDS && col != 0) pc(' '); else pc(0);
	} else {
	    putchar(*s);
	    /* Count UTF-8 codepoints. This is easy, but actually wrong */
	    if ((*s&0xC0) != 0x80) {
		col++;
		/* So we add a tiny fix to make Chinese characters double width.
		 * It's still not right, but no longer quite as wrong */
		if ((*s&0xFF) >= 0xE3 && (*s&0xFF) <= 0xEA)
		    col++;
	    }
	}
	s++;
    }
}

static void
pmc(const char * s)
{
    while (*s) pc(*s++);
}

void
outcmd(int ch, int count)
{
    char * p;

    if (ch == '!') {
	if (!(langclass & GEN_RLE)) {
	    if (bytecell) c = cbyte; else c = cint;
	} else {
	    if (bytecell) c = cbyte_rle; else c = cint_rle;
	}
	if (lang == cbyte) lang = c;
    }

    if (ch == '!' && (langclass & C_HEADERS) != 0) {
	int i,j;
	if (bytecell)
	    printf("#include<unistd.h>\n");
	else
	    printf("#include<stdio.h>\n");
	if (langclass & C_DEFINES) {
	    for (j=0; j<8; j++){
		i=(14-j)%8;
		printf("#define %s %s\n", lang[i], c[i]);
	    }
	    if (langclass & GEN_RLE) {
		printf("#define %s %s\n", lang[8], c[8]);
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

    if (ch == '!' && (langclass & GEN_HEADER) != 0)
	ps(lang[8]);

    switch (L_BASE) {
    case L_WORDS:
    case L_JNWORD:
	{
	    int v;
	    if (!(p = strchr(bf,ch))) break;
	    v = p-bf;
	    if ((v>=0 && v<2 && (langclass & C_MOVRLE)) ||
	        (v>=2 && v<4 && (langclass & C_ADDRLE))) {
		char sbuf[256];
		snprintf(sbuf, sizeof(sbuf), lang[v], count);
		ps(sbuf);
		break;
	    }
	    while(count-->0){
		ps(lang[v]);
		if (langclass & GEN_RLE)
		    while(count-->0) ps(lang[8]);
	    }
	}
	break;

    case L_CHARS:
	if (!(p = strchr(bf,ch))) break;
	while(count-->0) pmc(lang[p-bf]);
	break;

    case L_TOKENS:	printf("%c %d\n", ch, count); col = 0; break;
    case L_RISBF:	risbf(ch, count); break;
    case L_HEADSECKS:	headsecks(ch, count); break;
    case L_BF:		bftranslate(ch, count); break;
    case L_BFRLE:	bfrle(ch, count); break;
    case L_BFXML:	bfxml(ch, count); break;
    case L_UGLYBF:	bfugly(ch, count); break;
    case L_MALBRAIN:	malbrain(ch, count); break;
    case L_HANOILOVE:	hanoilove(ch, count); break;
    }

    if (ch == '~' && (langclass & GEN_HEADER) != 0)
	ps(lang[9]);

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

/*
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
 */
static void
bftranslate(int ch, int count)
{
    char * p;
    if ((p = strchr(bf,ch)) || (enable_debug && ch == '#')) {
	if (bf_multi) {
	    struct instruction * n = calloc(1, sizeof*n);
	    if (!n) { perror("bf2multi"); exit(42); }

	    n->ch = ch;
	    n->count = count;
	    if (!last) pgm = n; else last->next = n;
	    last = n;
	} else {
	    if (ch == '>' || ch == '<') tmp_clean = 0;
	    else if (ch == '.' || ch == ',' || ch == '#') ;
	    else if (!tmp_clean && lang[8]) {
		pmc(lang[8]); tmp_clean = 1;
	    }
	    if (ch == '#') pc('\n');
	    if (p)
		while(count-->0) pmc(lang[p-bf]);
	    else
		pc(ch);
	    if (ch == '#') pc('\n');
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
	if ((bf_multi & 7) == 7) {
	    /* This generates 65536 to check for larger than 16bit cells. */
	    pc(0); puts("// This generates 65536 to check for larger than 16bit cells");
	    pmc(">[-]<[-]++[>++++++++<-]>[<++++++++>-]<[>++++++++<-]>[<++++++++>-]<[>++++++++<-]+>[");
	    pmc(">\n\n");

	    pc(0); puts("// This code may be replaced by the original source");
	    lang = bfout; bfreprint();
	    pc('\n'); puts("// to here");

	    pmc("\n<[-]]<\n\n");

	    pc(0); puts("// This section is cell doubling for 16bit cells");
	    /* This generates 256 to check for larger than byte cells. */
	    pmc(">[-]<[-]++++++++[>++++++++<-]>[<++++>-]<[");

	    /* This generates 65536 to check for cells upto 16 bits */
	    pmc("[-]>[-]++[<++++++++>-]<[>++++++++<-]>[<++++++++>-]<[>++++++++<-]>[<++++++++>-]+<[>-<[-]]>[");
	    pmc(">");
	    pmc("\n\n");

	    lang = doubler; bfreprint();

	    pmc("\n\n");
	    pmc("<[-]]<");
	    pmc("[-]]\n\n");

	    pc(0); puts("// This section is cell quadrupling for 8bit cells");
	    /* This generates 256 to check for cells upto 8 bits */
	    pmc(">[-]<[-]++++++++[>++++++++<-]>[<++++>-]+<[>-<[-]]>[>");
	    pmc("\n\n");

	    lang = bfquad; bfreprint();

	    pmc("\n\n");
	    pmc("<[-]]<");
	} else if ((bf_multi & 7) != 5) {
	    /* The two cell size checks here are independent, they can be
	     * reordered or used on their own.
	     */

	    /* This generates 256 to check for larger than byte cells. */
	    pmc(">[-]<[-]++++++++[>++++++++<-]>[<++++>-]");
	    pmc("<[" ">>\n\n");

	    if ((bf_multi&1) == 0) lang = doubler; else lang = bfout;
	    bfreprint();

	    pmc("\n\n<<[-]]\n\n");

	    /* This generates 256 to check for cells upto 8 bits */
	    pmc(">[-]<[-]++++++++[>++++++++<-]>[<++++>-]");
	    pmc("+<[>-<[-]]>[>");

	    pmc("\n\n");

	    if(bf_multi&4) lang = bfquad;
	    else if (bf_multi&2) lang = doubler;
	    bfreprint();

	    pmc("\n\n");
	    pmc("<[-]]<");
	} else {
	    /* Eight bit and thirty two bit. */
	    /* This condition is a bit more difficult to optimise and a bit
	     * smaller than the simple multiply list above.
	     * It checks for binary cells of 16bits or less. */
	    pmc("++>>+++++[-<<[->++++++++<]>[-<+>]>]<");
	    pmc("+<[[-]>>");
	    pmc("\n\n");

	    lang = bfout;
	    bfreprint();

	    /* This is an "else" condition, the code cannot be resequenced */
	    pmc("\n\n");
	    pmc("<[-]<[-]]>[>");
	    pmc("\n\n");

	    lang = bfquad;
	    bfreprint();

	    pmc("\n\n");
	    pmc("<[-]]<");
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
	    else if (!tmp_clean && lang[8]) {
		pmc(lang[8]); tmp_clean = 1;
	    }
	    while(count-->0) pmc(lang[p-bf]);
	} else {
	    if (ch == '#') pc('\n');
	    pc(ch);
	    if (ch == '#') pc('\n');
	}
    }
}

/******************************************************************************/
/*       Now some other small, but not completely trivial tranlations.        */
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

static void
bfxml(int ch, int count)
{
    char * p;

    if (! (p = strchr(bf,ch))) {
	if (ch == '!') {
	    puts("<?xml version=\"1.0\"?>");
	    printf("<fuck bits='%d' wrap='Y'>",
		    (int)(bytecell?8:sizeof(int)*8));
	    if (tapelen > 0)
		printf("<tapes><tape length='%d'/></tapes>\n", tapelen);
	    else
		printf("<tapes><tape/></tapes>\n");
	}
	else if (ch == '~') {
	    printf("</fuck>\n");
	}
	return;
    }

    if ((p-bf) >= 6) {
	printf("%s\n", bfxml_1[p-bf]);
    } else if (count > 1) {
	printf("%s by='%d'/>\n", bfxml_1[p-bf], count);
    } else {
	printf("%s/>\n", bfxml_1[p-bf]);
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
	    pc('\''); // Save reg
	    while(state != 0) {state = (state+1)%4; pc('.');}

	    for(v=count+1,maxbit=0; v; v>>=1,maxbit++)
		;
	    maxbit = (1<<(maxbit-2));

	    // reg = 2
	    pc(','); pc(';');

	    for(v=count+1; maxbit; ) {
		if (v & maxbit) pc(';');
		maxbit >>= 1;
		if (!maxbit) break;
		pc('\''); pc(';');
	    }
	    pc('\''); // Save count

	    while(state != 1) {state = (state+1)%4; pc('.');}
	    pc(','); // pop saved reg

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
	if (col+2>maxcol && maxcol) pc('\n');
	pc('"'); pc('\'');
	break;
    case ',':
	while(state == 3) {state = (state+1)%4; pc('.');}
	if (col+2>maxcol && maxcol) pc('\n');
	pc('"'); pc(',');
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
