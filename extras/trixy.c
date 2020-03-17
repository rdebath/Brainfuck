/* This is the Trixy brainfuck interpreter.
 *
 * It includes several trixy things you can do with BF.
 * Translations to various, non-tc, languages.
 * A few small interpreters, including the deadbeef embedded variant,
 * a pretty printer and a full trace.
 *
 * Robert de Bath (c) 2014-2019 GPL v2 or later.  */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

void Usage(char *);
void deadbeef(void);
void dumpbeef(void);
void addbeef(int cmd, int count);
void addbeefctrl(int cmd, int count, int disable);
void zc_deadbeef(void);
void longbeef(void);

void dowhile(void);

#ifdef ENABLE_BF2ANY
extern int be_option(const char * arg);
extern void be_outcmd(int mov, int ch, int count);
#endif

void putch(int);
void minibf(char * bfprg);
void microbf(char * bfprg);
void bfprinter(char * b);
void bf_trace(char * bfprg);

static void pc(int ch);
static void lastnl(void);

/* Deadbeef interpreter state */
static unsigned char t[(sizeof(int) > sizeof(short))+USHRT_MAX];
static struct bfi { int mov; int cmd; int arg; } *pgm = 0;
static int jmp = -1, cp = -1, pp = -1;
static int pgmlen = 0, on_eof = 1, debug = 0;

/* Line wrapping state */
static int col = 0, maxcol = 72;

/* Output and input styles, mixed with interpreter type. */
static enum outtype {
    L_DEFAULTOUT,
    L_ASCII, L_UTF8, L_DECIMAL, L_EXCON, L_ABCD, L_MINIMAL, L_DEADSIMPLE,
    L_BINERDY, L_TICK, L_MSF, L_DOLLAR, L_BCD, L_BFTXT3, L_BFTXT4, L_BFTXT5,
    } out_format = L_DEFAULTOUT;

static enum intype {
    L_DEFAULTIN,
#ifdef ENABLE_BF2ANY
    L_BF2,
#endif
    L_DEADBEEF, L_DUMPBEEF, L_ZEROBEEF, L_LONGBEEF, L_DOWHILE,
    L_INP_ASCII,
    L_MINIBF, L_MICROBF, L_PRETTYBF, L_TRACEBF
    } in_format = L_DEFAULTIN;

static
int compact_bf = 0, /* Strip non-bf chars for  non-deabeef interpreters. */
    db_disable = 0; /* Disable "optimisations" of the deadbeef variants */

