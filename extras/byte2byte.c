/* This converts bytes into brainfuck code that will regenerate those bytes.
 * It uses two byte wrapped cells.
 *
 * Finding the shortest sequence with one cell is trivial.
 *
 * With two cells there are 65536 states the cells can be in, this program
 * constructs one of the cells to always have a zero byte and so reduces
 * this to 256 states (plus their mirror states with the cells swapped)
 * and generates an optimal solution for this system.
 *
 * Note: If all 65536 states were used a few sequences could be shorter,
 * for example:
 *  ++++[++++>+<]>++.+[+<---->]<++.
 * and
 *  ++++[++++>+<]>++.>++++++++++.
 *
 * But that would also give the problem that there is more than one
 * possible solution for printing each byte so it may be better to accept
 * a sub-optimal solution for this byte to get the best solution for
 * the entire string. This program has only one best solution for each
 * A->B conversion of cell values as there is only one cell value that
 * is acceptable for printing a byte and the mirror images are equlivent.
 *
 * With three or more cells shorter sequences are possible, simple algorithms
 * average to about 4.5 BF per English character, this averages to about
 * 9.5 BF per character.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/* Ancient systems don't have strdup */
#if defined(__GNUC__) \
    && (__GNUC__ > 2) || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7)
#if (_XOPEN_VERSION+0) < 500 && _POSIX_VERSION < 200809L
#if !defined(strdup) && !defined(_WIN32)
#define strdup(str) \
    ({char*_s=(str);int _l=strlen(_s)+1;void*_t=malloc(_l);if(_t)memcpy(_t,_s,_l);_t;})
#endif
#endif
#endif

int verbose = 0;
int cell_wrap = 0;
int cell_unsigned = 1;
int flippy = 1;		/* Both cells used for printing */
int disable_mult = 0;
int testmode = 0;
int dump_table = 0;
int cell_init = 0;
int cell_clear = 0;

struct s_bftab {
    int len;
    char * code;
} bytebftab[256][256], bytebftab2[256][256];

int flip = 0;
int col = 0;
unsigned char curr = 0;

void Usage(void);
void initbytebftab(void);
void write_tail(void);
void write_byte(int ch);
void dump_tables(void);

int
main(int argc, char ** argv)
{
    int ar, opton=1;
    char *p;
    const char ** filelist = 0;
    int filecount = 0;

    filelist = calloc(argc, sizeof*filelist);

    /* Traditional option processing. */
    for(ar = 1; ar < argc; ar++)
	if(opton && argv[ar][0] == '-' && argv[ar][1] != 0)
	    for(p = argv[ar]+1; *p; p++)
		switch(*p)
		{
		case '-': opton = 0; break;
		case 'v': verbose = 1; break;
		case 'b': cell_wrap = 1; break;
		case 'u': cell_unsigned = 1; break;
		case 's': cell_unsigned = 0; break;
		case 'i': cell_init = 1; break;
		case 'c': cell_clear = 1; break;
		case 'f': flippy = 0; break;
		case 't': testmode = 1; break;
		case 'd': dump_table = 1; break;
		case 'm': disable_mult = 1; testmode = 1; break;
		default:
		    Usage();
		}
	else
	    filelist[filecount++] = argv[ar];

    if (dump_table) dump_tables();

    initbytebftab();

    if (cell_init) {
	flip = (flippy && !disable_mult);
	col += printf("%s", flip?"[-]>[-]":">[-]<[-]");
    }

    if (filecount>0) {
	for(ar=0; ar<filecount; ar++) {
	    const char * s;
	    int ch;
	    if (ar+1<filecount) ch = ' '; else ch = '\n';

	    s = filelist[ar];
	    for(;;) {
		int c = *s?*s:ch;
		write_byte(c);
		if (!*s) break;
		s++;
	    }
	}
    }

    if (filecount == 0)
    {
	int ch;
	while((ch = getchar()) != EOF) {
	    write_byte(ch);
	}
    }

    write_tail();

    return 0;
}

