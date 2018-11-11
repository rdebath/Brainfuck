/*
 * This is a converter for various Brainfuck equivalents.
 *
 * It also contains the "deadbeef" interpreter for running the converted code.
 */
%{
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define STFU(expression) (expression)	/* Make GCC ... */

#define MAXCOL 72
static void pc(int);
static void commentch(int);
static void pcr(int, int);
static int countch(char *, int);
static void wordlist();

static int uncomment_mode = 0;
static int bfcomment = 0;

#define YY_INPUT(buf,result,max_size) result = bf_prefilter(buf,max_size)
size_t bf_prefilter(char * buf, size_t maxsize);

%}
    /* Not using these, make GCC STFU. */
%option noinput
%option nounput

    /* Probably case insensitive ... */
%option case-insensitive
%option noyywrap
%option nodefault

%s BFRLE
%s BFRLE2
%s OOK
%s BLUB
%s POG
%x FK
%x CCOMMENT
%x BFCOMMENT
%x ZZZ
%x TRIP
%x PETOOH
%x SPOON
%x TROLL
%x CUPID
%x TERNARY
%x BEWBS
%x LISP
%x WORDLIST
%x WSWORDLIST

w	[\t ]*
ws	[\t ]+
wl	[\t\n ]*
num	[0-9]+

twoc	[A-Za-z*][A-Za-z*]
pock	([ \t\n]*pock!)*
ook	B?[Oo]ok

%%
<INITIAL>[]<+,-.#>[]		pc(yytext[0]);

<BFRLE>{
    \[				{
				    BEGIN(BFCOMMENT);
				    bfcomment++;
				    commentch('[');
				}
    =				;
}

<BFRLE2>{
    \[				pc(yytext[0]);
    =				{
				    BEGIN(BFRLE);
				    pcr('=', 0);
				}
}

<BFRLE,BFRLE2>{
    \]				{ BEGIN(BFRLE); pc(yytext[0]); }
    [<+,-.>]			{ BEGIN(BFRLE2); pc(yytext[0]); }

    {num}{w}[-+><=]		{
				    int ch=yytext[yyleng-1];
				    BEGIN(BFRLE2);
				    yytext[yyleng-1] = 0;
				    pcr(ch, atoi(yytext));
				}

    \"[^"\n\\]*\"		{
				    int i;
				    BEGIN(BFRLE2);
				    for(i=1; i<yyleng-1; i++) {
					pcr('=', yytext[i]);
					pc('.');
				    }
				}

    \/\/.*		{int i; for(i=1; i<yyleng; i++) commentch(yytext[i]);}
    "/*"		{ uncomment_mode = YY_START; BEGIN(CCOMMENT); }
}

<BFCOMMENT>{

    \]		{
		    commentch(yytext[0]);
		    bfcomment--;
		    if (bfcomment == 0) {
			BEGIN(BFRLE);
		    }
		}
    \[		{
		    bfcomment++;
		    commentch(yytext[0]);
		}

    \/\/.*	{int i; for(i=1; i<yyleng; i++) commentch(yytext[i]);}
    "/*"	{ uncomment_mode = YY_START; BEGIN(CCOMMENT); }

    [^]\/\n[]+	|
    \/		|
    \n		{int i; for(i=0; i<yyleng; i++) commentch(yytext[i]);}
    <<EOF>>	{   fprintf(stderr, "WARNING: End of file in BF comment\n");
		    yyterminate(); }
}

<OOK>{
    {ook}\.[ \t\n]*{ook}\?	pc('>');
    {ook}\?[ \t\n]*{ook}\.	pc('<');
    {ook}\.[ \t\n]*{ook}\.	pc('+');
    {ook}![ \t\n]*{ook}!	pc('-');
    {ook}![ \t\n]*{ook}\.	pc('.');
    {ook}\.[ \t\n]*{ook}!	pc(',');
    {ook}![ \t\n]*{ook}\?	pc('[');
    {ook}\?[ \t\n]*{ook}!	pc(']');
}

<BLUB>{
    blub\.[ \t\n]*blub\?	pc('>');
    blub\?[ \t\n]*blub\.	pc('<');
    blub\.[ \t\n]*blub\.	pc('+');
    blub![ \t\n]*blub!  	pc('-');
    blub![ \t\n]*blub\. 	pc('.');
    blub\.[ \t\n]*blub! 	pc(',');
    blub![ \t\n]*blub\? 	pc('[');
    blub\?[ \t\n]*blub! 	pc(']');
}

<FK>{
    f{twoc}k!*		pcr('>', yyleng-3);
    s{twoc}g!*		pcr('<', yyleng-3);
    b{twoc}b!*		pcr('+', yyleng-3);
    t{twoc}s!*		pcr('-', yyleng-3);
    c{twoc}k!*		pcr('.', yyleng-3);
    k{twoc}b!*		pcr(',', yyleng-3);
    a{twoc}e!*		pcr('[', yyleng-3);
    b{twoc}t!*		pcr(']', yyleng-3);
    "/*"		{ uncomment_mode = YY_START; BEGIN(CCOMMENT); }
    \n		|
    .		commentch(yytext[0]);
}

