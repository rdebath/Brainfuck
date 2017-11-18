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

int cell_wrap = 0;
int cell_unsigned = 0;
int textmode = 0;
int dump_table = 0;

struct s_bftab {
    int len;
    char * code;
} bytebftab[256][256];

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
		case 'b': cell_wrap ^= 1; break;
		case 'u': cell_unsigned ^= 1; break;
		case 't': textmode ^= 1; break;
		case 'd': dump_table ^= 1; break;

		default:
		    Usage();
		}
	else
	    filelist[filecount++] = argv[ar];

    if (dump_table) dump_tables();

    if (filecount>0) {
	for(ar=0; ar<filecount; ar++) {
	    const char * s;
	    int ch;
	    if (ar+1<filecount) ch = ' '; else ch = '\n';

	    s = filelist[ar];
	    for(;;) {
		write_byte(*s?*s:ch);

		if (!*s) break;
		s++;
	    }
	}
	write_tail();
    }

    if (filecount == 0)
    {
	int ch;
	while((ch = getchar()) != EOF) {
	    write_byte(ch);
	}
	write_tail();
    }

    return 0;
}

void
Usage()
{
    fprintf(stderr, "byte2byte {opts} text\n");
    fprintf(stderr, "  -b\tAssume wrapping bytes\n");
    fprintf(stderr, "  -u\tAssume '.' needs unsigned\n");
    fprintf(stderr, "  -d\tDump table and exit\n");
    exit(1);
}

void
write_tail()
{
    if (flip) {
	flip = 0;
	putchar('<');
	if ((col=((col+1)%72)) == 0) putchar('\n');
    }
    if(col) putchar('\n');
}

void
write_byte(int ch)
{
    unsigned char nextc = ch;
    char *s = bytebftab[curr][nextc].code;
    curr = nextc;
    while(*s) {
	if (*s == '>' || *s == '<') {
	    if (flip) putchar('<');
	    else putchar('>');
	    flip = !flip;
	} else
	    putchar(*s);
	s++;
	if ((col=((col+1)%72)) == 0) putchar('\n');
    }
    putchar('.');
    if ((col=((col+1)%72)) == 0) putchar('\n');
}

void
mergecodes()
{
    int from,via,to;
    int gotone;
    char bfbuf[260];

    do
    {
	gotone = 0;
	for(from=0; from<256; from++) {
	    for(via=0; via<256; via++) {
	        if (from == via) continue;

		for(to=0; to<256; to++) {
		    if (from == to) continue;
		    if (via == to) continue;

		    if ((bytebftab[from][to].len >
			 bytebftab[from][via].len + bytebftab[via][to].len)
#if 0
			 || (bytebftab[from][to].len ==
			bytebftab[from][via].len + bytebftab[via][to].len &&
			bytebftab[from][via].code[0] < bytebftab[from][to].code[0]
			)
#endif
			) {

			strcpy(bfbuf, bytebftab[from][via].code);
			strcat(bfbuf, bytebftab[via][to].code);
			free(bytebftab[from][to].code);
			bytebftab[from][to].code = strdup(bfbuf);
			bytebftab[from][to].len =
			    bytebftab[from][via].len + bytebftab[via][to].len;
			gotone = 1;
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
    int R=0, E=0;
    char bfbuf[260];

    /* First the dumb method */
    for(from=0; from<256; from++) {
	for(to=0; to<256; to++)
	{
	    int f;

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
	    /* To zero is dumb too */
	    if (to == 0 && bytebftab[from][to].len > 3) {
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
    }

    mergecodes(); /* Prefer simple stuff. */

    if (cell_wrap) E=7;
    else if (cell_unsigned) E=8;
    else E=6;

    if(E>0) {
	/* Some additive strings ">++++[<++++>-]<" */
	for(ini= E; ini> -E; ini--) {
	    for(dec = -E; dec < E; dec++) {
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

    mergecodes(); /* Prefer simple stuff. */

    if (cell_wrap) R=9;
    else if (cell_unsigned) R=14;
    else R=12;

    if (R>0) {
	/* Then a multiplication set */
	for(from=0; from<256; from++) {
	    for(dec = R; dec > -R; dec--) {
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
			if (bytebftab[from][to].len > len) {
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
			    bytebftab[from][to].len = len;
			    bytebftab[from][to].code = strdup(bfbuf);
			}
		    }
		}
	    }
	}
    }

    mergecodes();
}

void do_dump()
{
    int from,to, mtype = 's';
    if (cell_unsigned) mtype = 'u';
    if (cell_wrap) mtype = 'b';

    printf("char * bftable_%c[256][256] =\n", mtype);
    printf("{\n");
    for(from=0; from<256; from++) {
	printf("{\n");
	for(to=0; to<256; to++) {
	    printf("/* bf_%c[%3d][%3d] */ \"%s\"%s\n", mtype,
		    from, to, bytebftab[from][to].code,
		    to==255?"":",");
	}
	printf("}%s\n", from==255?"":",");
    }
    printf("};\n");
}

void dump_tables()
{
    int from, to;
    struct s_bftab u_bftab[256][256];


    cell_wrap = cell_unsigned = 0;
    initbytebftab();
    do_dump();

    for(from=0; from<256; from++)
        for(to=0; to<256; to++)
	{
	    if (bytebftab[from][to].code)
		free(bytebftab[from][to].code);
	    bytebftab[from][to].code = 0;
	    bytebftab[from][to].len = 0;
	}

    cell_unsigned = 1;
    initbytebftab();
    do_dump();

    for(from=0; from<256; from++)
        for(to=0; to<256; to++)
	{
	    u_bftab[from][to].code = bytebftab[from][to].code;
	    u_bftab[from][to].len = bytebftab[from][to].len;
	    bytebftab[from][to].code = 0;
	    bytebftab[from][to].len = 0;
	}

    cell_unsigned = 0;
    cell_wrap = 1;
    initbytebftab();

    for(from=0; from<256; from++)
        for(to=0; to<256; to++)
	{
	    if (u_bftab[from][to].len <= bytebftab[from][to].len) {
		if (bytebftab[from][to].code)
		    free(bytebftab[from][to].code);
		bytebftab[from][to].code = u_bftab[from][to].code;
		bytebftab[from][to].len = u_bftab[from][to].len;
		u_bftab[from][to].len = 0;
		u_bftab[from][to].code = 0;
	    }
	}

    do_dump();

    exit(0);
}
