/*
 * This is a converter for various Brainfuck equivalents.
 *
 * It also contains the "deadbeef" interpreter for running the converted code.
 */
%{
#include <stdio.h>

#define MAXCOL 72
static void pc(int);
static void commentch(int);
static void pcr(int, int);
static int countch(char *, int);
static void pcs(int, int);

static int uncomment_mode = 0;

%}
    /* Not using these, make GCC STFU. */
%option noinput
%option nounput

    /* Probably case insensitive ... */
%option case-insensitive
%option noyywrap
%option nodefault

%s OOK
%s BLUB
%s POG
%s ALWORD
%x ZZZ
%s FK
%x CCOMMENT
%s CHI

twoc	[A-Za-z*][A-Za-z*]
pock	([ \t\n]*pock!)*

%%
<INITIAL>[]<+,-.>[]	pc(yytext[0]);

<OOK>{
    Ook\.[ \t\n]*Ook\?	pc('>');
    Ook\?[ \t\n]*Ook\.	pc('<');
    Ook\.[ \t\n]*Ook\.	pc('+');
    Ook![ \t\n]*Ook!	pc('-');
    Ook![ \t\n]*Ook\.	pc('.');
    Ook\.[ \t\n]*Ook!	pc(',');
    Ook![ \t\n]*Ook\?	pc('[');
    Ook\?[ \t\n]*Ook!	pc(']');
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

    [ \t\n]+	{int i; for(i=0; i<yyleng; i++) commentch(yytext[i]);}
    .		commentch(yytext[0]);
}

<ALWORD>{
    /* Simple alpha-numeric "words", only one set is active at a time */

    right		pcs('>', 0);
    left		pcs('<', 0);
    up			pcs('+', 0);
    down		pcs('-', 0);
    out			pcs('.', 0);
    in			pcs(',', 0);
    begin		pcs('[', 0);
    end			pcs(']', 0);

    fnord		pcs('>', 1);
    pineal		pcs('<', 1);
    hail		pcs('+', 1);
    eris		pcs('-', 1);
    kallisti		pcs('.', 1);
    chaos		pcs(',', 1);
    23			pcs('[', 1);
    5			pcs(']', 1);

    pipi		pcs('>', 2);
    pichu		pcs('<', 2);
    pi			pcs('+', 2);
    ka			pcs('-', 2);
    pikachu		pcs('.', 2);
    pikapi		pcs(',', 2);
    pika		pcs('[', 2);
    chu			pcs(']', 2);

    once		pcs('>', 3);
    there		pcs('<', 3);
    was			pcs('+', 3);
    a			pcs('-', 3);
    fish		pcs('.', 3);
    dead		pcs(',', 3);
    named		pcs('[', 3);
    fred		pcs(']', 3);

}

<CHI>{
    右		pc('>');
    左		pc('<');
    上		pc('+');
    下		pc('-');
    出		pc('.');
    里		pc(',');
    始		pc('[');
    末		pc(']');
}

[A-Za-z0-9]+	|
[ \t\n]+	{int i; for(i=0; i<yyleng; i++) commentch(yytext[i]);}
.		commentch(yytext[0]);
%%

void addcmd(int);
void run(void);
struct bfi { int mov; int cmd; int arg; } *pgm = 0;
int pgmlen = 0;
int jmp = -1, cp = -1, pp = -1;
int on_eof = 1, runit = 0, debug = 0;

int col = 0, ccol = 0;
int alset = 0;