<CCOMMENT>{
    "*/"	BEGIN(uncomment_mode);
    [^*\n]+	|
    "*"		|
    \n		{int i; for(i=0; i<yyleng; i++) commentch(yytext[i]);}
    <<EOF>>	{   fprintf(stderr, "WARNING: End of file in /* comment\n");
		    yyterminate(); }
}

<POG>{
    pogack!{pock}		pcr('>', countch(yytext, '!'));
    pogaack!{pock}		pcr('<', countch(yytext, '!'));
    pogaaack!{pock}		pcr('+', countch(yytext, '!'));
    poock!{pock}		pcr('-', countch(yytext, '!'));
    pogaaack\?{pock}		pcr('.', countch(yytext, '!')+1);
    poock\?{pock}		pcr(',', countch(yytext, '!')+1);
    pogack\?{pock}		pcr('[', countch(yytext, '!')+1);
    pogaack\?{pock}		pcr(']', countch(yytext, '!')+1);
}

<ZZZ>{
    "zz"	pc('>');
    "-zz"	pc('<');
    "z"		pc('+');
    "-z"	pc('-');
    "zzz"	pc('.');
    "-zzz"	pc(',');
    "z+z"	pc('[');
    "z-z"	pc(']');

    \001.*	|
    [ \t\n]+	{int i; for(i=0; i<yyleng; i++) commentch(yytext[i]);}
    .		commentch(yytext[0]);
}

<TRIP>{
    001		pc('>');
    100		pc('<');
    111		pc('+');
    000		pc('-');
    010		pc('.');
    101		pc(',');
    110		pc('[');
    011		pc(']');

    [ \t\n]+	{int i; for(i=0; i<yyleng; i++) commentch(yytext[i]);}
    .		commentch(yytext[0]);
}

<PETOOH>{
    (?-i:Kudah)		pc('>');
    (?-i:kudah)		pc('<');
    (?-i:Ko)		pc('+');
    (?-i:kO)		pc('-');
    (?-i:Kukarek)	pc('.');
    (?-i:kukarek)	pc(',');
    (?-i:Kud)		pc('[');
    (?-i:kud)		pc(']');

    [ \t\n]+	{int i; for(i=0; i<yyleng; i++) commentch(yytext[i]);}
    .		commentch(yytext[0]);
}

<SPOON>{
    1		pc('+');
    000		pc('-');
    010		pc('>');
    011		pc('<');
    0011	pc(']');
    00100	pc('[');
    001010	pc('.');
    0010110	pc(',');
    00101110	pc('#');
    00101111	;

    [ \t\n]+	{int i; for(i=0; i<yyleng; i++) commentch(yytext[i]);}
    .		commentch(yytext[0]);
}

<TROLL>{
    ooo		pc('>');
    ool		pc('<');
    olo		pc('+');
    oll		pc('-');
    loo		pc('.');
    lol		pc(',');
    llo		pc('[');
    lll		pc(']');
    tro		;
    "ll."	;

    [ \t\n]+	{int i; for(i=0; i<yyleng; i++) commentch(yytext[i]);}
    .		commentch(yytext[0]);
}

<CUPID>{
    "->"	pc('>');
    "<-"	pc('<');
    "><"	pc('+');
    "<>"	pc('-');
    "<<"	pc('.');
    ">>"	pc(',');
    "-<"	pc('[');
    ">-"	pc(']');
    "--"	pc('#');

    [ \t\n]+	{int i; for(i=0; i<yyleng; i++) commentch(yytext[i]);}
    .		commentch(yytext[0]);
}

<TERNARY>{
    "01"	pc('>');
    "00"	pc('<');
    "11"	pc('+');
    "10"	pc('-');
    "20"	pc('.');
    "21"	pc(',');
    "02"	pc('[');
    "12"	pc(']');
    "22"	pc('#');

    [ \t\n]+	{int i; for(i=0; i<yyleng; i++) commentch(yytext[i]);}
    .		commentch(yytext[0]);
}

<BEWBS>{
    "(.)(.)"		pc('>');
    "(.){.}"		pc('<');
    "(.)[.]"		pc('+');
    "[.](.)"		pc('-');
    "[.][.]"		pc('.');
    "{.}{.}"		pc(',');
    "{.}[.]"		pc('[');
    "[.]{.}"		pc(']');

    \001.*	|
    [ \t\n]+	{int i; for(i=0; i<yyleng; i++) commentch(yytext[i]);}
    .		commentch(yytext[0]);
}

<LISP>{
    "(("		pc('>');
    "))"		pc('<');
    "()("		pc('+');
    "())"		pc('-');
    ")())"		pc('.');
    ")((("		pc(',');
    ")()("		pc('[');
    ")(()"		pc(']');

    [ \t\n]+	{int i; for(i=0; i<yyleng; i++) commentch(yytext[i]);}
    .		commentch(yytext[0]);
}

