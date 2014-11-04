/*
 *  This program takes a text input and generates BF code to create that text.
 *
 *  If the input text is larger than 'linebuf' it uses a simple algorithmic
 *  method to generate the code from a fixed 'init string'.
 *
 *  If the input fits in linebuf this program will search for the shortest
 *  piece of BF code it can. If all the search options are turned on it can
 *  take a VERY long time to run. But it can give very good results.
 *
 *  For small text first the fixed init strings are tested.
 *  Secondly a generic two cell generator is tried.
 *
 *  The 'subrange' method is uses a multiply loop to set a collection of
 *  cells to the center values of short ranges of possible values. The
 *  cell values are sorted by frequency. Each cell is either used only
 *  with the characters from the 'correct' zone or using a find the
 *  closest cell method.
 *
 *  The 'subrange' routine is run with several different zone sizes.
 *
 *  Up to this point the trials are very quick, the following searches tend
 *  to take a lot longer.
 *
 *  Optionally a short or long search of multiply strings may be done.
 *  Optionally a short or long search of 'slipping loop' strings may be done.
 *  Optionally a long search of 'nested loop' strings may be done.
 *
 *  In addition the search may include a manually chosen init string.
 *
 *  NOTE: Null bytes are ignored.
 *
 *  Also:
 *      ++>+++>+>+++++>+++++++++++>-<[<<<<++++>++++++++>++
 *      ++>++++++++>->++++<],[[>.<-]<<<<[.>],]++++++++++.
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void check_if_best(char * name);
void gen_unzoned(char * buf);

void gen_subrange(char * buf, int subrange, int flg_zoned, int flg_nonl);
void gen_ascii(char * buf);
void gen_multonly(char * buf);
void gen_nestloop(char * buf);
void gen_slipnest(char * buf);
void gen_special(char * buf, char * initcode, char * name);
void gen_twoflower(char * buf);
int runbf(char * prog, int m);

#define MAX_CELLS 256

/* These setup strings have, on occasion, given good results */

/* Some very generic multiply loops */
#define RUNNERCODE1 "++++++++++++++[>+++++>+++++++>++++++++>++>++++++<<<<<-]"
#define RUNNERCODE2 ">+++++[<++++++>-]<++[>+>++>+++>++++<<<<-]"
#define RUNNERCODE3 ">+++++[<++++++>-]<++[>+>++>+++>++++>+++++>++++++>+++++++<<<<<<<-]"
#define RUNNERCODE4 ">+++++[<++++++>-]<++[>+>++>+++>++++>->-->---<<<<<<<-]"

/* Some Hello World prefixes */
#define HELLOCODE  "++++++++++[>+++++++>++++++++++>+++>+<<<<-]"
#define HELLOCODE1 "++++++++[>++++[>++>+++>+++>+<<<<-]>+>+>->>+[<]<-]"
#define HELLOCODE2 ">+>+>++>++<[>[->++++<<+++>]<<]"
#define HELLOCODE3 ">+>++++>+>+>+++<[>[->+++<<++>]<<]"
#define HELLOCODE4 "+++++++++[>++++++[>++>++>++>++>+<<<<<-]>>+>->>-[<]<-]"
#define HELLOCODE5 ">+>+>++>+++<[>[->++<<+++>]<<]"
#define HELLOCODE6 ">+>+>++>+++<[>[->+++<<+++>]<<]"

/* This is a prefix for huge ASCII files */
#define HUGEPREFIX \
		    "++++++++[>+++++++++++++>++++>++++++++++++++>++++++" \
		    "+++++++++>++++++++++++>++++++>+++++++>++++++++>+++" \
		    "++++++>++++++++++>+++++++++++>+++++<<<<<<<<<<<<-]"

char linebuf[30000];        /* Only one buffer full for searching.  */
int flg_init = 0;
int flg_clear = 0;
int flg_signed = 1;
int flg_rtz = 0;

int enable_multloop = 0;
int enable_multdblloop = 0;
int multloop_maxcell = 5;
int multloop_maxinc = 10;
int multloop_maxloop = 20;
int flg_nestloop = 0;

int enable_sliploop = 0;
int sliploop_maxcell = 7;
int sliploop_maxind = 7;
int enable_twocell = 1;

int enable_subrange = 1;
int subrange_count = 0;
int verbose = 0;

int bytewrap = 0;
int maxcol = 72;

void reinit_state(void);
void output_str(char * s);
int add_chr(int ch);
void add_str(char * p);

char * str_start = 0;
int str_max = 0, str_next = 0;
int str_cells_used = 0;

char * best_str = 0;
int best_len = -1;
int best_cells = 1;