int main(int argc, char **argv)
{
    FILE * ifd;
    char * fn = 0;
    int ch, ar;
    if (isatty(fileno(stdout))) setbuf(stdout, NULL);

#ifdef ENABLE_BF2ANY
    for (ar=1; ar<argc; ar++) {
	if (strncmp(argv[ar], "-bf2", 4) == 0) {
	    if (in_format == L_DEFAULTIN && be_option(argv[ar])) {
		in_format = L_BF2;
		out_format = L_ASCII;
		db_disable = 1;
	    } else {
		fprintf(stderr,
		    "%s: Unexpected argument: '%s'\n", argv[0], argv[ar]);
		exit(1);
	    }
	}
    }
#endif

    for (ar=1; ar<argc; ar++) {
	switch(0) { default:
	    if (!strcmp(argv[ar], "-e")) on_eof = -1;
	    else if (!strcmp(argv[ar], "-z")) on_eof = 0;
	    else if (!strcmp(argv[ar], "-n")) on_eof = 1;
	    else if (!strcmp(argv[ar], "-x")) on_eof = 2;
	    else if (!strcmp(argv[ar], "-d")) debug = 1;
	    else if (!strcmp(argv[ar], "-#")) debug = 1;
	    else if (!strcmp(argv[ar], "-h")) Usage(argv[0]);
	    else break;
	    continue;
	}

	switch (out_format)
	{
	case L_DEFAULTOUT:
	    if (!strcmp(argv[ar], "-ascii")) out_format = L_ASCII;
	    else if (!strcmp(argv[ar], "-utf8")) out_format = L_UTF8;
	    else if (!strcmp(argv[ar], "-u")) out_format = L_UTF8;
	    else if (!strcmp(argv[ar], "-decimal")) out_format = L_DECIMAL;
	    else if (!strcmp(argv[ar], "-excon")) out_format = L_EXCON;
	    else if (!strcmp(argv[ar], "-abcd")) out_format = L_ABCD;
	    else if (!strcmp(argv[ar], "-dead")) out_format = L_DEADSIMPLE;
	    else if (!strcmp(argv[ar], "-minimal")) out_format = L_MINIMAL;
	    else if (!strcmp(argv[ar], "-binerdy")) out_format = L_BINERDY;
	    else if (!strcmp(argv[ar], "-tick")) out_format = L_TICK;
	    else if (!strcmp(argv[ar], "-msf")) out_format = L_MSF;
	    else if (!strcmp(argv[ar], "-$")) out_format = L_DOLLAR;
	    else if (!strcmp(argv[ar], "-bcd")) out_format = L_BCD;

	    else if (!strcmp(argv[ar], "-bftxt1")) out_format = L_MSF;
	    else if (!strcmp(argv[ar], "-bftxt2")) out_format = L_MINIMAL;
	    else if (!strcmp(argv[ar], "-bftxt3")) out_format = L_BFTXT3;
	    else if (!strcmp(argv[ar], "-bftxt4")) out_format = L_BFTXT4;
	    else if (!strcmp(argv[ar], "-bftxt5")) out_format = L_BFTXT5;
	    else break;
	    continue;
	default:;
	}

	switch (in_format) {
	case L_DEFAULTIN:
	    if (!strcmp(argv[ar], "-deadbeef")) in_format = L_DEADBEEF;
	    else if (!strcmp(argv[ar], "-dump")) in_format = L_DUMPBEEF;
	    else if (!strcmp(argv[ar], "-zcells")) in_format = L_ZEROBEEF;
	    else if (!strcmp(argv[ar], "-longbf")) in_format = L_LONGBEEF;
	    else if (!strcmp(argv[ar], "-dowhile")) in_format = L_DOWHILE;

	    else if (!strcmp(argv[ar], "-iascii")) in_format = L_INP_ASCII;

	    else if (!strcmp(argv[ar], "-minibf")) in_format = L_MINIBF;
	    else if (!strcmp(argv[ar], "-microbf")) in_format = L_MICROBF;
	    else if (!strcmp(argv[ar], "-pretty")) in_format = L_PRETTYBF;
	    else if (!strcmp(argv[ar], "-trace")) in_format = L_TRACEBF;
	    else break;
	    continue;
#ifdef ENABLE_BF2ANY
	case L_BF2:
	    if (strncmp(argv[ar], "-bf2", 4) == 0)
		continue;
#endif
	default:;
	}

	if (strncmp(argv[ar], "-w", 2) == 0 &&
		(argv[ar][2] == 0 ||
		(argv[ar][2] >= '0' && argv[ar][2] <= '9') )) {
	    maxcol = atol(argv[ar]+2);

#ifdef ENABLE_BF2ANY
	    be_option(argv[ar]);
#endif

	}

#ifdef ENABLE_BF2ANY
	else if (in_format == L_BF2 && be_option(argv[ar])) /*OK*/ ;
	else if (in_format == L_DEFAULTIN && out_format == L_DEFAULTOUT &&
		(argv[ar][0] == '-' && argv[ar][1] != '\0') &&
		be_option("-bf2bf") && be_option(argv[ar]) ) {
	    in_format = L_BF2;
	    out_format = L_ASCII;
	    db_disable = 1;
	    continue;
	}
#endif

	else if (!strcmp(argv[ar], "-compact")) compact_bf = 1;
	else if (!strcmp(argv[ar], "-no-rail")) db_disable |= 1;
	else if (!strcmp(argv[ar], "-no-mov")) db_disable |= 2 | 1;
	else if (!strcmp(argv[ar], "-no-set")) db_disable |= 4;
	else if (!strcmp(argv[ar], "-no-rle")) db_disable |= 8 | 2 | 1;

	else if ((argv[ar][0] == '-' && argv[ar][1] != '\0') || fn) {
	    fprintf(stderr,
		    "%s: Unexpected argument: '%s'\n", argv[0], argv[ar]);
	    fprintf(stderr, "Use -h for help\n");
	    exit(2);
	} else fn = argv[ar];
    }

    if (out_format == L_DEFAULTOUT) out_format = L_ASCII;
    if (in_format == L_DEFAULTIN) in_format = L_DEADBEEF;

    ifd = fn && strcmp(fn, "-") ? fopen(fn, "r") : stdin;
    if(!ifd) {
	perror(fn);
	exit(1);

    } else if (in_format < L_INP_ASCII) {
	int inp = 0;
	while((ch = getc(ifd)) != EOF &&
		(ifd != stdin || ch != '!' || jmp >= 0 || !inp)) {
	    inp |= (ch == ',');
	    if (db_disable)
		addbeefctrl(ch, 1, db_disable);
	    else
		addbeef(ch, 1);
	}

	while(jmp>=0) { pp=jmp; jmp=pgm[jmp].arg; pgm[pp].arg=pp; }

	if (ifd != stdin) fclose(ifd);
	if (pgm) {
	    switch(in_format) {
	    case L_DEADBEEF: deadbeef(); break;
	    case L_DUMPBEEF: dumpbeef(); break;
	    case L_LONGBEEF: longbeef(); break;
	    case L_ZEROBEEF: zc_deadbeef(); break;
	    case L_DOWHILE: dowhile(); break;

#ifdef ENABLE_BF2ANY
	    case L_BF2:
		{
		    int n;
		    be_outcmd(0, 0, 0);
		    for(n = 0; pgm[n].cmd; n++) {
			if (pgm[n].cmd == ',' && on_eof <= 0) {
			    be_outcmd(pgm[n].mov, '=', on_eof);
			    be_outcmd(0, pgm[n].cmd, pgm[n].arg);
			} else
			    be_outcmd(pgm[n].mov, pgm[n].cmd, pgm[n].arg);
		    }
		    be_outcmd(0, '~', 0);
		}
		break;
#endif

	    default:
		fprintf(stderr, "Incorrect format selection\n");
		exit(6);
	    }
	}

    } else if (in_format > L_INP_ASCII) {
	int inp=0, n=0, s=0;
	char * b=0;
	while((ch = getc(ifd)) != EOF &&
		(ifd != stdin || ch != '!' || jmp >= 0 || !inp)) {
	    inp |= (ch == ',');
	    if (ch == 0) continue;
	    if (compact_bf && strchr("><+-.,[]#", ch) == 0) continue;
	    if (n+2>s && !(b = realloc(b, s += 1024))) { perror(fn); return 1;}
            b[n++] = ch; b[n] = 0;
	}
	if (ifd != stdin) fclose(ifd);
	if (b) {
	    switch (in_format) {
	    case L_MINIBF: minibf(b); break;
	    case L_MICROBF: microbf(b); break;
	    case L_PRETTYBF: bfprinter(b); break;
	    case L_TRACEBF: bf_trace(b); break;
	    default:
		fprintf(stderr, "Incorrect format selection\n");
		exit(5);
	    }
	    free(b);
	}

    } else {
	while((ch = getc(ifd)) != EOF)
	    putch(ch);
	if (ifd != stdin) fclose(ifd);
    }

    lastnl();
    return 0;
}