<WORDLIST>{
    [A-Za-z0-9\200-\377]+	wordlist();

    \001.*	|
    [ \t\n]+	{int i; for(i=0; i<yyleng; i++) commentch(yytext[i]);}
    .		commentch(yytext[0]);
}

<WSWORDLIST>{
    [^ \t\n\001]+	wordlist();

    \001.*	|
    [ \t\n]+	{int i; for(i=0; i<yyleng; i++) commentch(yytext[i]);}
}

\001.*		|
[A-Za-z0-9]+	|
[ \t\n]+	{int i; for(i=0; i<yyleng; i++) commentch(yytext[i]);}
.		commentch(yytext[0]);
%%

void Usage(void);
void addcmd(int, int);
void run(void);
struct bfi { int mov; int cmd; int arg; } *pgm = 0;
int pgmlen = 0;
int jmp = -1, cp = -1, pp = -1;
int on_eof = 1, runit = 0, debug = 0;

int sof = 1, cln = 0, col = 0, ccol = 0;

int quiet = 1;

char * progname = "C";

char * character_prefilter = 0;
char * character_conversion = 0;
int line_cc = EOF;

int sc_pending_off = 0;
char * sc_pending_buf = 0;

char bf[] = "><+-.,[]#";
char *bflist = 0;
int bflist_ic = 1;

size_t (*custom_prefilter)(char *, size_t) = 0;
size_t utf8_conversion(char * buf, size_t max_size);
size_t string_conversion(char * buf, size_t max_size);
size_t tiny_bf_to_bf(char * buf, size_t max_size);
size_t qqq_to_bf(char * buf, size_t max_size);
size_t headsecks_to_bf(char * buf, size_t max_size);

