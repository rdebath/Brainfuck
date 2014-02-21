#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void gen_text(char * buf);
void gen_runner(char * buf);
void gen_ascii(char * buf);
void gen_multonly(char * buf);
void gen_nestloop(char * buf);
unsigned int isqrt(unsigned int x);

char linebuf[16384];        /* Only one buffer full for multi-gen.  */
int flg_init = 0;
int flg_clear = 0;
int flg_signed = 1;
int flg_nortz = 0;
int enable_multloop = 0;

int subrange = 0;
int got_hiascii = 0;
int verbose = 0;
int flg_zoned = 1;

void reinit_state(void);
void add_chr(int ch);
void add_str(char * p);

char * str_start = 0;
int str_max = 0, str_next = 0;
int str_cells_used = 0;

char * best_str = 0;
int best_len = -1;
int best_cells = 1;

int
main(int argc, char ** argv)
{
    FILE * ifd;
    for(;;) {
	if (argc < 2 || argv[1][0] != '-' || argv[1][1] == '\0') {
	    break;
	} else if (strcmp(argv[1], "--") == 0) {
	    argc--; argv++;
	    break;
	} else if (strcmp(argv[1], "-v") == 0) {
	    verbose++;
	    argc--; argv++;
	} else if (strcmp(argv[1], "-i") == 0) {
	    flg_init = !flg_init;
	    argc--; argv++;
	} else if (strcmp(argv[1], "-c") == 0) {
	    flg_clear = !flg_clear;
	    argc--; argv++;
	} else if (strcmp(argv[1], "-e") == 0) {
	    flg_signed = !flg_signed;
	    argc--; argv++;
	} else if (strcmp(argv[1], "-n") == 0) {
	    flg_nortz = !flg_nortz;
	    argc--; argv++;
	} else if (strcmp(argv[1], "-M") == 0) {
	    enable_multloop = !enable_multloop;
	    argc--; argv++;
	} else if (strncmp(argv[1], "-s", 2) == 0) {
	    subrange = atol(argv[1]+2);
	    argc--; argv++;
	} else {
	    fprintf(stderr, "Unknown option '%s'\n", argv[1]);
	    exit(1);
	}
    }
    if (argc < 2 || strcmp(argv[1], "-") == 0)
	ifd = stdin;
    else
	ifd = fopen(argv[1], "r");
    if (ifd == 0) perror(argv[1]);
    else {
	int c;
	char * p = linebuf;

	while((c = fgetc(ifd)) != EOF) {
	    if (c == '\r') continue;
	    if (c & 0x80) got_hiascii = 1;
	    *p++ = c;
	    if (c == 0 || p-linebuf >= sizeof(linebuf)-1) {
		*p++ = 0;
		if (subrange == 0) subrange = 10;

		/* Need to clear tape for next block */
		if (!flg_init) flg_clear += 2;
		gen_text(linebuf);
		if (!flg_init) flg_clear -= 2;
		p = linebuf;
	    }

	    if (c == 0) add_chr('.');
	}
	*p++ = 0;
	if (ifd != stdin) fclose(ifd);

	if (subrange == -1)
	    gen_runner(linebuf);
	else if (subrange != 0)
	    gen_text(linebuf);
	else {

	    subrange=10;
	    flg_zoned = 1;
	    {
		reinit_state();
		gen_text(linebuf);
		if (best_len == -1 || best_len > str_next || (best_len == str_next && str_cells_used < best_cells)) {
		    if (verbose)
			fprintf(stderr, "Found Subrange=%d zoned (->%d)\n", subrange, str_next);

		    if (best_str) free(best_str);
		    best_str = strdup(str_start);
		    best_len = str_next;
		    best_cells = str_cells_used;
		}
	    }

	    flg_zoned = 1;
	    for(subrange = 1; subrange<33; subrange++) if (subrange!=10) {
		reinit_state();
		gen_text(linebuf);
		if (best_len == -1 || best_len > str_next || (best_len == str_next && str_cells_used < best_cells)) {
		    if (verbose)
			fprintf(stderr, "Found Subrange=%d zoned (%d->%d)\n", subrange, best_len, str_next);

		    if (best_str) free(best_str);
		    best_str = strdup(str_start);
		    best_len = str_next;
		    best_cells = str_cells_used;
		}
	    }

	    flg_zoned = 0;
	    for(subrange = 1; subrange<33; subrange++) {
		reinit_state();
		gen_text(linebuf);
		if (best_len == -1 || best_len > str_next || (best_len == str_next && str_cells_used < best_cells)) {
		    if (verbose)
			fprintf(stderr, "Found Subrange=%d unzoned (%d->%d)\n", subrange, best_len, str_next);

		    if (best_str) free(best_str);
		    best_str = strdup(str_start);
		    best_len = str_next;
		    best_cells = str_cells_used;
		}
	    }

	    if (enable_multloop) {
		reinit_state();
		flg_zoned = 0;
		gen_multonly(linebuf);
	    }

	    if(0) {
		reinit_state();
		flg_zoned = 0;
		gen_nestloop(linebuf);
	    }

	    reinit_state();
	    gen_runner(linebuf);
	    if (best_len == -1 || best_len > str_next || (best_len == str_next && str_cells_used < best_cells)) {
		if (best_str) free(best_str);
		best_str = strdup(str_start);
		best_len = str_next;
		best_cells = str_cells_used;

		if (verbose)
		    fprintf(stderr, "Method 'runner' is better\n");
	    }

	    if (verbose)
		fprintf(stderr, "BF Size = %d, %.2f bf/char, cells = %d\n",
		    best_len, best_len * 1.0/ strlen(linebuf), best_cells);

	    printf("%s\n", best_str);
	    free(str_start); str_start = 0; str_max = str_next = 0;
	}
    }

    if (str_start)
	printf("%s\n", str_start);
    return 0;
}