void
Usage(char * arg0)
{
    fprintf(stderr, "Usage: %s [options] [file]\n", arg0);
    fprintf(stderr, "        -e         EOF is -1\n");
    fprintf(stderr, "        -z         EOF is zero\n");
    fprintf(stderr, "        -n         EOF leaves the cell alone\n");
    fprintf(stderr, "        -x         EOF terminates program.\n");
    fprintf(stderr, "        -#         Enable '#' command for debug.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "        -deadbeef  The default interpreter.\n");
    fprintf(stderr, "        -zcells    Note all cells that are not cleared.\n");
    fprintf(stderr, "        -dump      Dump the deadbeef execution array.\n");
    fprintf(stderr, "        -longbf    Deadbeef with lots of memory.\n");
    fprintf(stderr, "        -dowhile   Do-while BF or Newbiefuck\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "        -iascii    Input in ASCII.\n");
    fprintf(stderr, "        -minibf    Use a plain, slower BF interpreter.\n");
    fprintf(stderr, "        -microbf   Use a plain, very slow BF interpreter.\n");
    fprintf(stderr, "        -pretty    A slow interpreter that pretty-prints text->bf code.\n");
    fprintf(stderr, "        -trace     A slow interpreter that traces bf code.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "        -ascii     Output in ASCII.\n");
    fprintf(stderr, "        -u -utf8   Output in UTF8.\n");
    fprintf(stderr, "        -decimal   Output in Decimal.\n");
    fprintf(stderr, "        -excon     Output in EXCON.\n");
    fprintf(stderr, "        -abcd      Output in ABCD.\n");
    fprintf(stderr, "        -dead      Output in DeadSimple.\n");
    fprintf(stderr, "        -minimal   Output in Minimal.\n");
    fprintf(stderr, "        -binerdy   Output in Binerdy.\n");
    fprintf(stderr, "        -tick      Output in Tick.\n");
    fprintf(stderr, "        -msf       Output in MiniStringFuck.\n");
    fprintf(stderr, "        -$         Output in $.\n");
    fprintf(stderr, "        -bcd       Output in BCDFuck.\n");

#ifdef ENABLE_BF2ANY
    be_option("-h");
#endif

    exit(1);
}

/*****************************************************************************
 * This is the "deadbeef" brainfuck interpreter.
 */

void addbeef(int ch, int cnt)
{
    int r = (ch == '<' || ch == '>' || ch == '+' || ch == '-');
    if (!r && cnt != 1) {
	while(cnt-->0) addbeef(ch, 1);
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
		cp--; pgm[pp=cp].cmd = (pgm[cp].arg = pgm[cp+1].mov)?'?':'!';
	    }
	}
	pgm[cp+1].cmd = 0;
    }
}

void deadbeef(void)
{
    unsigned short m = 0;
    int n, ch;
    for(n = 0;; n++) {
	m += pgm[n].mov;
	switch(pgm[n].cmd)
	{
	case 0: return;
	case '=': t[m] = pgm[n].arg; break;
	case '+': t[m] += pgm[n].arg; break;
	case '[': if (t[m] == 0) n = pgm[n].arg; break;
	case ']': if (t[m] != 0) n = pgm[n].arg; break;
	case '?': while(t[m]) m += pgm[n].arg; break;
	case '>': m += pgm[n].arg; break;
	case '.': putch(t[m]); break;
	case ',':
	    t[m] = ((ch = getchar()) != EOF) ? ch : on_eof < 1 ? on_eof : t[m];
	    if (ch == EOF && on_eof == 2)
		exit((fprintf(stderr, "^D\n"),0));
	    break;
	case '!': if(t[m]) exit(2); break;
	case '#':
	    fprintf(stderr, "\n%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d\n%*s\n",
		    t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7], t[8], t[9],
		    4*m+3, "^");
	    break;
	}
    }
}