void
Usage()
{
    fprintf(stderr, "byte2byte {opts} text\n");
    fprintf(stderr, "  -b\tAssume wrapping bytes\n");
    fprintf(stderr, "  -u\tAssume '.' needs unsigned values (default)\n");
    fprintf(stderr, "  -s\tAssume '.' needs signed values\n");
    fprintf(stderr, "  -i\tInit cells before code\n");
    fprintf(stderr, "  -c\tClear current cell after\n");
    fprintf(stderr, "  -d\tDump table and exit\n");
    fprintf(stderr, "  -f\tOnly print from first cell.\n");
    fprintf(stderr, "  -m\tDisable inline multiply\n");
    fprintf(stderr, "  -t\tEnable testmode\n");
    exit(1);
}

void
write_char(int ch)
{
    putchar(ch);
    if (ch == '\n')
	col = 0;
    else if ((col=((col+1)%72)) == 0)
	putchar('\n');
}

void
write_tail()
{
    if (cell_clear && curr) {
	const char * s = "[-]";
	while(*s) write_char(*s++);
    }
    if (flip) {
	flip = 0;
	write_char('<');
    }
    if(col) write_char('\n');
}

void
write_byte(int ch)
{
    unsigned char nextc = ch;
    char *s = bytebftab[curr][nextc].code;
    curr = nextc;
    while(*s) {
	if (*s == '>' || *s == '<') {
	    if (flip) write_char('<');
	    else write_char('>');
	    flip = !flip;
	} else
	    write_char(*s);
	s++;
    }
    if (ch) write_char('.');
}

static inline int
test_merge(
    struct s_bftab * from,
    struct s_bftab * via,
    struct s_bftab * res)
{
    int nlen, dflip = 0;
    char bfbuf[260];
    int ocnt = 0, ncnt = 0;

    nlen = from->len + via->len;

    if (from->len > 1 && via->len > 1)
	if ((from->code[from->len-1] == '>' || from->code[from->len-1] == '<') &&
		(via->code[0] == '>' || via->code[0] == '<')) {
	    nlen -= 2;
	    dflip = 1;
	}

    if (res->len == nlen && cell_wrap) {
	char * s;
	for(s=res->code; *s; s++) if (*s=='[') ocnt++;
	for(s=via->code; *s; s++) if (*s=='[') ncnt++;
	for(s=from->code; *s; s++) if (*s=='[') ncnt++;
    }

    if (res->len > nlen || (res->len == nlen && ocnt > ncnt)) {
	strcpy(bfbuf, from->code);
	if (dflip) {
	    bfbuf[from->len-1] = 0;
	    strcat(bfbuf, via->code+1);
	} else
	    strcat(bfbuf, via->code);
	free(res->code);
	res->code = strdup(bfbuf);
	res->len = nlen;
	return 1;
    }
    return 0;
}

void
mergecodes()
{
    int from,via,to;
    int gotone;

    do
    {
	gotone = 0;
	for(via=0; via<256; via++) {
	    for(from=0; from<256; from++) {
	        if (from == via) continue;

		for(to=0; to<256; to++) {
		    if (from == to) continue;
		    if (via == to) continue;

		    gotone |= test_merge(
			&bytebftab[from][via],
			&bytebftab[via][to],
			&bytebftab[from][to]);
		}
	    }
	}

	if (!flippy) {
	    for(from=0; from<256; from++) {
		for(via=0; via<256; via++) {
		    for(to=0; to<256; to++) {
			if (from == to) continue;

			if (bytebftab2[from][via].code) {
			    gotone |= test_merge(
				&bytebftab2[from][via],
				&bytebftab[via][to],
				&bytebftab2[from][to]);
			}

			if (bytebftab2[via][to].code) {
			    gotone |= test_merge(
				&bytebftab[from][via],
				&bytebftab2[via][to],
				&bytebftab2[from][to]);

			    if (bytebftab2[from][via].code)
				gotone |= test_merge(
				    &bytebftab2[from][via],
				    &bytebftab2[via][to],
				    &bytebftab[from][to]);

			}
		    }
		}
	    }
	}
    }
    while(gotone);
}