int main(int argc, char ** argv)
{
    int mode = INITIAL;
    int enable_opts = 1;

    char ** filelist = 0;
    int ar, filecount = 0;

    progname = argv[0];
    filelist = calloc(argc, sizeof *filelist);

    while(argc>1)
    {
	if(argv[1][0] == '-' && enable_opts) {
	    if (!strcmp(argv[1], "--")) enable_opts = 1;
	    else if (!strcmp(argv[1], "-r")) runit = 1;
	    else if (!strcmp(argv[1], "-run")) runit = 1;

	    else if (!strcmp(argv[1], "-e")) on_eof = -1;
	    else if (!strcmp(argv[1], "-z")) on_eof = 0;
	    else if (!strcmp(argv[1], "-n")) on_eof = 1;
	    else if (!strcmp(argv[1], "-d")) debug = 1;
	    else if (!strcmp(argv[1], "-q")) quiet = 1;
	    else if (!strcmp(argv[1], "-v")) quiet = 0;

	    else if (!strcmp(argv[1], "-chash")) line_cc = '#';
	    else if (!strcmp(argv[1], "-cslash")) line_cc = '/';
	    else if (!strcmp(argv[1], "-csemi")) line_cc = ';';
	    else if (!strcmp(argv[1], "-nocom")) line_cc = EOF;

	    else if (!strcmp(argv[1], "-bfrle")) { mode = BFRLE; }
	    else if (!strcmp(argv[1], "-R")) { mode = BFRLE; }

	    else if (!strcmp(argv[1], "-ook")) { mode = OOK; line_cc = '/'; }
	    else if (!strcmp(argv[1], "-blub")) mode = BLUB;
	    else if (!strcmp(argv[1], "-pog")) mode = POG;
	    else if (!strcmp(argv[1], "-zzz")) mode = ZZZ;
	    else if (!strcmp(argv[1], "-fk")) {
		mode = FK;
		character_prefilter =
		    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz*/!";
	    }

	    else if (!strcmp(argv[1], "-nice")) {
		mode = WORDLIST;
		bflist = "right,left,up,down,out,in,begin,end";
	    }
	    else if (!strcmp(argv[1], "-eris")) {
		mode = WORDLIST;
		bflist = "fnord,pineal,hail,eris,kallisti,chaos,23,5";
	    }
	    else if (!strcmp(argv[1], "-pika")) {
		mode = WORDLIST;
		bflist = "pipi,pichu,pi,ka,pikachu,pikapi,pika,chu";
	    }
	    else if (!strcmp(argv[1], "-fish")) {
		mode = WORDLIST;
		bflist = "once,there,was,a,fish,dead,named,fred";
	    }
	    else if (!strcmp(argv[1], "-bewbs")) { mode = BEWBS; }
	    else if (!strcmp(argv[1], "-lisp")) { mode = LISP; character_prefilter="()"; }

	    else if (!strcmp(argv[1], "-mini")) {
		mode = INITIAL;
		character_prefilter = "rludoibe";
		character_conversion = bf;
	    }
	    else if (!strcmp(argv[1], "-alphuck")) {
		mode = INITIAL;
		character_prefilter = "aceijops";
		character_conversion = bf;
	    }
	    else if (!strcmp(argv[1], "-chi") || !strcmp(argv[1], "-chinese")) {
		mode = INITIAL;
		character_prefilter = "右左上下出里始末";
		character_conversion = bf;
		custom_prefilter = utf8_conversion;
	    }
	    else if (!strcmp(argv[1], "-ara") || !strcmp(argv[1], "-arabic")) {
		mode = INITIAL;
		character_prefilter = "شسثتصضقف";
		character_conversion = bf;
		custom_prefilter = utf8_conversion;
	    }

	    else if (!strncmp(argv[1], "-bf=", 4)) {
		mode = INITIAL;
		character_prefilter = argv[1]+4;
		character_conversion = bf;
		custom_prefilter = utf8_conversion;
	    }
	    else if (!strncmp(argv[1], "-match=", 7) && argv[1][7]) {
		mode = INITIAL;
		character_prefilter = argv[1]+7;
		character_conversion = bf;
		custom_prefilter = string_conversion;
	    }
	    else if (!strncmp(argv[1], "-al=", 4)) {
		char * s;
		int has_upper = 0, has_lower = 0;
		mode = WORDLIST;
		bflist = strdup(argv[1]+4);
		for(s=bflist; *s; s++) {
		    if(*s==',') continue;
		    if ((*s>='0' && *s<='9') || ((*s & 0x80) != 0)) continue;
		    if (*s>='a' && *s<='z') { has_lower = 1; continue; }
		    if (*s>='A' && *s<='Z') { has_upper = 1; continue; }
		    mode = WSWORDLIST;
		    break;
		}

		if (!has_upper || !has_lower) {
		    bflist_ic = 1;
		    for(s=bflist; *s; s++)
			if (isupper(*s))
			    *s = tolower(*s);
		} else
		    bflist_ic = 0;
	    }

	    else if (!strcmp(argv[1], "-petooh")) {
		mode = PETOOH;
		character_prefilter = "adehkKoOru";
	    }

	    else if (!strcmp(argv[1], "-trip")) {
		mode = TRIP;
		character_prefilter = "01";
	    }

	    else if (!strcmp(argv[1], "-spoon")) {
		mode = SPOON;
		character_prefilter = "01";
	    }

	    else if (!strcmp(argv[1], "-troll")) {
		mode = TROLL;
		character_prefilter = "trol.TROL";
	    }

	    else if (!strcmp(argv[1], "-cupid")) {
		mode = CUPID;
		character_prefilter = "<->";
	    }

	    else if (!strcmp(argv[1], "-ternary")) {
		mode = TERNARY;
		character_prefilter = "012";
	    }

	    else if (!strcmp(argv[1], "-head")) {
		mode = INITIAL;
		custom_prefilter = headsecks_to_bf;
	    }

	    else if (!strcmp(argv[1], "-tinybf")) {
		mode = INITIAL;
		custom_prefilter = tiny_bf_to_bf;
	    }

	    else if (!strcmp(argv[1], "-???") || !strcmp(argv[1], "-qqq")) {
		mode = INITIAL;
		custom_prefilter = qqq_to_bf;
	    }

	    else {
		fprintf(stderr, "Unknown option %s\n", argv[1]);
		Usage();
		exit(1);
	    }
	    argv++, argc--;
	    continue;
	}

	filelist[filecount++] = argv[1];
	argv++, argc--;
    }
    if (filecount == 0)
	filelist[filecount++] = "-";

    for(ar=0; ar<filecount; ar++)
    {
	if(strcmp(filelist[ar], "-")) {
	    if (!(yyin = fopen(filelist[ar], "r"))) {
		perror(filelist[ar]);
		exit(1);
	    }
	    yyrestart(yyin);
	}

	BEGIN(mode);
	yylex();

	if(strcmp(filelist[ar], "-"))
	    fclose(yyin);

	if (col || ccol) putchar('\n'), ccol = col = 0;
	if (sof > 1) { printf("]*/\n"); sof = 0; }
    }

    if (pgm) {
	pgm[cp+1].cmd = 0;
	while(jmp>=0)
	{ pp=jmp; jmp=pgm[jmp].arg; pgm[pp].arg=0; pgm[pp].cmd = '+'; }
	setbuf(stdout, NULL);
	run();
    }
    return 0;
}