/*****************************************************************************
 * A variant of the deadbeef interpreter that checks for unused cells.
 */
void zc_deadbeef(void)
{
    static unsigned char z[(sizeof(int) > sizeof(short))+USHRT_MAX];
    static unsigned short m = 0;
    int n, ch;

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
	case 0: return;
	case '=': t[m] = pgm[n].arg; break;
	case '+': t[m] += pgm[n].arg; break;
	case '[': if (t[m] == 0) n = pgm[n].arg; break;
	case ']': if (t[m] != 0) n = pgm[n].arg; break;
	case '?': while(t[m]) m += pgm[n].arg; break;
	case '>': m += pgm[n].arg; break;
	case '.': putch(t[m]); break;
	case ',':
	    t[m] = ((ch = getchar()) != EOF) ? ch : on_eof < 1 ? on_eof : t[m];
	    if (ch == EOF && on_eof == 2)
		exit((fprintf(stderr, "^D\n"),0));
	    break;
	case '!':
	    if(!t[m]) break;
	    fprintf(stderr, "STOP command executed.\n");
	    exit(2);
	case '#':
	    fprintf(stderr, "\n%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d\n%*s\n",
		    t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7], t[8], t[9],
		    4*m+3, "^");
	    break;
	}
    }
}

/*****************************************************************************
 * A variant of the deadbeef interpreter with large memory.
 */