void
initbytebftab()
{
    int from,to,i,j,k;
    int ini, dec, inc;
    int oflg;
    int R=0, E=0;
    char bfbuf[260];

    if (verbose) 
	fprintf(stderr, "Building %s table ... ",
	    cell_wrap?"wrapping":cell_unsigned?"unsigned":"signed");

    /* First the dumb method */
    for(from=0; from<256; from++) {
	for(to=0; to<256; to++)
	{
	    int f;
	    bytebftab2[from][to].len = 30000;
	    bytebftab2[from][to].code = 0;

	    if (cell_wrap) {
		j = ((to-from) & 0xFF);
		k = ((from-to) & 0xFF);
		f = (j<=k);
	    } else if (cell_unsigned) {
		j = (to-from);
		k = (from-to);
		f = (j>=0);
	    } else {
		j = ((signed char)to-(signed char)from);
		k = ((signed char)from-(signed char)to);
		f = (j>=0);
	    }

	    if (f) {
		for(i=0; i<j; i++) bfbuf[i] = '+';
		bfbuf[j] = 0;
		bytebftab[from][to].len = j;
		bytebftab[from][to].code = strdup(bfbuf);
	    } else {
		for(i=0; i<k; i++) bfbuf[i] = '-';
		bfbuf[k] = 0;
		bytebftab[from][to].len = k;
		bytebftab[from][to].code = strdup(bfbuf);
	    }
	}
    }

    for(from=0; from<256; from++) {
	/* To zero is dumb too */
	to = 0;
	if (bytebftab[from][to].len > 3) {
	    free(bytebftab[from][to].code);
	    bytebftab[from][to].len = 3;
	    if (cell_unsigned || cell_wrap)
		bytebftab[from][to].code = strdup("[-]");
	    else {
		if (from>127)
		    bytebftab[from][to].code = strdup("[+]");
		else
		    bytebftab[from][to].code = strdup("[-]");
	    }
	}
    }

    /*	This call is rather expensive and only merges via-zero anyway
	mergecodes();
     */

    if (testmode) E=32;
    else if (cell_wrap) E=11;
    else if (cell_unsigned) E=14;
    else E=14;

    if(E>0) {
	if (verbose) fprintf(stderr, "additive ... ");
	/* Some additive strings ">++++[<++++>-]<" */
	for(ini= E; ini> -E; ini--) {
	  for(oflg = 0; oflg<4; oflg++) {
	    for(dec = -E; dec < E; dec++) {
		/* Prefer +/- 1 on desc loop */
		if (oflg == 0) {
		    if (dec != -1) continue;	/* += ini * inc */
		} else if (oflg == 1) {
		    if (dec != 1) continue;
		} else if (oflg > 2)
		    break;

		for(inc = E; inc > -E; inc--) {
		    int cnt = 0;
		    int sum = ini;
		    from = to = 0;
		    while(sum && cnt < 256 && to > -256 && to <= 256) {
			cnt ++;
			if (cell_wrap) {
			    to = ((to + inc) & 0xFF);
			    sum = ((sum + dec) & 0xFF);
			} else {
			    to = to + inc;
			    sum = sum + dec;
			}
		    }
		    if (sum == 0 && cnt < 256 && to > -256 && to <= 256) {
			int dif = to;
			for(from = 0; from<256; from++) {
			    int len = abs(ini) + abs(inc) + abs(dec) + 6;
			    char * p = bfbuf;

			    if (cell_wrap) {
				to = ((from + dif) & 0xFF);
			    } else if (cell_unsigned) {
				to = from + dif;
			    } else {
				to = (signed char)from + dif;
				if (to >= -128 && to <= 127) to &= 0xFF; else to = -1;
			    }

			    if (to >= 0 && to <= 256 && bytebftab[from][to].len > len) {
				*p++ = '>';
				i = ini;
				while(i>0) { i--; *p++ = '+'; }
				while(i<0) { i++; *p++ = '-'; }
				*p++ = '[';
				*p++ = '<';
				i = inc;
				while(i>0) { i--; *p++ = '+'; }
				while(i<0) { i++; *p++ = '-'; }
				*p++ = '>';
				i = dec;
				while(i>0) { i--; *p++ = '+'; }
				while(i<0) { i++; *p++ = '-'; }
				*p++ = ']';
				*p++ = '<';
				*p++ = 0;
				bytebftab[from][to].len = len;
				bytebftab[from][to].code = strdup(bfbuf);

			    }
			}
		    }
		}
	    }
	  }
	}
    }

    mergecodes(); /* Prefer simple stuff. */

    if (testmode) R=32;
    else if (cell_wrap) R=11;
    else if (cell_unsigned) R=17;
    else R=15;

    if (R>0 && !disable_mult) {
	if (verbose) fprintf(stderr, "multiplicative ... ");

	/* Add the flippy nul codes */
	if (!flippy) {
	    for(from=0; from<256; from++) {
		/* To zero with a flip. */
		to = 0;
		if (from == 0) {
		    bytebftab2[from][to].len = 1;
		    bytebftab2[from][to].code = strdup(">");
		} else if (cell_unsigned || cell_wrap || from < 128) {
		    bytebftab2[from][to].len = 4;
		    bytebftab2[from][to].code = strdup("[-]>");
		}

		/* Move a value to the other cell */
		if (from != 0) {
		    if (cell_unsigned || cell_wrap || from < 128) {
			bytebftab2[from][from].len = 7;
			bytebftab2[from][from].code = strdup("[->+<]>");
		    } else {
			bytebftab2[from][from].len = 7;
			bytebftab2[from][from].code = strdup("[+>-<]>");
		    }
		}
	    }
	}

	/* Then a multiplication set "[-->+++++<]>" */
	for(from=0; from<256; from++) {
	  for(oflg = 0; oflg<4; oflg++) {
	    for(dec = -R; dec < R; dec++) {
		/* Prefer +/- 1 on desc loop */
		if (oflg == 0) {
		    if (dec != -1) continue;	/* Times inc */
		} else if (oflg == 1) {
		    if (dec > 0) continue;		/* Div -dec times inc */
		} else if (oflg != 2) {
		    break;
		}

		for(inc = R; inc > -R; inc--) {
		    int cnt = 0;
		    int sum = from;
		    to = 0;

		    if (!cell_wrap && !cell_unsigned)
			sum = (signed char)sum;

		    while(sum && cnt < 256 && to > -256 && to < 256) {
			cnt ++;
			if (cell_wrap) {
			    to = ((to + inc) & 0xFF);
			    sum = ((sum + dec) & 0xFF);
			} else {
			    to = to + inc;
			    sum = sum + dec;
			}
		    }

		    if (!cell_wrap && !cell_unsigned) {
			if (to >= -128 && to <= 127) to &= 0xFF; else to = -1;
		    }

		    if (cnt < 256 && to >= 0 && to < 256) {
			int len = abs(inc) + abs(dec) + 5;
			char * p = bfbuf;
			int f = 0;
			if (flippy)
			    f = (bytebftab[from][to].len > len);
			else
			    f = (bytebftab2[from][to].len > len);

			if (f) {
			    *p++ = '[';
			    i = dec;
			    while(i>0) { i--; *p++ = '+'; }
			    while(i<0) { i++; *p++ = '-'; }
			    *p++ = '>';
			    i = inc;
			    while(i>0) { i--; *p++ = '+'; }
			    while(i<0) { i++; *p++ = '-'; }
			    *p++ = '<';
			    *p++ = ']';
			    *p++ = '>';
			    *p++ = 0;
			    if (flippy) {
				if (bytebftab[from][to].code)
				    free(bytebftab[from][to].code);
				bytebftab[from][to].len = len;
				bytebftab[from][to].code = strdup(bfbuf);
			    } else {
				if (from == to) {
				    fprintf(stderr, "%d: %d %s -> %d %s\n",
					from,
					bytebftab2[from][to].len,
					bytebftab2[from][to].code,
					len,
					bfbuf);
				}
				if (bytebftab2[from][to].code)
				    free(bytebftab2[from][to].code);
				bytebftab2[from][to].len = len;
				bytebftab2[from][to].code = strdup(bfbuf);
			    }
			}
		    }
		}
	    }
	  }
	}
    }

    mergecodes();

    if (verbose) fprintf(stderr, "Done.\n");
}