char * special_init = 0;

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
	} else if (strncmp(argv[1], "-v", 2) == 0) {
	    if (argv[1][2])
		verbose = strtol(argv[1]+2, 0, 10);
	    else
		verbose++;
	    argc--; argv++;
	} else if (strcmp(argv[1], "-init") == 0) {
	    flg_init = 1;
	    argc--; argv++;
	} else if (strcmp(argv[1], "-noinit") == 0) {
	    flg_init = 0;
	    argc--; argv++;
	} else if (strcmp(argv[1], "-clear") == 0) {
	    flg_clear = 1;
	    argc--; argv++;
	} else if (strcmp(argv[1], "-noclear") == 0) {
	    flg_clear = 0;
	    argc--; argv++;
	} else if (strcmp(argv[1], "-max") == 0) {
	    enable_sliploop = 1;
	    sliploop_maxcell = 8;
	    sliploop_maxind = 8;
	    enable_multloop = 1;
	    multloop_maxcell = 7;
	    multloop_maxinc = 12;
	    multloop_maxloop = 32;
	    enable_multdblloop = 1;
	    enable_twocell = 1;
	    enable_subrange = 1;
	    flg_nestloop = 1;
	    argc--; argv++;
	} else if (strncmp(argv[1], "-I", 2) == 0) {
	    special_init = argv[1]+2;
	    enable_sliploop = 0;
	    enable_multloop = 0;
	    enable_multdblloop = 0;
	    enable_subrange = 0;
	    enable_twocell = 0;
	    flg_nestloop = 0;
	    flg_init = 0;
	    flg_clear = 0;
	    flg_rtz = 0;

	    argc--; argv++;

	} else if (strcmp(argv[1], "-O") == 0) {
	    special_init = RUNNERCODE1;
	    enable_multloop = 1;
	    multloop_maxcell = 5;
	    multloop_maxinc = 10;
	    multloop_maxloop = 20;
	    enable_multdblloop = 0;
	    enable_subrange = 1;
	    enable_twocell = 1;
	    flg_nestloop = 0;
	    flg_init = 1;
	    flg_clear = 0;
	    flg_rtz = 0;

	    argc--; argv++;

	} else if (strcmp(argv[1], "-i") == 0) {
	    flg_init = !flg_init;
	    argc--; argv++;
	} else if (strcmp(argv[1], "-c") == 0) {
	    flg_clear = !flg_clear;
	    argc--; argv++;
	} else if (strcmp(argv[1], "-sc") == 0) {
	    flg_signed = 1;
	    argc--; argv++;
	} else if (strcmp(argv[1], "-uc") == 0) {
	    flg_signed = 0;
	    argc--; argv++;
	} else if (strcmp(argv[1], "-b") == 0) {
	    bytewrap = !bytewrap;
	    argc--; argv++;
	} else if (strcmp(argv[1], "-z") == 0) {
	    flg_rtz = !flg_rtz;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-slip") == 0) {
	    enable_sliploop = 1;
	    sliploop_maxcell = 7;
	    sliploop_maxind = 6;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-slip2") == 0) {
	    enable_sliploop = 1;
	    sliploop_maxcell = 8;
	    sliploop_maxind = 7;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-D") == 0) {
	    enable_multloop = 1;
	    enable_multdblloop = 1;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-mult") == 0) {
	    enable_multloop = 1;
	    multloop_maxcell = 5;
	    multloop_maxinc = 10;
	    multloop_maxloop = 20;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-mult2") == 0) {
	    enable_multloop = 1;
	    multloop_maxcell = 7;
	    multloop_maxinc = 12;
	    multloop_maxloop = 32;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-q") == 0) {
	    enable_twocell = !enable_twocell;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-X") == 0) {
	    enable_subrange = !enable_subrange;
	    argc--; argv++;
	} else if (strncmp(argv[1], "-s", 2) == 0 && argv[1][2] >= '0' && argv[1][2] <= '9') {
	    subrange_count = atol(argv[1]+2);
	    enable_subrange = 1;
	    argc--; argv++;

	} else if (strncmp(argv[1], "-M", 2) == 0) {
	    enable_multloop = 1;
	    if (argv[1][2] == '+')
		multloop_maxinc = atol(argv[1]+3);
	    if (argv[1][2] == '*')
		multloop_maxloop = atol(argv[1]+3);
	    if (argv[1][2] == '=')
		multloop_maxcell = atol(argv[1]+3);
	    argc--; argv++;
	} else if (strcmp(argv[1], "-N") == 0) {
	    flg_nestloop = !flg_nestloop;
	    argc--; argv++;

	} else if (strncmp(argv[1], "-S", 2) == 0) {
	    special_init = argv[1]+2;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-h") == 0) {
	    fprintf(stderr, "txtbf [options] [file]\n");
	    fprintf(stderr, "-v      Verbose\n");
	    fprintf(stderr, "-max    Enable everything -- very slow\n");
	    fprintf(stderr, "-I><    Enable only special string.\n");
	    fprintf(stderr, "-O      Enable easy to optimise codes.\n");
	    fprintf(stderr, "\n");
	    fprintf(stderr, "-i      Add/remove cell init strings and return to cell zero\n");
	    fprintf(stderr, "-c      Add/remove cell clear strings and return to cell zero\n");
	    fprintf(stderr, "-z      Set/clear Return to cell zero\n");
	    fprintf(stderr, "-sc/-uc Assume chars are signed/unsigned\n");
	    fprintf(stderr, "-b      Assume cells are bytes\n");
	    fprintf(stderr, "\n");
	    fprintf(stderr, "-q      Enable/disable two cells method\n");
	    fprintf(stderr, "-X      Enable/disable subrange multiply method\n");
	    fprintf(stderr, "\n");
	    fprintf(stderr, "-mult   Search short multiply loops\n");
	    fprintf(stderr, "-mult2  Search long multiply loops\n");
	    fprintf(stderr, "-D      Search double multiply loops\n");
	    fprintf(stderr, "-slip   Search short slip loops\n");
	    fprintf(stderr, "-slip2  Search long slip loops\n");
	    fprintf(stderr, "-N      Search nested loops v.long\n");
	    fprintf(stderr, "-s10    Try just one subrange multiply size\n");
	    fprintf(stderr, "-M+99   Specify multiply loop max increment\n");
	    fprintf(stderr, "-M*99   Specify multiply loop max loops\n");
	    fprintf(stderr, "-M=99   Specify multiply loop max cells\n");
	    fprintf(stderr, "-S+>+<  Try one special string (disable builtins)\n");
	    exit(0);
	} else {
	    fprintf(stderr, "Unknown option '%s'\n", argv[1]);
	    exit(1);
	}
    }
    flg_rtz = (flg_rtz || flg_init || flg_clear);
    if (bytewrap) flg_signed = 0;
    if (argc < 2 || strcmp(argv[1], "-") == 0)
	ifd = stdin;
    else
	ifd = fopen(argv[1], "r");
    if (ifd == 0) perror(argv[1]);
    else {
	int c;
	char * p = linebuf;
	int eatnl = 0;
	int override_for_bigtext = 0, last_clear = 0;

	while((c = fgetc(ifd)) != EOF) {
	    if (c == '\r') { eatnl = 1; c = '\n'; }
	    else if (c == '\n' && eatnl) { eatnl = 0; continue; }
	    else eatnl = 0;
	    if (c == 0) continue;

	    *p++ = c;
	    if (p-linebuf >= (int)sizeof(linebuf)-1) {
		*p++ = 0;
		if (!override_for_bigtext) {
		    override_for_bigtext = 1;
		    best_len = -2; /* Disable 'best' processing. */

		    if (special_init == 0)
			special_init = HUGEPREFIX;

		    fprintf(stderr, "Input too large, using '%s'.\n",
			    special_init);

		    last_clear = flg_clear;
		    flg_clear = 0; flg_rtz = 1;
		    gen_special(linebuf, special_init, "Big buffer");
		    flg_init = 0;
		} else
		    gen_unzoned(linebuf);

		p = linebuf;

		if (str_start) {
		    output_str(str_start);
		    str_start[str_next=0] = 0;
		}
	    }
	}
	*p++ = 0;
	if (ifd != stdin) fclose(ifd);

	if (override_for_bigtext) {
	    flg_clear = last_clear;
	    gen_unzoned(linebuf);
	    if (str_start)
		output_str(str_start);
	} else {

	    if (special_init != 0)
		gen_special(linebuf, special_init, "cmd special");

	    if (subrange_count <= 0 && !special_init) {
		if (verbose>2) fprintf(stderr, "Trying special strings\n");
		gen_special(linebuf, HUGEPREFIX, "big ASCII");
		gen_special(linebuf, RUNNERCODE1, "mult English");
		gen_special(linebuf, RUNNERCODE2, "mult*32 to 128");
		gen_special(linebuf, RUNNERCODE3, "mult*32 to 224");
		gen_special(linebuf, RUNNERCODE4, "mult*32 to 128/-96");
		gen_special(linebuf, HELLOCODE, "hello111 mult");
		gen_special(linebuf, HELLOCODE1, "hello106 nest");
		gen_special(linebuf, HELLOCODE2, "hello104 slip");
		gen_special(linebuf, HELLOCODE3, "hello119 slip");
		gen_special(linebuf, HELLOCODE4, "hello183 nest");
		gen_special(linebuf, HELLOCODE5, "hello120 slip");
		gen_special(linebuf, HELLOCODE6, "BFoRails slip");
	    }

	    if (enable_twocell) {
		if (verbose>2) fprintf(stderr, "Trying two cell routine.\n");
		gen_twoflower(linebuf);
	    }

	    if (subrange_count > 0) {
		reinit_state();
		gen_subrange(linebuf,subrange_count,1,0);
	    } else if (subrange_count == 0) {

		if (enable_subrange) {
		    if (verbose>2) fprintf(stderr, "Trying subrange routines.\n");
		    subrange_count=10;
		    {
			reinit_state();
			gen_subrange(linebuf,subrange_count,1,0);
		    }

		    for(subrange_count = 2; subrange_count<33; subrange_count++)
			if (subrange_count!=10) {
			    reinit_state();
			    gen_subrange(linebuf,subrange_count,1,0);
			}

		    for(subrange_count = 2; subrange_count<33; subrange_count++) {
			reinit_state();
			gen_subrange(linebuf,subrange_count,0,0);
		    }

		    for(subrange_count = 2; subrange_count<33; subrange_count++) {
			reinit_state();
			gen_subrange(linebuf,subrange_count,0,1);
		    }
		}
	    }

	    if (enable_multloop) {
		if (verbose>2) fprintf(stderr, "Trying multiply loops.\n");
		gen_multonly(linebuf);
	    }

	    if (enable_sliploop) {
		if (verbose>2) fprintf(stderr, "Trying 'slipping loop' routine.\n");
		gen_slipnest(linebuf);
	    }

	    if(flg_nestloop) {
		if (verbose>2) fprintf(stderr, "Trying 'nested loop' routine.\n");
		reinit_state();
		gen_nestloop(linebuf);
	    }

	    if (best_len>=0) {
		if (verbose)
		    fprintf(stderr, "BF Size = %d, %.2f bf/char, cells = %d\n",
			best_len, best_len * 1.0/ strlen(linebuf), best_cells);

		output_str(best_str);
		free(str_start); str_start = 0; str_max = str_next = 0;
	    }
	}
	output_str("\n");
    }

    return 0;
}