void
Usage(void)
{
    fprintf(stderr, "%s: [options] [file]\n", progname);

    fprintf(stderr, "\n");
    fprintf(stderr, "This program converts various trivial brainfuck substitutions\n");
    fprintf(stderr, "into brainfuck and, optionally, runs it.\n");
    fprintf(stderr, "Most of these are defined on https://esolangs.org\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");

    fprintf(stderr, "   -run -r  Run the converted brainfuck\n");
    fprintf(stderr, "   -v       Verbose, try to include original source in output.\n");
    fprintf(stderr, "   -d       Debug, use '#' for a debug character.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "   -e       Use -1 for EOF.\n");
    fprintf(stderr, "   -z       Use 0 for EOF.\n");
    fprintf(stderr, "   -n       Leave the cell unchanged on EOF.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "   -chash   A comment starts at a '#' til the end of line.\n");
    fprintf(stderr, "   -cslash  A comment starts at a '/' til the end of line.\n");
    fprintf(stderr, "   -csemi   A comment starts at a ';' til the end of line.\n");
    fprintf(stderr, "   -nocom   Disable any default line comments.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "   -ook     Input using Ook! with '/' comments. \n");
    fprintf(stderr, "   -bfrle   Input run length encoded BF, decimal repeat prefix, '=' and quoted\n");
    fprintf(stderr, "            strings.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "   -bf='><+-.,[]#'\n");
    fprintf(stderr, "            Input using any single character substitution.\n");
    fprintf(stderr, "            Note: Uses UTF8 codepoints.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "   -al=right,left,up,down,out,in,begin,end\n");
    fprintf(stderr, "            Input using a simple word substitution.\n");
    fprintf(stderr, "            Words must normally be separated by whitespace but if they're\n");
    fprintf(stderr, "            alphanumeric separating by any non-alphanumeric character works.\n");
    fprintf(stderr, "            Note: Uses order: '><+-.,[]#'\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "   -match=,right,left,up,down,out,in,begin,end\n");
    fprintf(stderr, "            Match exact strings without any separation.\n");
    fprintf(stderr, "            The first character after the '=' separates the commands in\n");
    fprintf(stderr, "            the command line option. The commands do not have to have unique\n");
    fprintf(stderr, "            prefixes. Extra words can be added after the standard to block\n");
    fprintf(stderr, "            conversions that might break alignment. The first of those is\n");
    fprintf(stderr, "            translated as a '#' command, the rest are ignored.\n");
    fprintf(stderr, "            Note: Uses order: '><+-.,[]'\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "   -blub    Input using blub.\n");
    fprintf(stderr, "   -pog     Input using pogaack.\n");
    fprintf(stderr, "   -fk      Input using fuck fuck.\n");
    fprintf(stderr, "   -zzz     Input using zzz.\n");
    fprintf(stderr, "   -nice    Input using up, down, left, right, in, out, begin, end.\n");
    fprintf(stderr, "   -eris    Input using BrainFNORD discordian buzzwords.\n");
    fprintf(stderr, "   -pika    Input using Pikalang, sounds made by Pikachu\n");
    fprintf(stderr, "   -fish    Input using There Once was a Fish Named Fred\n");
    fprintf(stderr, "   -bewbs   Input using Bewbs\n");
    fprintf(stderr, "   -chi     Input using some random Chinese\n");
    fprintf(stderr, "   -arabic  Input using some random Arabic letters\n");
    fprintf(stderr, "   -lisp    Input using lots of parentheses\n");
    fprintf(stderr, "   -mini    Input using just the first letters of -nice\n");
    fprintf(stderr, "   -alphuck Input using yet another single letter substitution.\n");
    fprintf(stderr, "   -petooh  Input using Petooh https://github.com/Ky6uk/PETOOH\n");
    fprintf(stderr, "   -trip    Input using Triplet\n");
    fprintf(stderr, "   -tinybf  Input using TinyBF\n");
    fprintf(stderr, "   -troll   Input using Trollscript\n");
    fprintf(stderr, "   -cupid   Input using Cupid\n");
    fprintf(stderr, "   -ternary Input using Ternary\n");
    fprintf(stderr, "   -???     Input using ??? (also -qqq)\n");

//  fprintf(stderr, "--------------------------------------------------------------------------------\n");

}

static void
pc(int ch)
{
    if (runit) {
	addcmd(ch, 1);
	return;
    }
    if (sof) {
	while(cln-->0) putchar(']');
	if (sof == 2) printf("\n]");
	sof = 0;
    }
    if (ch == '#' && !debug) return;
    if (ccol) putchar('\n'), ccol = col = 0;
    putchar(ch);
    if (++col >= MAXCOL) putchar('\n'), col = 0;
}

static void commentch(int ch)
{
    if (runit || quiet) return;
    if (sof == 1 && bfcomment) sof = 0;
    if (sof == 1) { printf("[\n"); sof = 2; }
    if ((ccol == 0) && (ch == '\n' || ch == '\t' || ch == ' ')) return;

    /* Only output characters we don't use in BF. Plus a comment loop. */
    if (sof) {
	if (ch == '[') cln++;
	else if (ch == ']') {
	    if (cln > 0) cln--;
	    else ch = ':';
	}
    } else if (bfcomment == 0 &&
	       (ch == '+' || ch == '-' || ch == '#' || ch == '[' || ch == ']' ||
		ch == '.' || ch == ',' || ch == '>' || ch == '<' ))
	ch = ':';

    if (col) putchar('\n'), ccol = col = 0;
    if (ccol <	0) ccol += printf("\n");
    if (ccol == 0 && !sof && !bfcomment) ccol += printf("//%s", (ch=='\t' || ch =='\n') ? "" : " ");
    if (ch == '\n') ccol = -1;
    else {
	ccol++;
	putchar(ch);
    }
}

static void pcr(int ch, int count)
{
    if (ch == '=') {
	pc('['); pc('-'); pc(']');
	ch = '+';
    }
    if (runit) addcmd(ch, count);
    else while(count-->0) pc(ch);
}

static int countch(char * s, int ch)
{
    int rv = 0;
    if(s) while(*s) rv += (*s++ == ch);
    return rv;
}

static void wordlist()
{
    char *s = bflist, *e;
    int c = 0;
    if (bflist_ic)
    {
        int i;
        for(i=0; i<yyleng; i++)
            if (isupper(yytext[i]))
		yytext[i] = tolower(yytext[i]);
    }
    for(;;)
    {
	for(e = s; *e && *e != ',';) e++;
	if (e-s == yyleng && !strncmp(yytext, s, yyleng)) {
	    pc(bf[c]);
	    return;
	}
	if (!*e) break;
	s = e+1; c++;
    }
    {
        int i;
        for(i=0; i<yyleng; i++)
            commentch(yytext[i]);
    }
}

/*
 * This preprocesses the source before lex gets to see it.
 *
 * The first thing it does is to turn unknown control characters into spaces,
 * this is so I can use them to simulate a changable regex for line comments.
 *
 * The second process is elimination of all non-command characters for
 * variants that don't like embeded spaces. Some of these are simple
 * BF character transliterations so the third process is the ability
 * to change these direct conversions to pure BF in this preprocessing
 * stage.
 */
size_t
bf_prefilter(char * buf, size_t max_size)
{
    size_t n;
    int c = 0xDEADBEEF;
    if (custom_prefilter)
	return custom_prefilter(buf, max_size);
    else if (!character_prefilter && line_cc == EOF) {
	for ( n = 0; n < max_size && (c = getc( yyin )) != EOF && c != '\n';) {
	    if (c>=0 && c<' ' && c!='\t')
		c = ' ';
	    buf[n++] = (char) c;
	}
	if (c == '\n')
	    buf[n++] = (char) c;
    } else {
	/* NB: If we're preprocessing don't bother trying to line buffer. */
	for ( n = 0; n < max_size && (c = getc( yyin )) != EOF;) {
	    char * p;
	    if (c == line_cc) c = '\001';
	    else if (c>=0 && c<' ' && c!='\t' && c!='\n') c = ' ';

	    if (!character_prefilter)
		buf[n++] = (char) c;
	    else if(STFU(p=strchr(character_prefilter, c))) {
		if (character_conversion)
		    buf[n++] = character_conversion[p-character_prefilter];
		else
		    buf[n++] = (char) c;
	    } else if (line_cc != EOF && (c == '\001' || c == '\n'))
		buf[n++] = (char) c;
	}
    }
    if ( c == EOF && ferror( yyin ) )
	YY_FATAL_ERROR( "input in flex scanner failed" );
    return n;
}

/*
 * This preprocesses the source before lex gets to see it.
 *
 * Unlike the previous function the "character_prefilter" variable
 * contains a list of UTF-8 characters that are the BF instructions.
 */
size_t
utf8_conversion(char * buf, size_t max_size)
{
    size_t n;
    int c = 0xDEADBEEF;
    static unsigned char chrbuf[16], *utf = chrbuf;
    static unsigned char UTFlen[] = {
        0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
        4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        /*             4, 4, 4, 5, 5, 5, 5, 6, 6, 7, 8 // not in standard. */
    };

    for ( n = 0; n < max_size && (c = getc( yyin )) != EOF;) {
	if (c == line_cc) { buf[n++] = '\001'; continue; }
	else if (c == '\n' && line_cc != EOF) { buf[n++] = '\n'; continue; }
	else if (c>=0 && c<' ' && c!='\t' && c!='\n') c = ' ';

	c = (unsigned char)c;
	if (c <= 0x7f) {
	    char *m, *r;
	    m = character_prefilter;
	    r = character_conversion;

	    while(*m && *r) {
		if (*m == c) {
		    buf[n++] = *r;
		    break;
		}
		r++; m++;
		while((unsigned char)*m >= 0x80 && (unsigned char)*m <= 0xC0)
		    m++;
	    }

	    utf = chrbuf;
	} else if(c>= 0xC0) {
	    utf = chrbuf; *utf++ = c;
	} else if (utf-chrbuf < 8) {
	    *utf++ = c;
	    if (utf-chrbuf == UTFlen[chrbuf[0]&0x3F]) {
		unsigned char *m;
		char *r;

		m = (unsigned char*) character_prefilter;
		r = character_conversion;
		while(*m && *r) {
		    if (*m == chrbuf[0]) {
			int i, f=1;
			for(i=0; i<UTFlen[chrbuf[0]&0x3F]; i++) {
			    if (chrbuf[i] != m[i] ) {
				f = 0;
				break;
			    }
			}
			if (f) {
			    buf[n++] = *r;
			    break;
			}
		    }

		    r++; m++;
		    while(*m >= 0x80 && *m <= 0xC0)
			m++;
		}
	    }
	}
    }
    return n;
}

/*
 * This preprocesses the source before lex gets to see it.
 *
 * This time the "character_prefilter" variable contains a list of
 * strings which are the BF instructions. They are matched on the
 * basis of "longest wins". (NB: no control characters!)
 *
 * Flex does this much better, but the list needs to be known at compile time.
 * For a large input building a regex would be faster; but large inputs
 * are very uncommon!
 */
size_t
string_conversion(char * buf, size_t max_size)
{
    size_t n;
    int c = 0xDEADBEEF;
    int match_flg, match_id, match_len;
    int cnt;
    char *s, *e;

    if (sc_pending_buf == 0) sc_pending_buf = strdup(character_prefilter);

    for ( n = 0; n < max_size && (c = getc( yyin )) != EOF;) {
	if (c == line_cc) { buf[n++] = '\001'; continue; }
	else if (c == '\n' && line_cc != EOF) { buf[n++] = '\n'; continue; }
	else if (c>=0 && c<' ') c = ' ';

	c = (unsigned char)c;

	/* Multiple whitespace are compacted. */
	if (sc_pending_off>0 && c == ' ' && sc_pending_buf[sc_pending_off-1] == ' ')
	    continue;

	sc_pending_buf[sc_pending_off++] = (char) c;
	match_flg = 0;
	match_id = 0;
	match_len = 0;
	s = character_prefilter+1;
	cnt = 0;

	for(;;)
	{
	    for(e = s; *e && *e != *character_prefilter;) e++;
	    if (e-s <= sc_pending_off && !strncmp(sc_pending_buf, s, e-s)) {
		if ((e-s > match_len && match_flg < 2) || match_flg == 0) {
		    match_flg = 1;
		    match_id = cnt;
		    match_len = e-s;
		}
	    }
	    if (e-s > sc_pending_off &&
		    !strncmp(sc_pending_buf, s, sc_pending_off)) {
		match_flg = 2;
	    }
	    if (!*e) break;
	    s = e+1; cnt++;
	}

	if (match_flg == 1) {
	    if (match_len < sc_pending_off) {
		memmove(sc_pending_buf, sc_pending_buf+match_len, sc_pending_off-match_len);
		sc_pending_off -= match_len;
	    } else
		sc_pending_off = 0;

	    if (match_id < sizeof(bf))
		buf[n++] = bf[match_id];
	    else
		buf[n++] = ' ';
	} else if (match_flg == 0) {
	    if (sc_pending_off > 1) {
		memmove(sc_pending_buf, sc_pending_buf+1, sc_pending_off-1);
		sc_pending_off--;
	    } else
		sc_pending_off = 0;
	}
    }
    if ( c == EOF && ferror( yyin ) )
        YY_FATAL_ERROR( "input in flex scanner failed" );
    return n;
}

/* This character conversion function is a manual conversion from TinyBF */
size_t
tiny_bf_to_bf(char * buf, size_t max_size)
{
    static int tbf_dir = 0;
    static int lookahead[8];
    static int lookahead_len;

    size_t n;
    int c, more;

    for(n=0; n<max_size;) {
	c = getc(yyin);
	if (c == EOF) return n;
	if (c == '\n') {
	    buf[n++] = c;	       /* Keep shorter lines. */
	    continue;
	} else
	if (c != '=' && c != '+' && c != '>' && c != '|')
	    continue;

	lookahead[lookahead_len++] = c;

	for(more=0; !more && n<max_size;)
	{
	    int eat=1;
	    c = *lookahead;
	    switch(c)
	    {
	    case '+': buf[n++] = "+-"[tbf_dir]; break;
	    case '>': buf[n++] = "><"[tbf_dir]; break;
	    case '=':
		switch(lookahead[1])
		{
		case 0: eat=0; break;
		case '=': buf[n++] = '.'; eat=2; break;
		default: tbf_dir = !tbf_dir; break;
		}
		break;
	    case '|':
		if (tbf_dir) {
		    buf[n++] = ']';
		} else
		    switch(lookahead[1])
		    {
		    case 0: eat=0; break;
		    case '=':
			switch(lookahead[2])
			{
			case 0: eat=0; break;
			case '|': buf[n++] = '.'; eat=3; break;
			default: buf[n++] = '['; break;
			}
			break;
		    default: buf[n++] = "[]"[tbf_dir]; break;
		    }
		break;
	    }

	    more = !eat;
	    for(; eat>0; eat--) {
		int i;
		for(i=1; STFU(lookahead[i-1] = lookahead[i]); i++) ;
		lookahead_len--;
	    }
	    if (!*lookahead) more = 1;
	}
    }
    return n;
}

/* This character conversion function is a manual conversion from ??? */
size_t
qqq_to_bf(char * buf, size_t max_size)
{
    static int qqq_state = 0;

    size_t n;
    int c;

    for(n=0; n<max_size;) {
	c = getc(yyin);
	if (c == EOF) return n;

	switch(c)
	{
	case '\n': buf[n++] = '\n'; break;	/* Keep shorter lines. */
	case ';': buf[n++] = '>'; break;
	case '-': buf[n++] = '<'; break;
	case '.': buf[n++] = '+'; break;
	case ',': buf[n++] = '-'; break;
	case '!': buf[n++] = '.'; break;
	case '?': buf[n++] = ','; break;

	case '\'': qqq_state = 1-qqq_state; break;
	case '"':
	    if (qqq_state == 0)
		buf[n++] = '[';
	    else
		buf[n++] = ']';
	}
    }
    return n;
}

/* This character conversion function is a manual conversion from Headsecks */
size_t
headsecks_to_bf(char * buf, size_t max_size)
{
    size_t n;
    int c = 0xDEADBEEF;
    int bskip = 0;
    static unsigned char UTFlen[] = {
        0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
        4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        /*             4, 4, 4, 5, 5, 5, 5, 6, 6, 7, 8 // not in standard. */
    };
    static char hsbf[] = "+-<>.,[]";
    /* static char hsbf[] = "<>-+.,[]"; // Konnichi wa (Kana dialect) */

    for ( n = 0; n < max_size && (c = getc( yyin )) != EOF;) {
	c = (unsigned char)c;

	if (c>=0xC0) {
	    bskip = UTFlen[c&0x3F]-2;
	    continue;
	} else if (bskip>0 && c>=0x80 && c<0xC0) {
	    bskip--;
	    continue;
	}
	bskip = 0;

	c = hsbf[c&7];
	buf[n++] = c;
    }
    return n;
}

/*****************************************************************************
 * This is the "deadbeef" brainfuck interpreter.
 */
void addcmd(int ch, int cnt)
{
    int r = (ch == '<' || ch == '>' || ch == '+' || ch == '-');
    if (!r && cnt != 1) {
	while(cnt-->0) addcmd(ch, 1);
	return;
    }
    if (r) r = cnt;

    if (r || (debug && ch == '#') || (ch == ']' && jmp>=0) ||
	ch == '[' || ch == ',' || ch == '.') {
	if (ch == '<') { ch = '>'; r = -r; }
	if (ch == '-') { ch = '+'; r = -r; }
	if (r && pp>=0 && pgm[pp].cmd == ch) { pgm[pp].arg += r; return; }
	if (pp>=0 && pgm[pp].cmd == '=' && ch == '+')
	{ pgm[pp].arg += r; return; }
	if (pp>=0 && pgm[pp].cmd == '>') { pgm[pp].mov = pgm[pp].arg; }
	else {
	    cp++;
	    if (cp>= pgmlen-2) pgm = realloc(pgm, (pgmlen=cp+99)*sizeof *pgm);
	    if (!pgm) { perror("realloc"); exit(1); }
	    pgm[cp].mov = 0;
	}
	pgm[cp].cmd = ch; pgm[cp].arg = r; pp = cp;
	if (pgm[cp].cmd == '[') { pgm[cp].arg=jmp; jmp = cp; }
	else if (pgm[cp].cmd == ']') {
	    pgm[cp].arg = jmp; jmp = pgm[jmp].arg; pgm[pgm[cp].arg].arg = cp;
	    if (  pgm[cp].mov == 0 && pgm[cp-1].mov == 0 &&
		  pgm[cp-1].cmd == '+' && (pgm[cp-1].arg&1) == 1 &&
		  pgm[cp-2].cmd == '[') {
		cp -= 2; pgm[pp=cp].cmd = '='; pgm[cp].arg = 0;
	    } else if (pgm[cp-1].cmd == '[') {
		cp--; pgm[pp=cp].cmd = '?'; pgm[cp].arg = pgm[cp+1].mov;
	    }
	}
    }
}

void run(void)
{
    static unsigned char t[1+(unsigned short)-1];
    unsigned short m = 0;
    int n, ch;
    for(n=0;; n++) {
	m += pgm[n].mov;
	switch(pgm[n].cmd)
	{
	case 0:	   return;
	case '=':  t[m] = pgm[n].arg; break;
	case '+':  t[m] += pgm[n].arg; break;
	case '[':  if (t[m] == 0) n=pgm[n].arg; break;
	case ']':  if (t[m] != 0) n=pgm[n].arg; break;
	case '?':  while(t[m]) m += pgm[n].arg; break;
	case '>':  m += pgm[n].arg; break;
	case '.':  putchar(t[m]); break;
	case ',':
	    if((ch=getchar())!=EOF) t[m]=ch;
	    else if (on_eof != 1) t[m]=on_eof;
	    break;
	case '#':
	    fprintf(stderr, "\n%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d\n%*s\n",
		    t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7], t[8], t[9],
		    4*m+3, "^");
	    break;
	}
    }
}