#define MINALLOC 16
static char *tp = 0, *te = 0;

static char *
alloc_tape(char *p)
{
   int amt, memoff, off, memsize = te-tp;
   if (p < te) return p;
   memoff = p-tp; off = 0;
   if(memoff>=memsize) off = memoff-memsize;
   amt = (off / MINALLOC + 1) * MINALLOC;
   tp = realloc(tp, (memsize+amt)*sizeof(*tp));
   if (!tp) { perror("realloc"); exit(1); }
   memset(tp+memsize,0,amt*sizeof(*tp));
   te = tp+memsize+amt;
   return tp+memoff;
}

void
longbeef(void)
{
   char * m = alloc_tape(tp);
   int n, ch, ms = 0;
   for(n=0; pgm[n].cmd; n++) {
      if (pgm[n].cmd == '?') {
         if (pgm[n].arg > ms) ms = pgm[n].arg;
         if (-pgm[n].arg > ms) ms = -pgm[n].arg;
      }
   }
   m += ms;
   for(n=0; ; n++) {
      m += pgm[n].mov;
      if (m-ms < tp) {
	 fprintf(stderr, "Run off start of tape\n");
	 exit(1);
      }
      if (m+ms >= te) m=alloc_tape(m+ms)-ms;
      switch(pgm[n].cmd)
      {
	 case 0: free(tp); tp=te=0; return;
	 case '=': *m = pgm[n].arg; break;
	 case '+': *m += pgm[n].arg; break;
	 case '[': if (*m == 0) n=pgm[n].arg; break;
	 case ']': if (*m != 0) n=pgm[n].arg; break;
	 case '?': while(*m) m += pgm[n].arg; break;
	 case '>': m += pgm[n].arg; break;
	 case '.': putch(*m); break;
	 case ',':
	    if((ch=getchar())!=EOF) *m=ch;
	    else if (on_eof != 1) *m=on_eof;
	    break;
	 case '!': if(*m) exit(2); break;
	 case '#':
	    fprintf(stderr,
		    "\n%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d\n%*s\n",
		    tp[0],tp[1],tp[2],tp[3],tp[4],tp[5],tp[6],tp[7],tp[8],tp[9],
		    4*(int)(m-tp)+3,"^");
	    break;
      }
   }
}

/*****************************************************************************
 * A broken BF interpreter; the do-while or newbie-fuck variant.
 */
void dowhile(void)
{
    unsigned short m = 0;
    int n, ch;
    for(n = 0;; n++) {
	m += pgm[n].mov;
	switch(pgm[n].cmd)
	{
	case 0: return;
	case '=': t[m] = pgm[n].arg; break;
	case '+': t[m] += pgm[n].arg; break;
	case '[': break;
	case ']': if (t[m] != 0) n = pgm[n].arg; break;
	case '?': do { m += pgm[n].arg; } while(t[m]); break;
	case '>': m += pgm[n].arg; break;
	case '.': if(t[m]) putch(t[m]); break;
	case ',':
	    t[m] = ((ch = getchar()) != EOF) ? ch : on_eof < 1 ? on_eof : t[m];
	    if (ch == EOF && on_eof == 2) exit(-1);
	    break;
	case '!': if(t[m]) exit(2); break;
	case '#':
	    fprintf(stderr, "\n%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d\n%*s\n",
		    t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7], t[8], t[9],
		    4*m+3, "^");
	    break;
	}
    }
}