static void
pc(int ch)
{
static int col = 0;
#if 0
static int bf_mov = 0;

    if (ch == '>') { bf_mov++; return; }
    else if (ch == '<') { bf_mov--; return; }
    else {
	while (bf_mov>0) {bf_mov--; pc('>' | 0x100); }
	while (bf_mov<0) {bf_mov++; pc('<' | 0x100); }
    }
#endif

    if ((col>=maxcol && maxcol) || ch == '\n') {
        putchar('\n');
        col = 0;
        if (ch == ' ' || ch == '\n') ch = 0;
    }
    if (ch) { putchar(ch & 0xFF); col++; }
}

void
output_str(char * s)
{
    while (*s) pc(*s++);
}

void
check_if_best(char * name)
{
    if (best_len == -1 || best_len > str_next || (best_len == str_next && str_cells_used < best_cells)) {
	if (best_str) free(best_str);
	best_str = strdup(str_start);
	best_len = str_next;
	best_cells = str_cells_used;
    }

    if (best_len == str_next && str_cells_used == best_cells) {
	/* Note this also shows strings of same length */
	if (verbose>2)
	    fprintf(stderr, "Found '%s' len=%d, cells=%d, '%s'\n",
		    name, str_next, str_cells_used, str_start);
	else if (verbose>1)
	    fprintf(stderr, "Found '%s' len=%d, cells=%d\n",
		    name, str_next, str_cells_used);
    }
}