int main(int argc, char ** argv)
{
    int mode = INITIAL;
    int enable_opts = 1;
    int done_file = 0;

    do
    {
	if(argc>1 && argv[1][0] == '-' && enable_opts) {
	    if (!strcmp(argv[1], "--")) enable_opts = 1;
	    else if (!done_file && !strcmp(argv[1], "-r")) runit = 1;
	    else if (!strcmp(argv[1], "-e")) on_eof = -1;
	    else if (!strcmp(argv[1], "-z")) on_eof = 0;
	    else if (!strcmp(argv[1], "-n")) on_eof = 1;
	    else if (!strcmp(argv[1], "-d")) debug = 1;

	    else if (!strcmp(argv[1], "-ook")) mode = OOK;
	    else if (!strcmp(argv[1], "-blub")) mode = BLUB;
	    else if (!strcmp(argv[1], "-pog")) mode = POG;
	    else if (!strcmp(argv[1], "-fk")) mode = FK;
	    else if (!strcmp(argv[1], "-zzz")) mode = ZZZ;
	    else if (!strcmp(argv[1], "-chi")) mode = CHI;

	    else if (!strcmp(argv[1], "-nice")) { mode = ALWORD; alset = 0; }
	    else if (!strcmp(argv[1], "-eris")) { mode = ALWORD; alset = 1; }
	    else if (!strcmp(argv[1], "-pika")) { mode = ALWORD; alset = 2; }
	    else if (!strcmp(argv[1], "-fish")) { mode = ALWORD; alset = 3; }

	    else {
		fprintf(stderr, "Unknown option %s\n", argv[1]);
	    }
	    argv++, argc--;
	    continue;
	}
	if(argc>1) {
	    if (!(yyin = fopen(argv[1], "r"))) {
		perror(argv[1]);
		exit(1);
	    }
	    yyrestart(yyin);
	}

	BEGIN(mode);
	yylex();
	done_file = 1;

	if(argc>1) {
	    fclose(yyin);
	    argv++, argc--;
	}

	if (col || ccol) putchar('\n'), ccol = col = 0;
    } while(!done_file || argc>1);

    if (pgm) {
	pgm[cp+1].cmd = 0;
	while(jmp>=0)
	    { pp=jmp; jmp=pgm[jmp].arg; pgm[pp].arg=0; pgm[pp].cmd = '+'; }
	setbuf(stdout, NULL);
	run();
    }
    return 0;
}

static void
pcs(int ch, int inset)
{
    if (inset != alset)
    {
	int i;
	for(i=0; i<yyleng; i++)
	    commentch(yytext[i]);
    } else
	pc(ch);
}

static void
pc(int ch)
{
    if (runit) {
	addcmd(ch);
	return;
    }
    if (ccol) putchar('\n'), ccol = col = 0;
    putchar(ch);
    if (++col >= MAXCOL) putchar('\n'), col = 0;
}

static void commentch(int ch)
{
    if (runit) return;
    if ((ccol == 0) && (ch == '\n' || ch == '\t' || ch == ' ')) return;
    if (ch == '.' || ch == ',' || ch == '>' || ch == '<' ||
	ch == '[' || ch == ']' || ch == '+' || ch == '-' ||
	ch == '#')	/* Only output characters we don't use in BF. */
	ch = ':';

    if (col) putchar('\n'), ccol = col = 0;
    if (ccol <  0) ccol += printf("\n");
    if (ccol == 0) ccol += printf("//%s", (ch=='\t'||ch =='\n')?"":" ");
    if (ch == '\n') ccol = -1;
    else {
	ccol++;
	putchar(ch);
    }
}

static void pcr(int ch, int count)
{
    while(count-->0) pc(ch);
}

static int countch(char * s, int ch)
{
    int rv = 0;
    if(s) while(*s) rv += (*s++ == ch);
    return rv;
}

/*****************************************************************************
 * This is the "deadbeef" brainfuck interpreter.
 */
void addcmd(int ch)
{
    int r = (ch == '<' || ch == '>' || ch == '+' || ch == '-');
    if (r || (debug && ch == '#') || (ch == ']' && jmp>=0) ||
	    ch == '[' || ch == ',' || ch == '.') {
	if (ch == '<') {ch = '>'; r = -r;}
	if (ch == '-') {ch = '+'; r = -r;}
	if (r && pp>=0 && pgm[pp].cmd == ch) { pgm[pp].arg += r; return; }
	if (pp>=0 && pgm[pp].cmd == '=' && ch == '+')
	     { pgm[pp].arg += r; return; }
	if (pp>=0 && pgm[pp].cmd == '>') { pgm[pp].mov = pgm[pp].arg; }
	else {
	    cp++;
	    if (cp>= pgmlen-2) pgm = realloc(pgm, (pgmlen=cp+99)*sizeof*pgm);
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
   for(n=0; ; n++) {
      m += pgm[n].mov;
      switch(pgm[n].cmd)
      {
	 case 0:    return;
	 case '=':  t[m] = pgm[n].arg; break;
	 case '+':  t[m] += pgm[n].arg; break;
	 case '[':  if (t[m] == 0) n=pgm[n].arg; break;
	 case ']':  if (t[m] != 0) n=pgm[n].arg; break;
	 case '?':  while(t[m]) m += pgm[n].arg; break;
	 case '>':  m += pgm[n].arg; break;
	 case '.':  putchar(t[m]); break;
	 case ',':  if((ch=getchar())!=EOF) t[m]=ch;
		    else if (on_eof != 1) t[m]=on_eof;
		    break;
	 case '#':
	    fprintf(stderr, "\n%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d\n%*s\n",
	       t[0],t[1],t[2],t[3],t[4],t[5],t[6],t[7],t[8],t[9],4*m+3,"^");
	    break;
      }
   }
}