/*****************************************************************************
 * Special purpose output, all the various "languages".
 */
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
lastnl()
{
    if (col <= 0) return;
    if ((out_format != L_ASCII && out_format != L_UTF8) ||
	    isatty(fileno(stdout)))
	pc('\n');
}

void
putch(int v)
{
    static int outbit = 1, outch = 0, outtog = 0;

    v &= 0xFF;
    switch(out_format)
    {
    default:
    case L_ASCII: putchar(v); col = (v != '\n' && v != '\r'); break;
    case L_UTF8:
	if (v < 0x80)
	    putchar(v);
	else {
	    putchar(0xC0 | (0x1F & (v >>  6)));
	    putchar(0x80 | (0x3F & (v      )));
	}
	col = (v != '\n' && v != '\r');
	break;

    case L_DECIMAL: printf("%d\n", v); break;
    case L_DOLLAR:
	// https://esolangs.org/wiki/$
	printf("0$+%d\n", v);
	break;

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
	    pc('*');
	}
	break;
    case L_MSF:
	// https://esolangs.org/wiki/MiniStringFuck
	{
	    while(outch != v) { outch=((outch+1)&0xFF); pc('+'); }
	    pc('.');
	}
	break;
    case L_BCD:
	{
	    int nibble;
	    for(nibble = 4; nibble>=0; nibble-=4) {
		int d, n = (v>>nibble) & 0xF;
		d = n - outch;
		if (d<0) d += 0x10;
		if (d>=9) { pc('0'+d/2); pc('A'); d-= d/2; }
		if (d>0) { pc('0'+d); pc('A'); }
		outch = n;
		pc('E');
	    }
	    /* No need to add padding characters as the result of a byte
	     * is always and even number of nibbles. */
	}
	break;

    case L_BFTXT3:
	// Similar to ... https://esolangs.org/wiki/Tick
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
    case L_BFTXT4:
	// Similar to ... https://esolangs.org/wiki/DeadSimple
	{
	    if (v < outch && v+3 < outch-v)
		{ outch = 0; pc('['); pc('-'); pc(']'); }
	    while(outch < v) { outch++; pc('+'); }
	    while(outch > v) { outch--; pc('-'); }
	}
	pc('.');
	break;
    case L_BFTXT5:
	// Similar to ... https://esolangs.org/wiki/DeadSimple
	{
	    if (v < outch && v+1 < outch-v) { outch = 0; pc('>'); }
	    while(outch < v) { outch++; pc('+'); }
	    while(outch > v) { outch--; pc('-'); }
	}
	pc('.');
	break;

	// https://esolangs.org/wiki/JR
    }
}

/*****************************************************************************
 * Routine to dump the deadbeef program array.
 */
void dumpbeef(void)
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

/*****************************************************************************
 * This is a limitable encoder for the deadbeef array.
 * Also some comments.
 */