/*
    This attempts to make an optimal BF code fragment to print a specific
    ASCII string. The technique used is to divide the ranges of characters
    to be printed into short subranges and create a simple multiply
    loop to init each cell into that range. The cells are sorted into
    frequency order.

    The first problem is the initial loop is quite a large overhead,
    for short strings other methods are likely to be smaller.

    The second problem is that the relation between the best selection
    of cell ranges and the string to print is very very complex. For
    long strings it averages out, for short ones, not so much.

    In addition this can clear all the cells it uses before use, this
    usually allows an optimiser to convert this code back to a printf()
    but it takes a little space. It can also clear the cells after use,
    this isn't so useful.

    Non-ASCII characters are not optimised and are printed using the
    negative aliases of the bytes: ie: chars are signed. However, if
    it's set negative a cell will be zeroed at the end of the fragment
    so [-] is safe even with bignum cells.

    The reason negatives are used is to be compatible with interpreters
    that use -1 as the EOF indicator. NB: This byte is illegal for UTF-8.

 */

static int cells[256];
static int cells_dirty = 0;
static int cells_used = 0;

void
add_chr(int ch)
{
    int lastch = 0;
    while (str_next+2 > str_max) {
	str_start = realloc(str_start, str_max += 1024);
	if(!str_start) { perror("realloc"); exit(1); }
    }
    if (str_next > 1) lastch = str_start[str_next-1];

    if ( (ch == '>' && lastch == '<') || (ch == '<' && lastch == '>') ) {
	str_start[--str_next] = 0;
	return;
    }

    str_start[str_next++] = ch;
    str_start[str_next] = 0;
}

void
add_str(char * p)
{
    while(p && *p) add_chr(*p++);
}

void
reinit_state(void)
{
    memset(cells, 0, sizeof(cells));
    cells_dirty = 0;
    cells_used = 0;
    str_next = 0;
    if (str_start) str_start[str_next] = 0;
}