static int cells[MAX_CELLS];

inline int
add_chr(int ch)
{
    int lastch = 0;
    int rv = (best_len<1 || best_len >= str_next);
    while (str_next+2 > str_max) {
	str_start = realloc(str_start, str_max += 1024);
	if(!str_start) { perror("realloc"); exit(1); }
    }
    if (str_next > 0) lastch = str_start[str_next-1];

    if ( (ch == '>' && lastch == '<') || (ch == '<' && lastch == '>') ) {
	str_start[--str_next] = 0;
	return rv;
    }

    str_start[str_next++] = ch;
    str_start[str_next] = 0;

    return rv;
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
    str_next = 0;
    if (str_start) str_start[str_next] = 0;
    else add_str("<>");
}

int
clear_tape(int currcell)
{
    if (flg_clear || flg_init) {
	int i;

	/* Clear the working cells */
	for(i=MAX_CELLS-1; i>=0; i--) {
	    if ((cells[i] != 0 && flg_clear) || (!bytewrap && cells[i] <0) ) {
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
    }

    /* Put the pointer back to the zero cell */
    if (flg_rtz) while(currcell > 0) { add_chr('<'); currcell--; }

    return currcell;
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

    Non-ASCII characters can be printed using the negative aliases of
    the bytes: ie: chars are signed. However, if it's set negative a
    cell will be zeroed at the end of the fragment so [-] is safe even
    with bignum cells.

    The reason negatives would be used is to be compatible with interpreters
    that use -1 as the EOF indicator. NB: This byte is illegal for UTF-8.

 */

void
gen_subrange(char * buf, int subrange, int flg_zoned, int flg_nonl)
{
    int counts[256] = {};
    int txn[256] = {};
    int rng[256] = {};
    int currcell=0, usecell=0;
    int maxcell = 0;
    char * p;
    int i, j;
    char name[256];

    sprintf(name, "Subrange %d, %s%s", subrange,
	    flg_zoned?"zoned":"unzoned",flg_nonl?", nonl":"");

    /* Count up a frequency table */
    for(p=buf; *p;) {
	int c = *p++;
	int r;
	if (flg_signed) c = (signed char)c; else c = (unsigned char)c;
	r = (c+subrange/2)/subrange;
	usecell = (r & 0xFF);
	rng[usecell] = r;

	if (usecell > maxcell) maxcell = usecell;

	if (flg_nonl && c == '\n') continue;
	if (r == 0) continue;
	counts[usecell] ++;
    }

    for(i=0; i<=maxcell; i++) txn[i] = i;

    /* Dumb sort for simplicity */
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
    for(i=0; i<=maxcell; i++) txn[rng[i] & 0xFF] = i;

    if (flg_init) {
	/* Clear the working cells */
	for(i=maxcell; i>=0; i--) {
	    if (counts[i] || i == 0) {
		while(currcell < i) { add_chr('>'); currcell++; }
		while(currcell > i) { add_chr('<'); currcell--; }
		add_str("[-]");
		cells[i] = 0;
	    }
	}
    }

    str_cells_used = 0;
    if (maxcell>1)
	for(i=maxcell; i>=0; i--)
	    if ((counts[i] || i == 0) && i>str_cells_used)
		str_cells_used = i;
    str_cells_used++;

    /* Generate the init strings */
    if (maxcell > 1) {
	if (subrange>15) {
	    char best_factor[] = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
		4, 4, 6, 6, 5, 7, 7, 7, 6, 5, 5, 9, 7, 7, 6, 6,
		8, 8, 8, 7, 6, 6, 6, 6, 8, 8, 7, 7,11, 9, 9, 9};

	    int l,r;
	    if (subrange<(int)sizeof(best_factor))
		l = best_factor[subrange];
	    else l = 10;
	    r = subrange/l;
	    if(r>1) add_chr('>');
	    for(i=0; i<l; i++) add_chr('+');
	    if(r>1) {
		add_str("[<");
		for(i=0; i<r; i++) add_chr('+');
		add_str(">-]<");
	    }
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
		if(bytewrap) cells[currcell] &= 255;
		for(j=0; j<rng[i]; j++) add_chr('+');
		for(j=0; j<-rng[i]; j++) add_chr('-');
	    }
	}
	while(currcell > 0) { add_chr('<'); currcell--; }
	add_chr('-');
	add_chr(']');
    }
    while(currcell > 0) { add_chr('<'); currcell--; }

    if (best_len>0 && str_next > best_len) return;	/* Too big already */

    if (verbose>3)
	fprintf(stderr, "Trying %s: %s\n", name, str_start);

    /* We have two choices on how to choose which cell to use; either pick
     * whatever is best right now, or stick with the ranges we chose above.
     * Either could end up better by the end of the string.
     */
    if (!flg_zoned) {
	gen_unzoned(buf);
    } else {
	/* Print each character */
	for(p=buf; *p;) {
	    int c = *p++;
	    if (flg_signed) c = (signed char)c; else c = (unsigned char)c;
	    usecell = ((c+subrange/2)/subrange & 0xFF);
	    usecell = txn[usecell];

	    while(currcell > usecell) { add_chr('<'); currcell--; }
	    while(currcell < usecell) { add_chr('>'); currcell++; }

	    while(cells[currcell] < c) { add_chr('+'); cells[currcell]++; }
	    while(cells[currcell] > c) { add_chr('-'); cells[currcell]--; }

	    add_chr('.');
	}

	currcell = clear_tape(currcell);
    }

    check_if_best(name);
}

/******************************************************************************/
/*
   This does a brute force search through all the simple multiplier
   loops upto a certain size.

   The text is then generated using a closest next function.
 */
void
gen_multonly(char * buf)
{
    int cellincs[10] = {0};
    int maxcell = 0;
    int currcell=0;
    int i;

    for(;;)
    {
return_to_top:
	{
	    int toohigh = 0;
	    for(i=0; ; i++) {
		if (i >= multloop_maxcell) return;
		cellincs[i]++;
		if (i == 0) {
		    if (cellincs[i] <= multloop_maxloop) break;
		    if (cellincs[i] < 127 && bytewrap) {
			cellincs[i] = 127;
			break;
		    }
		} else {
		    if (!bytewrap && cellincs[i]*cellincs[0] > 255+multloop_maxloop)
			toohigh = 1;
		    if (cellincs[i] <= multloop_maxinc) break;
		}
		cellincs[i] = 1;
	    }

	    for(maxcell=0; cellincs[maxcell] != 0; )
		maxcell++;

	    maxcell--; currcell=0;
	    if (toohigh) goto return_to_top;
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
	}

	str_cells_used = maxcell+1;

	if (cellincs[0] == 127 && bytewrap) {
	    add_str(">++[<+>++]<");
	} else if (cellincs[0]>15) {
	    char best_factor[] = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
		4, 4, 6, 6, 5, 7, 7, 7, 6, 5, 5, 9, 7, 7, 6, 6,
		8, 8, 8, 7, 6, 6, 6, 6, 8, 8, 7, 7,11, 9, 9, 9};

	    int l,r;
	    int sr = cellincs[0];
	    if (sr<(int)sizeof(best_factor))
		l = best_factor[sr];
	    else l = 10;
	    r = sr/l;
	    if(r>1) add_chr('>');
	    for(i=0; i<l; i++) add_chr('+');
	    if(r>1) {
		add_str("[<");
		for(i=0; i<r; i++) add_chr('+');
		add_str(">-]<");
	    }
	    for(i=l*r; i<sr; i++) add_chr('+');
	} else {
	    for(i=0; i<cellincs[0]; i++)
		add_chr('+');
	}
	if (maxcell == 1 && cellincs[1] == 1) {
	    /* Copy loop */
	    cells[0] = cellincs[0];
	    cells[1] = 0;
	} else {
	    add_chr('[');

	    cells[0] = 0;
	    for(i=1; i<=maxcell; i++) {
		int j;
		cells[i] = cellincs[i] * cellincs[0];
		if(bytewrap) cells[i] &= 255;
		add_chr('>');
		for(j=0; j<cellincs[i]; j++)
		    add_chr('+');
	    }

	    for(i=1; i<=maxcell; i++)
		add_chr('<');
	    add_chr('-');
	    add_chr(']');
	}

	if (best_len>0 && str_next > best_len) goto return_to_top;	/* Too big already */

	if (verbose>3)
	    fprintf(stderr, "Trying multiply: %s\n", str_start);

	gen_unzoned(buf);

	check_if_best("multiply loop");
    }
}