void
clearbytebftab()
{
    int from,to;

    for(from=0; from<256; from++) {
	for(to=0; to<256; to++)
	{
	    if (bytebftab[from][to].code)
		free(bytebftab[from][to].code);
	    bytebftab[from][to].code = 0;
	    bytebftab[from][to].len = 0;

	    if (bytebftab2[from][to].code)
		free(bytebftab2[from][to].code);
	    bytebftab2[from][to].code = 0;
	    bytebftab2[from][to].len = 0;
	}
    }
}

void do_dump()
{
    int from,to, mtype = 's';
    if (cell_unsigned) mtype = 'u';
    if (cell_wrap) mtype = 'b';

    printf("const char * bftable_%c[256][256] =\n", mtype);
    printf("{\n");
    for(from=0; from<256; from++) {
	printf("{\n");
	for(to=0; to<256; to++) {
	    char *s = bytebftab[from][to].code;
	    int f = 0;

	    printf("/* bf_%c[%3d][%3d] (%2d) */ \"",
		    mtype, from, to,
		    bytebftab[from][to].len);

	    while(*s) {
		if (*s == '>' || *s == '<') {
		    if (f) putchar('<');
		    else putchar('>');
		    f = !f;
		} else
		    putchar(*s);
		s++;
	    }

	    printf("\"%s\n", to==255?"":",");
	}
	printf("}%s\n", from==255?"":",");
    }
    printf("};\n");
}