void
gen_text(char * buf)
{
    int counts[256] = {};
    int txn[256];
    int rng[256];
    int currcell=0, usecell=0;
    int maxcell = 0;
    char * p;
    int i, j;

    /* Count up a frequency table */
    for(p=buf; *p;) {
	int c = *p++;
	if (!flg_signed) c &= 0xFF;
	if (c >= 0)
	    usecell = (c+subrange/2)/subrange;
	else
	    usecell = 0;

	if (usecell > maxcell) maxcell = usecell;
	counts[usecell] ++;
    }

    for(i=0; i<=maxcell; i++) rng[i] = txn[i] = i;

    /* Dumb bubble sort for simplicity */
    for(i=1; i<=maxcell; i++)
	for(j=i+1; j<=maxcell; j++)
	{
	    if(counts[i] < counts[j]) {
		int t;
		t = counts[i];
		counts[i] = counts[j];
		counts[j] = t;
		t = rng[i];
		rng[i] = rng[j];
		rng[j] = t;
	    }
	}

    /* Set the translation table */
    for(i=0; i<=maxcell; i++) txn[rng[i]] = i;

/* for(i=0; i<=maxcell; i++) fprintf(stderr, "%3d: %3d %3d %4d\n", i, rng[i], txn[i], counts[i]); //-*/

    if (flg_init) {
	/* Clear the working cells */
	for(i=maxcell; i>=0; i--) {
	    if (i >= cells_used)
		if (counts[i] || i == 0) {
		    while(currcell < i) { add_chr('>'); currcell++; }
		    while(currcell > i) { add_chr('<'); currcell--; }
		    add_str("[-]");
		    cells[i] = 0;
		}
	}
	cells_dirty = 0;
	if (maxcell>=cells_used) cells_used = maxcell+1;
    }

    str_cells_used = 0;
    if (maxcell>1)
	for(i=maxcell; i>=0; i--)
	    if ((counts[i] || i == 0) && i>str_cells_used)
		str_cells_used = i;
    str_cells_used++;

    /* Generate the init strings */
    if (maxcell > 1 && !cells_dirty) {
	if (subrange>15) {
	    int l = isqrt(subrange);
	    int r = subrange/l;
	    add_chr('>');
	    for(i=0; i<l; i++) add_chr('+');
	    add_str("[<");
	    for(i=0; i<r; i++) add_chr('+');
	    add_str(">-]<");
	    for(i=l*r; i<subrange; i++) add_chr('+');
	} else {
	    for(i=0; i<subrange; i++)
		add_chr('+');
	}
	add_chr('[');
	for(i=1; i<=maxcell; i++)
	{
	    if (counts[i] || i == 0) {
		while(currcell < i) { add_chr('>'); currcell++; }
		cells[currcell] += subrange * rng[i];
		for(j=0; j<rng[i]; j++)
		    add_chr('+');
	    }
	}
	while(currcell > 0) { add_chr('<'); currcell--; }
	add_chr('-');
	add_chr(']');
    }

    cells_dirty = 1;

    if (flg_zoned) {
	/* Print each character */
	for(p=buf; *p;) {
	    int c = *p++;
	    if (!flg_signed) c &= 0xFF;
	    if (c >= 0)
		usecell = (c+subrange/2)/subrange;
	    else
		usecell = 0;

	    usecell = txn[usecell];

	    while(currcell > usecell) { add_chr('<'); currcell--; }
	    while(currcell < usecell) { add_chr('>'); currcell++; }

	    while(cells[currcell] < c) { add_chr('+'); cells[currcell]++; }
	    while(cells[currcell] > c) { add_chr('-'); cells[currcell]--; }

	    add_chr('.');
	}
    } else {
	/* Print each character, use closest cell. */
	for(p=buf; *p;) {
	    int minrange = 999999;
	    int c = *p++;
	    if (!flg_signed) c &= 0xFF;
	    usecell = 0;
	    for(i=0; i<str_cells_used; i++) {
		int range = abs(currcell-i) + abs(cells[i] - c);
		if (range < minrange) {
		    usecell = i;
		    minrange = range;
		}
	    }

	    while(currcell > usecell) { add_chr('<'); currcell--; }
	    while(currcell < usecell) { add_chr('>'); currcell++; }

	    while(cells[currcell] < c) { add_chr('+'); cells[currcell]++; }
	    while(cells[currcell] > c) { add_chr('-'); cells[currcell]--; }

	    add_chr('.');
	}
    }

    if (flg_init) {
	/* Make sure the zero cell is positive */
	while(currcell > 0) { add_chr('<'); currcell--; }
	if (cells[currcell] < 0) add_str("[+]");
    }

    if (flg_clear) {
	/* Clear the working cells */
	for(i=maxcell; i>=0; i--) {
	    if (cells[i] != 0) {
		while(currcell < i) { add_chr('>'); currcell++; }
		while(currcell > i) { add_chr('<'); currcell--; }
		if (cells[i] > 0)
		    add_str("[-]");
		else
		    add_str("[+]");
		cells[i] = 0;
	    }
	}
	while(currcell > 0) { add_chr('<'); currcell--; }
	cells_dirty = 0;
    }

    /* Put the pointer back to the zero cell */
    if (!flg_nortz) while(currcell > 0) { add_chr('<'); currcell--; }
}