/******************************************************************************/
/*
   This searches through double multipler loops, with an extra addition or
   subtraction on each cell round the outer loop.

   The configurations are are in the defines below for the moment.

   The text is then generated using a closest next function.
 */
#define ADDINDEX 3
#define MAXCELLINCS 7
void
gen_nestloop(char * buf)
{
    int cellincs[MAXCELLINCS+2] = {0};
    int maxcell = 0;
    int currcell=0;
    int i,j;

    for(;;)
    {
return_to_top:
	{
	    for(i=0; ; i++) {
		if (i == MAXCELLINCS) return;
		cellincs[i]++;
		if (cellincs[i] <= MAXCELLINCS*ADDINDEX) break;
		cellincs[i] = 1;
	    }

	    for(maxcell=0; cellincs[maxcell] != 0; )
		maxcell++;

	    maxcell--; currcell=0;
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
	}

	str_cells_used = maxcell+1;

	if (verbose>3)
	    fprintf(stderr, "Trying nestloop: %d * [%d %d %d %d %d %d %d %d]\n",
		cellincs[0], cellincs[1], cellincs[2],
		cellincs[3], cellincs[4], cellincs[5],
		cellincs[6], cellincs[7], cellincs[8]);

//  ++++++++[>++++[>++>+++>+++>+<<<<-]>+>->+>>+[<]<-]

	for(j=0; j<cellincs[0]; j++)
	    add_chr('+');
	add_chr('[');
	add_chr('>');
	for(j=0; j<cellincs[1]; j++)
	    add_chr('+');
	add_chr('[');

	for(i=2; i<=maxcell; i++) {
	    add_chr('>');
	    for(j=0; j<cellincs[i]/ADDINDEX; j++)
		add_chr('+');
	}

	for(i=2; i<=maxcell; i++)
	    add_chr('<');
	add_chr('-');
	add_chr(']');

	currcell=1;
	for(i=2; i<=maxcell; i++) {
	    j = cellincs[i]%ADDINDEX - (ADDINDEX/2);
	    if (j) {
		while (currcell<i) {
		    add_chr('>');
		    currcell++;
		}
		while (j>0) {add_chr('+'); j--; }
		while (j<0) {add_chr('-'); j++; }
	    }
	}

	if (currcell <= 4) {
	    while(currcell > 0) {
		add_chr('<');
		currcell--;
	    }
	} else {
	    add_str("[<]<");
	}
	add_str("-]");

	if (verbose>3)
	    fprintf(stderr, "Running nestloop: %s\n", str_start);

	if (best_len>0 && str_next > best_len) goto return_to_top;	/* Too big already */

	/* This is the most reliable way of making sure we have the correct
	 * cell values as produced by the string we've built so far.
	 */
	if (runbf(str_start, 0)) continue;

	if (verbose>3)
	    fprintf(stderr, "Counting nestloop\n");

	gen_unzoned(buf);

	check_if_best("nested loop");
    }
}