void addbeefctrl(int ch, int cnt, int disable)
{
    int r = (ch == '<' || ch == '>' || ch == '+' || ch == '-');
    if (((disable&8) != 0 || !r) && cnt != 1 ) {
	while(cnt-->0) addbeefctrl(ch, 1, disable);
	return;
    }
    if (r) r = cnt;

    if (r || (debug && ch == '#') || (ch == ']' && jmp>=0) ||
	ch == '[' || ch == ',' || ch == '.') {

	/* We merge '<' with '>' and '-' with '+' */
	if (ch == '<') { ch = '>'; r = -r; }
	if (ch == '-') { ch = '+'; r = -r; }

	if ((disable&8) == 0) {
	    /* Do the '<', '>', '-', '+' merging */
	    if (r && pp>=0 && pgm[pp].cmd == ch) { pgm[pp].arg += r; return; }

	    /* If the previous is a set '=' add in any '+' */
	    if (pp>=0 && pgm[pp].cmd == '=' && ch == '+')
	    { pgm[pp].arg += r; return; }
	}

	/* Normally any '>' are added into the next instruction */
	if (pp>=0 && pgm[pp].cmd == '>' && (disable&(2|8)) == 0)
	    pgm[pp].mov = pgm[pp].arg;
	else {
	    /* Make sure there's enough space for the new instruction */
	    cp++;
	    if (cp>= pgmlen-2) pgm = realloc(pgm, (pgmlen=cp+99)*sizeof *pgm);
	    if (!pgm) { perror("realloc"); exit(1); }
	    pgm[cp].mov = 0;
	}

	/* Save the new instruction; note, '>' does go through this. */
	pgm[cp].cmd = ch; pgm[cp].arg = r; pp = cp;
	if (pgm[cp].cmd == '[') {
	    /* Save the location of the most recent open '[' and stack the
	     * previous value as a linked list in the program array */
	    pgm[cp].arg=jmp;
	    jmp = cp;
	}
	else if (pgm[cp].cmd == ']') {
	    /* Unstack and link up the most recent jump target */
	    pgm[cp].arg = jmp; jmp = pgm[jmp].arg; pgm[pgm[cp].arg].arg = cp;

	    /* Are the last three instructions '[-]' ? */
	    if (  pgm[cp].mov == 0 && pgm[cp-1].mov == 0 &&
		  pgm[cp-1].cmd == '+' && (pgm[cp-1].arg&1) == 1 &&
		  pgm[cp-2].cmd == '[' && (disable&4) == 0) {
		cp -= 2; pgm[pp=cp].cmd = '='; pgm[cp].arg = 0;

	    /* This is a specific check for '[]' or the STOP command */
	    } else if (pgm[cp-1].cmd == '[' && pgm[cp].mov == 0) {
		cp--; pgm[pp=cp].cmd = '!';

	    /* This code checks for '[>]', '[<]' and '[]' */
	    } else if (pgm[cp-1].cmd == '[' && (disable&1) == 0) {
		cp--; pgm[pp=cp].cmd = (pgm[cp].arg = pgm[cp+1].mov)?'?':'!';
	    }
	}
	pgm[cp+1].cmd = 0;
    }
}

/*****************************************************************************
 * Another BF interprer; slow but uses a stack for the loops.
 */
void minibf(char * b)
{
    char *p, *jmpstk[999];
    int c, i=0, sp = 0;
    unsigned short m=0;

    for(p=b;*p;p++)switch(*p) {
        case '>': m++;break;
        case '<': m--;break;
        case '+': t[m]++;break;
        case '-': t[m]--;break;
        case '.': putch(t[m]);break;
        case ',': if((c=getchar())!=EOF)t[m]=c;break;
        case '[': if(t[m]==0) while((i+=(*p=='[')-(*p==']'))&&p[1]) p++;
                  else jmpstk[sp++] = p;
                  break;
        case ']': if(sp) { if(t[m]!=0) p=jmpstk[sp-1]; else sp--;} break;
    }
}

/*****************************************************************************
 * Another BF interprer; very small and slow.
 */
void microbf(char * b)
{
    char *p;
    int c, i=0;
    unsigned short m=0;

    for(p=b;*p;p++)switch(*p) {
        case '>': m++;break;
        case '<': m--;break;
        case '+': t[m]++;break;
        case '-': t[m]--;break;
        case '.': putch(t[m]);break;
        case ',': if((c=getchar())!=EOF)t[m]=c;break;
        case '[': if(t[m]==0)while((i+=(*p=='[')-(*p==']'))&&p[1])p++;break;
        case ']': if(t[m]!=0)while((i+=(*p==']')-(*p=='['))&&p>b)p--;break;
    }
}

/*****************************************************************************
 * A special interpreter for pretty printing text generator BF programs.
 */