void
gen_runner(char * buf)
{
    const int maxcell = 5;
    int currcell=0, usecell=0;
    int i;
    char * p;

    if (flg_init) {
	/* Clear the working cells */
	for(i=maxcell; i>=0; i--) {
	    while(currcell < i) { add_chr('>'); currcell++; }
	    while(currcell > i) { add_chr('<'); currcell--; }
	    add_str("[-]");
	    cells[i] = 0;
	}
	cells_dirty = 0;
    }

    str_cells_used = maxcell+1;

    if (!cells_dirty) {
	add_str(">+>+>++>+++<[>[->++<<+++>]<<]");
	cells[0] = 0;
	cells[1] = 103;
	cells[2] = 0;
	cells[3] = 68;
	cells[4] = 22;
	cells[5] = 6;
    }

    cells_dirty = 1;

    /* Print each character, use closest cell. */
    for(p=buf; *p;) {
	int minrange = 999999;
	int c = *p++;
	if (!flg_signed) c &= 0xFF;
	usecell = 0;
	for(i=0; i<=maxcell; i++) {
	    int range = abs(currcell-i) + abs(cells[i] - c);
	    if (range < minrange) {
		usecell = i;
		minrange = range;
	    }
	}

	while(currcell > usecell) { add_chr('<'); currcell--; }
	while(currcell < usecell) { add_chr('>'); currcell++; }

	while(cells[currcell] < c) { add_chr('+'); cells[currcell]++; }
	while(cells[currcell] > c) { add_chr('-'); cells[currcell]--; }

	add_chr('.');
    }

    if (flg_clear || flg_init) {
	/* Clear the working cells */
	for(i=maxcell; i>=0; i--) {
	    if (cells[i] != 0) {
		while(currcell < i) { add_chr('>'); currcell++; }
		while(currcell > i) { add_chr('<'); currcell--; }
		if (cells[i] < 0)
		    add_str("[+]");
		else if (cells[i] > 0 && flg_clear)
		    add_str("[-]");
		cells[i] = 0;
	    }
	}
	while(currcell > 0) { add_chr('<'); currcell--; }
	cells_dirty = 0;
    }

    /* Put the pointer back to the zero cell */
    if (!flg_nortz) while(currcell > 0) { add_chr('<'); currcell--; }
}

/*
 * This integer square root routine tends to be a bit quick.
 * Please note it implicitly assumes that ints are 32 bits long.
 */
unsigned int isqrt(unsigned int x)
{
    register unsigned int op, res, one;

    op = x;
    res = 0;

    /* "one" starts at the highest power of four <= than the argument. */
    one = 1 << 30;  /* second-to-top bit set */
    while (one > op) one >>= 2;

    while (one != 0) {
	if (op >= res + one) {
	    op -= res + one;
	    res += one << 1;
	}
	res >>= 1;
	one >>= 2;
    }
    return res;
}

/*
 * This searches through all the simple multiplier loops upto a certain size.
 * The text is then generated using a closest next function.
 * Returns the shortest.
 */