/******************************************************************************/

void
gen_slipnest(char * buf)
{
    int cellincs[10] = {0};
    int maxcell = 0;
    int currcell=0;
    int i;
    int slipcount = 0;

    for(;;)
    {
return_to_top:
	{
	    for(i=0; ; i++) {
		if (i == sliploop_maxind) return;
		if (i>slipcount) slipcount = i;
		cellincs[i]++;
		if (i<2 && cellincs[i] > 5)
		    ;
		else
		if (cellincs[i] <= sliploop_maxcell) break;
		cellincs[i] = (i<2);
	    }

	    maxcell = slipcount -2;
	    currcell=0;
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
	}

	for(i=2; i<=slipcount; i++) {
	    int j;
	    add_chr('>');
	    for(j=0; j<cellincs[i]; j++)
		add_chr('+');
	}
	add_str("<[>[->");
	for(i=0; i<cellincs[0]; i++)
	    add_chr('+');
	add_str("<<");
	for(i=0; i<cellincs[1]; i++)
	    add_chr('+');
	add_str(">]<<]");

	if (verbose>3)
	    fprintf(stderr, "Running sliploop: %s\n", str_start);

	if (best_len>0 && str_next > best_len) goto return_to_top;	/* Too big already */

	/* This is the most reliable way of making sure we have the correct
	 * cells position as produced by the string we've built so far.
	 */
	if (runbf(str_start, 0) != 0) continue;

	if (verbose>3)
	    fprintf(stderr, "Counting sliploop\n");

	gen_unzoned(buf);

	check_if_best("slipping loop");
    }
}