void bfprinter(char * b)
{
    char *p=b, *s=b, *ss=b;
    int c, i=0;
    unsigned short m=0;

    for(;*p;p++)switch(*p) {
        case '>': m++;break;
        case '<': m--;break;
        case '+': t[m]++;break;
        case '-': t[m]--;break;
        case ',': if((c=getchar())!=EOF)t[m]=c;break;
        case '[': if(t[m]==0)while((i+=(*p=='[')-(*p==']'))&&p[1])p++;break;
        case ']':
	    if (ss<=p) {
		char * p2;
		ss=p+1;
		for(p2 = ss; *p2 && *p2 != '.'; p2++);
		/* If we've got to the end of the code, stop running it. */
		if (*p2 == 0) {p = p2; break;}
	    }

	    if(t[m]!=0)while((i+=(*p==']')-(*p=='['))&&p>b)p--;
	    break;

        case '.':
	{
	    int mc = maxcol;
	    if (ss<=p) ss=p+1;
	    maxcol = 47;
	    while(s<ss && *s) {
		if (strchr("><+-.,[]", *s))
		    pc(*s);
		s++;
	    }

	    maxcol = 0;
	    while(col<48) pc(' ');
	    maxcol = mc;
	    col = 0;

	    c = t[m];
	    if (c > ' ' && c <= '~'
		    && c != '-' && c != '+' && c != '<' && c != '>'
		    && c != '[' && c != ']' && c != ',' && c != '.') {
		printf("%c\n", c);
	    } else if (c == '\n') { printf(" NL\n");
	    } else if (c == '\r') { printf(" CR\n");
	    } else if (c == ' ') { printf(" SP\n");
	    } else if (c == '.') { printf("dot\n");
	    } else if (c == ',') { printf("comma\n");
	    } else if (c == '-') { printf("dash\n");
	    } else if (c == '+') { printf("plus\n");
	    } else if (c == '>') { printf("gt\n");
	    } else if (c == '<') { printf("lt\n");
	    } else {
		printf("\\x%02X\n", c&0xFF);
	    }
	}
    }

    while(*s) {
	if (strchr("><+-.,[]", *s))
	    pc(*s);
	s++;
    }
    if (col) pc('\n');
}

/*****************************************************************************
 * Full tracing BF interpreter.
 */
void bf_trace(char * b)
{
    unsigned short m=0;
    int ln=0, icol = 0;
    char *p;

    for (p=b;*p;p++)
    {
	if (ln == 0)
	{
	    char *s = b;
	    ln = 1;
	    icol = 1;
	    for (s = b; s < p; s++)
	    {
		if (*s == '\n')
		{
		    ln++;
		    icol = 0;
		}
		else
		    icol++;
	    }
	}
	if (*p == '>' || *p == '<' || *p == '+' || *p == '-' ||
	    *p == '.' || *p == ',' || *p == '[' || *p == ']')
	{

	    printf ("%2d,%3d: %c ptr=%d *t=%d: ", ln, icol, *p, m, t[m]);

	    if (*p == '.')
	    {
		if (t[m] > ' ' && t[m] <= '~')
		    printf ("prt('%c')", t[m]);
		else
		    printf ("prt(%d)", t[m]);
	    }
	}
	icol++;
	switch (*p)
	{
	case '>':
	    m++;
	    break;
	case '<':
	    m--;
	    break;
	case '+':
	    t[m]++;
	    break;
	case '-':
	    t[m]--;
	    break;
	case ',':
	{
	    int c = getchar ();
	    if (c != EOF)
		t[m] = c;
	    break;
	}
	case '[':
	    if (t[m] == 0) {
		char *s = p;
		int i = 0;
		while ((i += (*s == '[') - (*s == ']')) && s[1])
		    s++;
		if (s[1]) p = s; else printf("ERR:");
	    }
	    ln = 0;
	    break;
	case ']':
	    if (t[m] != 0) {
		char * s = p;
		int i = 0;
		while ((i += (*s == ']') - (*s == '[')) && s >= b)
		    s--;
		if (s >= b) p = s; else printf("ERR:");
	    }
	    ln = 0;
	    break;
	case '\n':
	    ln++;
	    icol = 1;
	    break;
	}

	if (*p == '>' || *p == '<' || *p == '+' || *p == '-' ||
	    *p == '.' || *p == ',' || *p == '[' || *p == ']')
	{

	    if (*p == '>' || *p == '<')
		printf ("ptr=%d ", m);
	    if (*p == '[' || *p == ']')
		printf ("to=%c", *p);
	    else if (*p != '.')
		printf ("*t=%d", t[m]);
	    printf ("\n");
	}
    }
}