void
gen_multonly(char * buf)
{
    int cellincs[10] = {0};
    int maxcell = 0;
    int currcell=0, usecell=0;
    int i;
    char * p;

    for(;;)
    {
	{
	    for(i=0; ; i++) {
		if (i == 7) return;
		cellincs[i]++;
		if (cellincs[i] < 12) break;
		cellincs[i] = 1;
	    }

	    for(maxcell=0; cellincs[maxcell] != 0; )
		maxcell++;

	    maxcell--; currcell=0, usecell=0;
	}

	reinit_state();
	if (flg_init) {
	    /* Clear the working cells */
	    for(i=maxcell; i>=0; i--) {
		while(currcell < i) { add_chr('>'); currcell++; }
		while(currcell > i) { add_chr('<'); currcell--; }
		add_str("[-]");
		cells[i] = 0;
	    }
	    cells_dirty = 0;
	}

	str_cells_used = maxcell+1;

	if (!cells_dirty) {
	    int j;

	    for(j=0; j<cellincs[0]; j++)
		add_chr('+');
	    add_chr('[');

	    cells[0] = 0;
	    for(i=1; i<=maxcell; i++) {
		cells[i] = cellincs[i] * cellincs[0];
		add_chr('>');
		for(j=0; j<cellincs[i]; j++)
		    add_chr('+');
	    }

	    for(i=1; i<=maxcell; i++)
		add_chr('<');
	    add_chr('-');
	    add_chr(']');
	}

	cells_dirty = 1;

	/* Print each character */
	for(p=buf; *p;) {
	    int minrange = 255;
	    int c = *p++;
	    if (!flg_signed) c &= 0xFF;

	    usecell = 0;
	    for(i=0; i<=maxcell; i++) {
		int range = abs(currcell-i) + abs(cells[i] - c);
		if (range < minrange) {
		    usecell = i;
		    minrange = range;
		}
	    }

	    while(currcell > usecell) { add_chr('<'); currcell--; }
	    while(currcell < usecell) { add_chr('>'); currcell++; }

	    while(cells[currcell] < c) { add_chr('+'); cells[currcell]++; }
	    while(cells[currcell] > c) { add_chr('-'); cells[currcell]--; }

	    add_chr('.');
	}

	if (flg_clear || flg_init) {
	    /* Clear the working cells */
	    for(i=maxcell; i>=0; i--) {
		if (cells[i] != 0) {
		    while(currcell < i) { add_chr('>'); currcell++; }
		    while(currcell > i) { add_chr('<'); currcell--; }
		    if (cells[i] < 0)
			add_str("[+]");
		    else if (cells[i] > 0 && flg_clear)
			add_str("[-]");
		    cells[i] = 0;
		}
	    }
	    while(currcell > 0) { add_chr('<'); currcell--; }
	    cells_dirty = 0;
	}

	/* Put the pointer back to the zero cell */
	if (!flg_nortz) while(currcell > 0) { add_chr('<'); currcell--; }

/******************************************************************************/
	if (best_len == -1 || best_len > str_next || (best_len == str_next && str_cells_used < best_cells)) {
	    if (best_str) free(best_str);
	    best_str = strdup(str_start);
	    best_len = str_next;
	    best_cells = str_cells_used;

	    if (verbose)
		fprintf(stderr, "Found multloop: %d * [%d %d %d %d %d %d %d %d]\n",
		    cellincs[0], cellincs[1], cellincs[2],
		    cellincs[3], cellincs[4], cellincs[5],
		    cellincs[6], cellincs[7], cellincs[8]);
	}
    }
}

/*
 * This searches through double shift loops.
 * The text is then generated using a closest next function.
 * Returns the shortest.
 */
