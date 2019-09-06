/* This was the deadbeef brainfuck interpreter.
 * It's now called Trixy (gollum, gollum)
 *
 * Robert de Bath (c) 2014-2019 GPL v2 or later.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

void qrun(void);
void putch(int);
void dump(void);
static void pc(int ch);

void run(void);
struct bfi { int mov; int cmd; int arg; } *pgm = 0;
int pgmlen = 0, on_eof = 1, debug = 0;

enum outtype {
    L_ASCII, L_DECIMAL, L_EXCON, L_ABCD, L_MINIMAL, L_DEADSIMPLE,
    L_BINERDY, L_TICK, L_MSF, L_DOLLAR
    } out_format = L_ASCII;

int col = 0, maxcol = 72, in_ascii = 0, dump_code = 0, zcells = 0;

int main(int argc, char **argv)
{
    FILE * ifd;
    char * fn = 0, *ar0 = argv[0];
    int ch, p = -1, n = -1, j = -1, i = 0;
    for (; argc > 1; argc--, argv++) {
	if (!strcmp(argv[1], "-e")) on_eof = -1;
	else if (!strcmp(argv[1], "-z")) on_eof = 0;
	else if (!strcmp(argv[1], "-n")) on_eof = 1;
	else if (!strcmp(argv[1], "-x")) on_eof = 2;
	else if (!strcmp(argv[1], "-d")) debug = 1;
	else if (!strcmp(argv[1], "-iascii")) in_ascii = 1;
	else if (!strcmp(argv[1], "-ascii")) out_format = L_ASCII;
	else if (!strcmp(argv[1], "-decimal")) out_format = L_DECIMAL;
	else if (!strcmp(argv[1], "-excon")) out_format = L_EXCON;
	else if (!strcmp(argv[1], "-abcd")) out_format = L_ABCD;
	else if (!strcmp(argv[1], "-dead")) out_format = L_DEADSIMPLE;
	else if (!strcmp(argv[1], "-minimal")) out_format = L_MINIMAL;
	else if (!strcmp(argv[1], "-binerdy")) out_format = L_BINERDY;
	else if (!strcmp(argv[1], "-tick")) out_format = L_TICK;
	else if (!strcmp(argv[1], "-msf")) out_format = L_MSF;
	else if (!strcmp(argv[1], "-$")) out_format = L_DOLLAR;
	else if (!strcmp(argv[1], "-dump")) dump_code = 1;
	else if (!strcmp(argv[1], "-zcells")) zcells = 1;
	else if (!strcmp(argv[1], "-#")) debug = 1;
	else if ((argv[1][0] == '-' && argv[1][1] != '\0') || fn) {
	    fprintf(stderr, "%s: Unexpected argument: '%s'\n", ar0, argv[1]);
	    fprintf(stderr, "Usage: %s [options] [file]\n", ar0);
	    fprintf(stderr, "    -e       EOF is -1\n");
	    fprintf(stderr, "    -z       EOF is zero\n");
	    fprintf(stderr, "    -n       EOF leaves the cell alone\n");
	    fprintf(stderr, "    -x       EOF terminates program.\n");
	    fprintf(stderr, "    -#       Enable '#' command for debug.\n");
	    fprintf(stderr, "    -iascii  Input in ASCII.\n");
	    fprintf(stderr, "    -ascii   Output in ASCII.\n");
	    fprintf(stderr, "    -decimal Output in Decimal.\n");
	    fprintf(stderr, "    -excon   Output in EXCON.\n");
	    fprintf(stderr, "    -abcd    Output in ABCD.\n");
	    fprintf(stderr, "    -binerdy Output in Binerdy.\n");
	    fprintf(stderr, "    -tick    Output in Tick.\n");
	    fprintf(stderr, "    -minimal Output in Minimal.\n");
	    fprintf(stderr, "    -$       Output in $.\n");
	    exit(1);
	} else fn = argv[1];
    }
    ifd = fn && strcmp(fn, "-") ? fopen(fn, "r") : stdin;
    if(!ifd) { perror(fn); exit(1); } else if (!in_ascii) {
	while((ch = getc(ifd)) != EOF && (ifd != stdin || ch != '!' || j >= 0 || !i)) {
	    int r = (ch == '<' || ch == '>' || ch == '+' || ch == '-');
	    if (r || ch == '[' || ch == ',' || ch == '.' ||
		(debug && ch == '#') || (ch == ']' && j >= 0)) {
		if (ch == '<') { ch = '>'; r = -r; }
		if (ch == '-') { ch = '+'; r = -r; }
		if (r && p >= 0 && pgm[p].cmd == ch) { pgm[p].arg += r; continue; }
		if (p >= 0 && pgm[p].cmd == '=' && ch == '+')
		{ pgm[p].arg += r; continue; }
		if (p >= 0 && pgm[p].cmd == '>') { pgm[p].mov = pgm[p].arg; }
		else {
		    n++;
		    if (n >= pgmlen-2) pgm = realloc(pgm, (pgmlen = n+99)*sizeof *pgm);
		    if (!pgm) { perror("realloc"); exit(1); }
		    pgm[n].mov = 0;
		}
		pgm[n].cmd = ch; pgm[n].arg = r; p = n;
		if (pgm[n].cmd == '[') { pgm[n].cmd = ' '; pgm[n].arg = j; j = n; }
		else if (pgm[n].cmd == ']') {
		    pgm[n].arg = j; j = pgm[r = j].arg; pgm[r].arg = n; pgm[r].cmd = '[';
		    if (  pgm[n].mov == 0 && pgm[n-1].mov == 0 &&
			  pgm[n-1].cmd == '+' && (pgm[n-1].arg&1) == 1 &&
			  pgm[n-2].cmd == '[') {
			n -= 2; pgm[p = n].cmd = '='; pgm[n].arg = 0;
		    } else if (pgm[n-1].cmd == '[') {
			n--; pgm[p = n].cmd = (pgm[n].arg = pgm[n+1].mov) ? '?' : '!';
		    }
		} else if (pgm[n].cmd == ',') i = 1;
	    }
	    if (j<0 && !i && n>0 && pgm && !dump_code)
	       { pgm[n+1].cmd = 0; pgm[n+1].mov = 0; run(); n=p= -1; }
	}
	if (ifd != stdin) fclose(ifd);
	setbuf(stdout, NULL);
	if (pgm) {
	    pgm[n+1].cmd = 0;
	    if (dump_code) dump(); else run();
	}
    } else {
	while((ch = getc(ifd)) != EOF)
	    putch(ch);
	if (ifd != stdin) fclose(ifd);
    }

    if (col > 0 && out_format != L_ASCII) pc('\n');
    if (col > 0 && out_format == L_ASCII) {
	fflush(stdout);
	fprintf(stderr, "\n");
	col = 0;
    }
    return 0;
}

static unsigned char t[(sizeof(int) > sizeof(short))+USHRT_MAX];

void run(void)
{
    static unsigned char z[(sizeof(int) > sizeof(short))+USHRT_MAX];
    static unsigned short m = 0;
    int n, ch;

    if (!zcells) { qrun() ; return; }

    for(n = 0;; n++) {
	m += pgm[n].mov;
	{
	    switch (pgm[n].cmd)
	    {
	    case '=': z[m] = 1; break;
	    case '+': case '[': case ']': case '.': case ',': case '!':
		if (!z[m]) {
		    z[m] = 1;
		    fprintf(stderr, "Zero cell %d\n", m);
		}
		break;

	    case '?':
		if (!z[m]) {
		    z[m] = 1;
		    fprintf(stderr, "Zero cell %d\n", m);
		}
		while(t[m]) {
		    m += pgm[n].arg;
		    if (!z[m]) {
			z[m] = 1;
			fprintf(stderr, "Zero cell %d\n", m);
		    }
		}
		continue;
	    }
	}
	switch(pgm[n].cmd)
	{
	case 0:    return;
	case '=':  t[m] = pgm[n].arg; break;
	case '+':  t[m] += pgm[n].arg; break;
	case '[':  if (t[m] == 0) n = pgm[n].arg; break;
	case ']':  if (t[m] != 0) n = pgm[n].arg; break;
	case '?':  while(t[m]) m += pgm[n].arg; break;
	case '>':  m += pgm[n].arg; break;
	case '.':  putch(t[m]); break;
	case ',':  t[m] = ((ch = getchar()) != EOF) ? ch : on_eof < 1 ? on_eof : t[m];
	    if (ch == EOF && on_eof == 2) exit(-1);
	    break;
	case '!':  if(t[m]) exit(2); break;
	case '#':
	    fprintf(stderr, "\n%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d\n%*s\n",
		    t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7], t[8], t[9], 4*m+3,
		    "^");
	    break;
	}
    }
}

void qrun(void)
{
    static unsigned short m = 0;
    int n, ch;
    for(n = 0;; n++) {
	m += pgm[n].mov;
	switch(pgm[n].cmd)
	{
	case 0:    return;
	case '=':  t[m] = pgm[n].arg; break;
	case '+':  t[m] += pgm[n].arg; break;
	case '[':  if (t[m] == 0) n = pgm[n].arg; break;
	case ']':  if (t[m] != 0) n = pgm[n].arg; break;
	case '?':  while(t[m]) m += pgm[n].arg; break;
	case '>':  m += pgm[n].arg; break;
	case '.':  putch(t[m]); break;
	case ',':  t[m] = ((ch = getchar()) != EOF) ? ch : on_eof < 1 ? on_eof : t[m];
	    if (ch == EOF && on_eof == 2) exit(-1);
	    break;
	case '!':  if(t[m]) exit(2); break;
	case '#':
	    fprintf(stderr, "\n%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d\n%*s\n",
		    t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7], t[8], t[9], 4*m+3,
		    "^");
	    break;
	}
    }
}

static void
pc(int ch)
{
    if ((col >= maxcol && maxcol) || ch == '\n') {
	putchar('\n');
	col = 0;
	if (ch == ' ' || ch == '\n') ch = 0;
    }
    if (ch) {
	putchar(ch);
	if ((ch&0xC0) != 0x80)		/* Count UTF-8 */
	    col++;
    }
}