void dump_tables()
{
    int from, to;
    struct s_bftab b_bftab[256][256];

    cell_wrap = cell_unsigned = 0;
    initbytebftab();
    do_dump();

    /* Save the signed strings */
    for(from=0; from<256; from++)
	for(to=0; to<256; to++)
	{
	    b_bftab[from][to].code = bytebftab[from][to].code;
	    b_bftab[from][to].len = bytebftab[from][to].len;
	    bytebftab[from][to].code = 0;
	    bytebftab[from][to].len = 0;
	}

    clearbytebftab();

    cell_unsigned = 1;
    initbytebftab();
    do_dump();

    /* Save the unsigned strings if they're not worse than the signed */
    for(from=0; from<256; from++)
	for(to=0; to<256; to++)
	if (bytebftab[from][to].len <= b_bftab[from][to].len) {
	    free(b_bftab[from][to].code);

	    b_bftab[from][to].code = bytebftab[from][to].code;
	    b_bftab[from][to].len = bytebftab[from][to].len;
	    bytebftab[from][to].code = 0;
	    bytebftab[from][to].len = 0;
	}

    clearbytebftab();

    cell_unsigned = 0;
    cell_wrap = 1;
    initbytebftab();

    /* Prefer to minimise wrapping */
    for(from=0; from<256; from++)
	for(to=0; to<256; to++)
	{
	    if (b_bftab[from][to].len <= bytebftab[from][to].len) {
		if (bytebftab[from][to].code)
		    free(bytebftab[from][to].code);
		bytebftab[from][to].code = b_bftab[from][to].code;
		bytebftab[from][to].len = b_bftab[from][to].len;
	    } else
		free(b_bftab[from][to].code);

		b_bftab[from][to].len = 0;
		b_bftab[from][to].code = 0;
	    }

    do_dump();

    clearbytebftab();

    exit(0);
}