void
gen_nestloop(char * buf)
{
    int cellincs[10] = {0};
    int maxcell = 0;
    int currcell=0, usecell=0;
    int i;
    char * p;

    for(;;)
    {
	{
	    for(i=0; ; i++) {
		if (i == 7) return;
		cellincs[i]++;
		if (cellincs[i] <= 25) break;
		cellincs[i] = 1;
	    }

	    for(maxcell=0; cellincs[maxcell] != 0; )
		maxcell++;

	    maxcell--; currcell=0, usecell=0;
	}

	reinit_state();
	if (flg_init) {
	    /* Clear the working cells */
	    for(i=maxcell; i>=0; i--) {
		while(currcell < i) { add_chr('>'); currcell++; }
		while(currcell > i) { add_chr('<'); currcell--; }
		add_str("[-]");
		cells[i] = 0;
	    }
	    cells_dirty = 0;
	}

	str_cells_used = maxcell+1;

	if (verbose>1)
	    fprintf(stderr, "Trying nestloop: %d * [%d %d %d %d %d %d %d %d]\n",
		cellincs[0], cellincs[1], cellincs[2],
		cellincs[3], cellincs[4], cellincs[5],
		cellincs[6], cellincs[7], cellincs[8]);

	if (!cells_dirty) {
	    int j;

//  ++++++++[>++++[>++>+++>+++>+<<<<-]>+>->+>>+[<]<-]

// >N+>N+>N+>N+>N+<[>[->N++++<<N+++>]<<]

	    for(j=0; j<cellincs[0]; j++)
		add_chr('+');
	    add_chr('[');
	    add_chr('>');
	    for(j=0; j<cellincs[1]; j++)
		add_chr('+');
	    add_chr('[');

	    for(i=2; i<=maxcell; i++) {
		add_chr('>');
		for(j=0; j<cellincs[i]/5; j++)
		    add_chr('+');
	    }

	    for(i=2; i<=maxcell; i++)
		add_chr('<');
	    add_chr('-');
	    add_chr(']');

	    for(i=2; i<=maxcell; i++) {
		add_chr('>');
		j = cellincs[i]%5 - 2;
		if (j >= 2) add_chr('+');
		if (j >= 1) add_chr('+');
		if (j <= -1) add_chr('-');
		if (j <= -2) add_chr('-');
	    }

	    if (maxcell <= 4) {
		for(i=1; i<=maxcell; i++)
		    add_chr('<');
	    } else {
		add_str("[<]<");
	    }
	    add_str("-]");

	    if (verbose>1)
		fprintf(stderr, "Running nestloop: %s\n", str_start);

	    /* This is the most reliable way of making sure we have the correct
	     * cells position as produced by the string we've built so far.
	     */
	    {
		char *codeptr, *b;
		int m=0, loopinc=0;
		int countdown = 2000;

		for(codeptr=b=str_start;*codeptr;codeptr++) {
		    switch(*codeptr) {
			case '>': m++;break;
			case '<': m--;break;
			case '+': cells[m]++;break;
			case '-': cells[m]--;break;
			case '[': if(cells[m]==0)while((loopinc+=(*codeptr=='[')-(*codeptr==']'))&&codeptr[1])codeptr++;break;
			case ']': if(cells[m]!=0)while((loopinc+=(*codeptr==']')-(*codeptr=='['))&&codeptr>b)codeptr--;break;
		    }
		    if (m<0 || m>maxcell || --countdown == 0) break;
		}

		/* Something went wrong; the code is invalid */
		if (m!=0 || countdown <= 0) continue;
	    }
	}

	if (verbose>1)
	    fprintf(stderr, "Counting nestloop\n");

	cells_dirty = 1;

	/* Print each character */
	for(p=buf; *p;) {
	    int minrange = 255;
	    int c = *p++;
	    if (!flg_signed) c &= 0xFF;

	    usecell = 0;
	    for(i=0; i<=maxcell; i++) {
		int range = abs(currcell-i) + abs(cells[i] - c);
		if (range < minrange) {
		    usecell = i;
		    minrange = range;
		}
	    }

	    while(currcell > usecell) { add_chr('<'); currcell--; }
	    while(currcell < usecell) { add_chr('>'); currcell++; }

	    while(cells[currcell] < c) { add_chr('+'); cells[currcell]++; }
	    while(cells[currcell] > c) { add_chr('-'); cells[currcell]--; }

	    add_chr('.');
	}

	if (flg_clear || flg_init) {
	    /* Clear the working cells */
	    for(i=maxcell; i>=0; i--) {
		if (cells[i] != 0) {
		    while(currcell < i) { add_chr('>'); currcell++; }
		    while(currcell > i) { add_chr('<'); currcell--; }
		    if (cells[i] < 0)
			add_str("[+]");
		    else if (cells[i] > 0 && flg_clear)
			add_str("[-]");
		    cells[i] = 0;
		}
	    }
	    while(currcell > 0) { add_chr('<'); currcell--; }
	    cells_dirty = 0;
	}

	/* Put the pointer back to the zero cell */
	if (!flg_nortz) while(currcell > 0) { add_chr('<'); currcell--; }

/******************************************************************************/
	if (best_len == -1 || best_len > str_next || (best_len == str_next && str_cells_used < best_cells)) {
	    if (best_str) free(best_str);
	    best_str = strdup(str_start);
	    best_len = str_next;
	    best_cells = str_cells_used;

	    if (verbose)
		fprintf(stderr, "Found nestloop: %d %d %d %d %d %d %d %d %d: (%d) %s\n",
		    cellincs[0], cellincs[1], cellincs[2],
		    cellincs[3], cellincs[4], cellincs[5],
		    cellincs[6], cellincs[7], cellincs[8],
		    best_len, best_str);
	}
    }
}