/******************************************************************************/
/*
   This is a simple 'Special case' text generator.
   Given a piece of BF code to fill in some cells it then uses those cells to
   print the text you give it.

   The code must take no more then 10000 clocks.
   For computation purposes any '.' and ',' commands are ignored.

   An empty string is a single cell set to zero, '>' is two cells set to zero.

   The cells are used in closest next order.
 */

void
gen_special(char * buf, char * initcode, char * name)
{
    int maxcell = 0;
    int currcell=0;
    int remaining_offset = 0;
    int i;

    reinit_state();

    if (verbose>3)
	fprintf(stderr, "Running Special code: %s\n", initcode);

    /* Work out what the 'special' code does to the cell array ... How?
     *
     * By running it of course!
     * Use a local interpreter so we can increase the counter and give a better
     * error message.
     */
    if(0) {
	remaining_offset = runbf(initcode, 0);
	if (remaining_offset<0) {
	    fprintf(stderr, "Special code failed '%s'\n", initcode);
	    return;
	}
    } else {
	char *codeptr, *b;
	int m=0, nestlvl=0;
	int countdown = 100000;

	for(codeptr=b=initcode;*codeptr;codeptr++) {
	    switch(*codeptr) {
		case '>': m++;break;
		case '<': m--;break;
		case '+': cells[m]++;break;
		case '-': cells[m]--;break;
		case '[': if(cells[m]==0)while((nestlvl+=(*codeptr=='[')-(*codeptr==']'))&&codeptr[1])codeptr++;break;
		case ']': if(cells[m]!=0)while((nestlvl+=(*codeptr==']')-(*codeptr=='['))&&codeptr>b)codeptr--;break;
	    }
	    if (m<0 || m>= MAX_CELLS || --countdown == 0) break;
	    if (m>maxcell) maxcell = m;
	    if (bytewrap) cells[m] &= 255;
	}

	/* Something went wrong; the code is invalid */
	if (*codeptr) {
	    fprintf(stderr, "Special code failed to run cellptr=%d countdown=%d iptr=%d '%s'\n",
		    m, countdown, codeptr-initcode, initcode);
	    return;
	}
	remaining_offset = m;
    }

    if (flg_init) {
	/* Clear the working cells */
	for(i=maxcell; i>=0; i--) {
	    while(currcell < i) { add_chr('>'); currcell++; }
	    while(currcell > i) { add_chr('<'); currcell--; }
	    add_str("[-]");
	}
    }

    str_cells_used = maxcell+1;

    add_str(initcode);
    while(remaining_offset<0) { add_chr('>'); remaining_offset++; }
    while(remaining_offset>0) { add_chr('<'); remaining_offset--; }

    if (best_len>0 && str_next > best_len) return;	/* Too big already */

    gen_unzoned(buf);

    check_if_best(name);
}

/*******************************************************************************
 * Given a set of cells and a string to generate them already in the buffer
 * this routine uses those cells to make the string.
 */
void
gen_unzoned(char * buf)
{
    char * p;
    int i;
    int currcell = 0;
    if (str_cells_used == 0) str_cells_used=1;

    /* Print each character, use closest cell. */
    for(p=buf; *p;) {
	int minrange = 999999;
	int c = *p++;
	int usecell, diff;
	if (flg_signed) c = (signed char)c; else c = (unsigned char)c;
	usecell = 0;
	for(i=0; i<str_cells_used; i++) {
	    int range = c - cells[i];
	    if (bytewrap) range = (signed char)range;
	    range = abs(range);
	    range += abs(currcell-i);
	    if (range == minrange) {
		if (abs(currcell-i) < abs(usecell-i)) {
		    usecell = i;
		    minrange = range;
		}
	    } else if (range < minrange) {
		usecell = i;
		minrange = range;
	    }
	}

	while(currcell > usecell) { add_chr('<'); currcell--; }
	while(currcell < usecell) { add_chr('>'); currcell++; }

	diff = c - cells[currcell];
	if (bytewrap) diff = (signed char)diff;
	while(diff>0) { add_chr('+'); cells[currcell]++; diff--; }
	while(diff<0) { add_chr('-'); cells[currcell]--; diff++; }
	if (bytewrap) cells[currcell] &= 255;

//	while(cells[currcell] < c) { add_chr('+'); cells[currcell]++; }
//	while(cells[currcell] > c) { add_chr('-'); cells[currcell]--; }

	if(!add_chr('.')) return;
    }

    currcell = clear_tape(currcell);
}