void
putch(int v)
{
    static int outbit = 1, outch = 0, outtog = 0;

    switch(out_format)
    {
    case L_ASCII: putchar(v); col = (v != '\n' && v != '\r'); break;
    case L_DECIMAL: printf("%d\n", v); break;

    case L_EXCON:
	// https://esolangs.org/wiki/EXCON
	{
	    int bit = 1;
	    for(bit = 1; bit < 0x100; bit <<= 1) {
		if ((outch & bit) != (v & bit)) {
		    if (bit < outbit) {
			pc(':');
			outbit = 1;
			outch = 0;
			bit = 1;
		    }
		}
		if ((outch & bit) != (v & bit)) {
		    while (bit > outbit) {
			outbit <<= 1;
			pc('<');
		    }
		    outch ^= outbit;
		    pc('^');
		}
	    }
	}
	pc('!');
	break;
    case L_ABCD:
	// https://esolangs.org/wiki/ABCD
	{
	    while(outch < v) { outch++; pc('A'); }
	    while(outch > v) { outch--; pc('B'); }
	}
	pc('D');
	break;
    case L_MINIMAL:
	// https://esolangs.org/wiki/Minimal
	{
	    while(outch < v) { outch++; pc('+'); }
	    while(outch > v) { outch--; pc('-'); }
	}
	pc('.');
	break;
    case L_DEADSIMPLE:
	// https://esolangs.org/wiki/DeadSimple
	{
	    if (v < outch && v+1 < outch-v) { outch = 0; pc('_'); }
	    while(outch < v) { outch++; pc('+'); }
	    while(outch > v) { outch--; pc('-'); }
	}
	pc('S');
	break;
    case L_BINERDY:
	// https://esolangs.org/wiki/Binerdy
	{
	    int i;
	    if (outbit) { pc('0'+outtog); outbit = 0; }
	    {
		while(outch < v) { outch++; pc('0'+outtog); pc('0'+(outtog = !outtog)); }
		while(outch > v) { outch--; pc('0'+outtog); pc('0'+(outtog = !outtog)); pc('0'+(outtog = !outtog)); }
	    }
	    pc('0'+outtog);
	    for(i = 0; i < 10; i++)
		pc('0'+(outtog = !outtog));
	}
	break;
    case L_TICK:
	// https://esolangs.org/wiki/Tick
	{
	    static unsigned char curval[256];
	    static int curcell = 0;
	    int best = 0, best_d = v - curval[0], i;
	    if (best_d < 0) best_d += 256;
	    for (i = 0; i < 256; i++) {
		int diff = v - curval[i];
		if (diff < 0) diff += 256;
		if (diff < best_d) {
		    best_d = diff;
		    best = i;
		}
	    }
	    while(curcell < best) { pc('>'); curcell++; }
	    while(curcell > best) { pc('<'); curcell--; }
	    while(curval[curcell] != v) {
		pc('+');
		curval[curcell]++;
		curval[curcell] &= 0xFF;
	    }
	    pc('.');
	}
	break;
    case L_MSF:
	// https://esolangs.org/wiki/MiniStringFuck
	{
	    while(outch != v) { outch=((outch+1)&0xFF); pc('+'); }
	    pc('.');
	}
	break;
    case L_DOLLAR:
	// https://esolangs.org/wiki/$
	printf("0$+%d\n", v);
	break;

	// https://esolangs.org/wiki/JR
    }
}

void dump(void)
{
    int n;
    for(n = 0; pgm[n].cmd; n++) {
	if (pgm[n].mov > 0 ) printf(">%-4d:", pgm[n].mov);
	else if (pgm[n].mov < 0) printf("<%-4d:", -pgm[n].mov);
	else printf("     :");

	if (pgm[n].cmd == '.' || pgm[n].cmd == ',')
	    printf("%c\n", pgm[n].cmd);
	else if (pgm[n].cmd != '[' && pgm[n].cmd != ']')
	    printf("%c %d\n", pgm[n].cmd, pgm[n].arg);
	else
	    printf("%c %d\n", pgm[n].cmd, pgm[n].arg-n);
    }
}