/*******************************************************************************
 * This is a generator that uses one cell as a multipler to get a second to
 * the correct value. It uses a table to remove the need for an explicit
 * search.
 *
 * Note: this doesn't use sqrt() to find the shortest loop because that doesn't
 * actually work despite seeming like it should.
 *
 * It's the classic algorithm but only rarely is it the shortest.
 */

void
gen_twoflower(char * buf)
{
    /* This table gives the shortest strings (using a simple multiplier form)
     * for all the values upto 255. It was created with a simple brute force
     * search.
     */
    char bestfactor[] = {
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

    const int maxcell = 1;
    int currcell=0;
    int i;
    char * p;

    reinit_state();

    /* Clear the working cells */
    if (flg_init) add_str(">[-]<[-]");

    str_cells_used = maxcell+1;

    /* Print each character */
    for(p=buf; *p;) {
	int cdiff,t;
	int a,b,c = *p++;
	if (flg_signed) c = (signed char)c; else c = (unsigned char)c;

	cdiff = c - cells[0];
	t = bestfactor[abs(cdiff)]; a =(t&0xF); t = !!(t&0x10);
	b = abs(cdiff)/a+t;

	if (a>1) {
	    int loopcost = 7+a+b+abs(cdiff)-a*b;
	    int da, db, ddiff;
	    int newcost = 3;

	    ddiff = c;
	    t = bestfactor[abs(ddiff)]; da =(t&0xF); t = !!(t&0x10);
	    db = abs(ddiff)/da+t;
	    if (da>1)
		newcost = 10 + da+db+abs(ddiff)-da*db;
	    else
		newcost = 3+ abs(ddiff);
	    if (newcost < loopcost) {
		a = da;
		b = db;
		cdiff = ddiff;
		if(cells[0]>0) add_str("[-]");
		if(cells[0]<0) add_str("[+]");
		cells[0] = 0;
	    }
	}

	if (a>1) {
	    if (cdiff<0) a= -a;

	    add_chr('>');
	    for(i=0; i<b; i++) add_chr('+');

	    add_str("[<");
	    while(a > 0) { add_chr('+'); a--; cells[0] += b; }
	    while(a < 0) { add_chr('-'); a++; cells[0] -= b; }
	    add_str(">-]<");
	}

	while(cells[0] < c) { add_chr('+'); cells[0]++; }
	while(cells[0] > c) { add_chr('-'); cells[0]--; }

	add_chr('.');
	if (best_len>0 && str_next > best_len) return;	/* Too big already */
    }

    currcell = clear_tape(currcell);

    check_if_best("twocell");
}

struct bfi { int mov; int cmd; int arg; } *pgm = 0;
int pgmlen = 0;

int
runbf(char * prog, int m)
{
    int p= -1, n= -1, j= -1, ch;
    int maxcell = 0, countdown = 10000;
    while((ch = *prog++)) {
	int r = (ch == '<' || ch == '>' || ch == '+' || ch == '-');
	if (r || (ch == ']' && j>=0) || ch == '[' ) {
	    if (ch == '<') {ch = '>'; r = -r;}
	    if (ch == '-') {ch = '+'; r = -r;}
	    if (r && p>=0 && pgm[p].cmd == ch) { pgm[p].arg += r; continue; }
	    if (p>=0 && pgm[p].cmd == '>') { pgm[p].mov = pgm[p].arg; }
	    else {
		n++;
		if (n>= pgmlen-2) pgm = realloc(pgm, (pgmlen=n+99)*sizeof*pgm);
		if (!pgm) { perror("realloc"); exit(1); }
		pgm[n].mov = 0;
	    }
	    pgm[n].cmd = ch; pgm[n].arg = r; p = n;
	    if (pgm[n].cmd == '[') { pgm[n].arg=j; j = n; }
	    else if (pgm[n].cmd == ']') {
		pgm[n].arg = j; j = pgm[j].arg; pgm[pgm[n].arg].arg = n;
	    }
	}
    }
    while(j>=0) { p=j; j=pgm[j].arg; pgm[p].arg=0; pgm[p].cmd = '+'; }
    if (!pgm) return 0;
    pgm[n+1].mov = pgm[n+1].cmd = 0;

    for(n=0; ; n++) {
	m += pgm[n].mov;
	if (m<0 || m>=MAX_CELLS || --countdown == 0) return -1;
	if (m>maxcell) maxcell = m;
	if (bytewrap) cells[m] &= 255;

        switch(pgm[n].cmd)
        {
            case 0:     str_cells_used = maxcell+1;
			return m;
            case '+':   cells[m] += pgm[n].arg; break;
            case '[':   if (cells[m] == 0) n=pgm[n].arg; break;
            case ']':   if (cells[m] != 0) n=pgm[n].arg; break;
            case '>':   m += pgm[n].arg; break;
        }
    }
}
