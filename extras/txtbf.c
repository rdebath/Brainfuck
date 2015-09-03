/*
 *  This program takes a text input and generates BF code to create that text.
 *
 *  This program will search for the shortest piece of BF code it can. If
 *  all the search options are turned on it can take a VERY long time to
 *  run. But it can give very good results. Even in the default mode it
 *  is likely to give significantly better results than other converters.
 *
 *  The program uses several methods to attempt to generate the shortest
 *  result. In the default mode it will try the normal method of changing
 *  a single cell using the best loops it can. This is rarely the best.
 *
 *  Most of the options use a loop of set of loops to setup a collection
 *  of cells to various values then use a simple "closest choice" routine
 *  to pick the "correct" cell.
 *
 *  The 'subrange' method is uses a multiply loop to set the collection of
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

void find_best_conversion(char * linebuf);

void gen_print(char * buf, int init_offset);
void check_if_best(char * buf, char * name);

void gen_subrange(char * buf, int subrange, int flg_zoned, int flg_nonl);
void gen_ascii(char * buf);
void gen_multonly(char * buf);
void gen_multbyte(char * buf);
void gen_nestloop(char * buf);
void gen_slipnest(char * buf);
void gen_special(char * buf, char * initcode, char * name, int failquiet);
void gen_twoflower(char * buf);
void gen_twobyte(char * buf);
void gen_trislipnest(char * buf);

int runbf(char * prog, int longrun);

#define MAX_CELLS 256

/* These setup strings have, on occasion, given good results */

/* This is a prefix for huge ASCII files, it's handmade. */
#define HUGEPREFIX "+++[>+++++++<-]>[>[++++++>]+[<]>-]"

/* Some very generic multiply loops */
#define RUNNERCODE1 "++++++++++++++[>+++++>+++++++>++++++++>++>++++++<<<<<-]"
#define RUNNERCODE2 ">+++++[<++++++>-]<++[>+>++>+++>++++<<<<-]"
#define RUNNERCODE3 ">+++++[<++++++>-]<++[>+>++>+++>++++>+++++>++++++>+++++++<<<<<<<-]"
#define RUNNERCODE4 ">+++++[<++++++>-]<++[>+>++>+++>++++>->-->---<<<<<<<-]"

/* Some Hello World prefixes, so you don't have to do big runs. */
char * hello_world[] = {
    "+++++++++++++++[>++++++++>+++++++>++>+++<<<<-]",
    "+++++++++++++++[>+++++++>++++++++>++>+++<<<<-]",
    "+++++++++++++++[>+++++>+++++>++++++>+++>++<<<<<-]",
    "+++++++++++++++[>+++++>+++++>++++++>++>+++<<<<<-]",
    "++++++++++++++[>+++++>+++++++>++++++>+++<<<<-]",
    "++++++++++++++[>+++++>+++++++>++++++>+++>++<<<<<-]",
    "++++++++++++++[>+++++>+++++++>++++++>++>+++<<<<<-]",
    "++++++++++++++[>+++++>+++++++>+++>++++++<<<<-]",
    "+++++++++++[>++++++>+++++++>++++++++>++++>+++>+<<<<<<-]",
    "+++++++++++[>+>++++++>+++++++>++++++++>++++>+++<<<<<<-]",
    "++++++++++[>+++++++>++++++++++>++++>+<<<<-]",
    "++++++++++[>+++++++>++++++++++>+++>+<<<<-]",
    "++++++++[>+++++++++>++++<<-]",

    ">++++++[<++++>-]<[>+++>++++>+++++>++<<<<-]",
    ">++++++[<++++>-]<[>+++>++++>++>+++++<<<<-]",
    ">++++[<++++>-]<+[>++++>++++++>+++++++>++<<<<-]",
    ">++++[<++++>-]<+[>++++>++++++>+++++>++<<<<-]",
    ">++++[<++++>-]<+[>++++>++++++>++>+++++++<<<<-]",
    ">++++[<++++>-]<+[>++++>++++++>++>+++++<<<<-]",

    0 };

char * hello_world2[] = {
    "+++++++++++[>++++[>++>++>++>+>+<<<<<-]>->+>>-[<]<-]",
    "+++++++++[>++++++[>++>++>++>++>+<<<<<-]>>+>->>-[<]<-]",
    "+++++++++[>++++[>++>+++>+++>+++>+<<<<<-]>>->>+>+[<]<-]",
    "+++++++++[>++++[>++>+++>+++>+>+++<<<<<-]>>->>+>+[<]<-]",
    "++++++++[>++++[>++>+++>+++>+++>+<<<<<-]>+>+>+>-[<]<-]",
    "++++++++[>++++[>++>+++>+++>+<<<<-]>+>+>->>+[<]<-]",
    "++++++++[>++++[>++>+++>+++>+>+<<<<<-]>+>+>->+[<]<-]",
    "+++++++[>+++++[>+++>+++>+++<<<-]>+>->>+[<]<-]",

    "++++++++[>++++[>+++>++>+<<<-]>->+<<<-]",
    "++++++++[>++++[>++>+++>+++>+<<<<-]>+>-<<<-]",
    "++++++++[>++++[>++>++>+++>+<<<<-]>+>+>-<<<<-]",
    "+++[>+++++[>+++++>+++++>++++++>+++>++<<<<<-]>-<<-]",
    "+++[>+++++[>+++++>+++++>++++++>++>+++<<<<<-]>-<<-]",

    ">+>++++>+>+>+++[>[->+++<<++>]<<]",
    ">+>++++>+>+>+++[>[->+++>[->++<]<<<++>]<<]",
    ">+>++>++>+++[>[->+++<<+++>]<<]",
    ">+>++>+>++>+++++[>[->++<<++>]<<]",
    ">+>+>+++++++>++>+++[>[->+++<<+++>]<<]",
    ">+>+>++>+++[>[->+++<<+++>]<<]",
    ">+>+>++>+++[>[->++<<+++>]<<]",
    ">+>+>++>++[>[->++++<<+++>]<<]",
    ">+>+>+>++[>[->++++<<+++>]<<]",
    ">+>+>+>+>++++[>[->+++<<+++>]<<]",

    0 };

    /* Bytewrap strings */
char * hello_world_byte[] = {
    /* Note that the initial sequences are NOT generated by this program */
    "+[->>++>+[<]<-]",		/* 0 */
    ">+[[>]-[---<]>]",		/* 1 */

    "+[+>+>->>-[<-]<]",		/* 2 */
    "+[>+>->>-[<-]<+]",		/* 3 */
    "+[>>-->->-[<]<+]",		/* 4 */
    "+[>>>+++<[-<]<+]",		/* 5 */
    "+[[->]+[--<]>--]",		/* 6 */
    "-[[+>]+[+++<]>+]",		/* 7 */
    "-[[+>]+[+++<]>-]",		/* 8 */
    ">-[-[>]+[+++<]>]",		/* 9 */

    "+[->[+>]-[++<]>+]",	/* 10 */
    "+[[->]+[----<]>-]",	/* 11 */
    "+[[->]+[---<]>--]",	/* 12 */
    "-[+[+<]>>>>->+>+]",	/* 13 */
    ">+[++>++[<]>>->+]",	/* 14 */
    ">+[[+>]+[+++<]>-]",	/* 15 */
    ">+[[--->]+[--<]>]",	/* 16 */
    ">-[-[+>]+[+++<]>]",	/* 17 */
    ">-[[--->]++[<]>-]",	/* 18 */

    "+[------->->->+++<<<]",					/* 19 */
    "++[+++++++>++++>->->->++>-<<<<<<]",			/* 20 */
    "++[+++++++>++++>->->->--->++<<<<<<]",			/* 21 */
    "++++[------->-->--->--->->++++<<<<<]",			/* 22 */
    "++[--------------->-->--->->+++++<<<<]",			/* 23 */
    ">++++[<++++>-]<[++>+++>+++>---->+<<<<]",			/* 24 */
    "++++++[+++++++>->++>++>---->+++>-<<<<<<]",			/* 25 */
    ">+++++[<++++>-]<[++>+++++>+++>---->+<<<<]",		/* 26 */
    "+++++++++++++[+++++++>++>++>----->---->+++<<<<<]",		/* 27 */
    "+++++++++++++[------->-->-->+++++>--->++++<<<<<]",		/* 28 */
    "+++++++++++++[+++++++>->++>++>----->---->+++<<<<<<]",	/* 29 */
    "+++++++++++++[------->+>-->-->+++++>--->++++<<<<<<]",	/* 30 */

    0 };

int flg_binary = 0;
int flg_init = 0;
int flg_clear = 0;
int flg_signed = 1;
int flg_rtz = 0;

int done_config_wipe = 0;

int enable_multloop = 0;
int multloop_maxcell = 5;
int multloop_maxloop = 20;
int multloop_maxinc = 10;
int multloop_no_prefix = 0;

int enable_sliploop = 0;
int sliploop_maxcell = 7;
int sliploop_maxind = 7;

int enable_subrange = 1;
int subrange_count = 0;

int enable_twocell = 1;
int enable_nestloop = 0;
int enable_trislipnest = 0;
int enable_special1 = 1;
int enable_special2 = 1;

int flg_lookahead = 0;
int flg_zoned = 0;

int verbose = 0;
int bytewrap = 0;
int maxcol = 72;
int blocksize = 0;

void reinit_state(void);
void output_str(char * s);
void add_chr(int ch);
void add_str(char * p);

char * str_start = 0;
int str_max = 0, str_next = 0;
int str_cells_used = 0;

char bf_print_buf[64];
int bf_print_off = 0;

char * best_str = 0;
int best_len = -1;
int best_cells = 1;

char * special_init = 0;

static int cells[MAX_CELLS];

void
wipe_config()
{
    if (done_config_wipe) return;
    done_config_wipe = 1;

    enable_multloop = 0;
    enable_nestloop = 0;
    enable_trislipnest = 0;
    enable_sliploop = 0;
    enable_twocell = 0;
    enable_subrange = 0;
    enable_special1 = 0;
    enable_special2 = 0;

    special_init = 0;
}

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
	} else if (strncmp(argv[1], "-w", 2) == 0 && argv[1][2] >= '0' && argv[1][2] <= '9') {
	    maxcol = atol(argv[1]+2);
	    argc--; argv++;
	} else if (strncmp(argv[1], "-B", 2) == 0 && argv[1][2] >= '0' && argv[1][2] <= '9') {
	    blocksize = atol(argv[1]+2);
	    argc--; argv++;

	} else if (strcmp(argv[1], "-binary") == 0) {
	    flg_binary = 1;
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
	} else if (strcmp(argv[1], "-lookahead") == 0) {
	    flg_lookahead = 1;
	    argc--; argv++;
	} else if (strcmp(argv[1], "-zoned") == 0) {
	    flg_zoned = 1;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-max") == 0) {
	    wipe_config();
	    enable_sliploop = 1;
	    sliploop_maxcell = 8;
	    sliploop_maxind = 8;
	    enable_multloop = 1;
	    multloop_maxcell = 7;
	    multloop_maxloop = 32;
	    multloop_maxinc = 12;
	    enable_twocell = 1;
	    enable_subrange = 1;
	    enable_nestloop = 1;
	    enable_trislipnest = 1;
	    enable_special1 = 1;
	    enable_special2 = 1;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-std") == 0) {
	    if (done_config_wipe) {
		fprintf(stderr, "The option -std must be first\n");
		exit(1);
	    }
	    done_config_wipe = 1;
	    argc--; argv++;

	} else if (strncmp(argv[1], "-I", 2) == 0) {
	    wipe_config();
	    special_init = argv[1]+2;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-O") == 0) {
	    wipe_config();
	    enable_multloop = 1;
	    enable_subrange = 1;
	    enable_twocell = 1;
	    enable_special1 = 1;
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
	} else if (strcmp(argv[1], "-rtz") == 0) {
	    flg_rtz = !flg_rtz;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-slip") == 0) {
	    wipe_config();
	    enable_sliploop = 1;
	    sliploop_maxcell = 7;
	    sliploop_maxind = 6;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-slip2") == 0) {
	    wipe_config();
	    enable_sliploop = 1;
	    sliploop_maxcell = 8;
	    sliploop_maxind = 7;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-mult") == 0) {
	    wipe_config();
	    enable_multloop = 1;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-mult2") == 0) {
	    wipe_config();
	    enable_multloop = 1;
	    multloop_maxcell = 7;
	    multloop_maxloop = 32;
	    multloop_maxinc = 12;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-mult-bare") == 0) {
            multloop_no_prefix = 1;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-q") == 0) {
	    wipe_config();
	    enable_twocell = 1;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-X") == 0) {
	    wipe_config();
	    enable_special1 = enable_special2 = 1;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-S") == 0) {
	    wipe_config();
	    enable_subrange = 1;
	    argc--; argv++;

	} else if (strncmp(argv[1], "-s", 2) == 0 && argv[1][2] >= '0' && argv[1][2] <= '9') {
	    wipe_config();
	    subrange_count = atol(argv[1]+2);
	    enable_subrange = 1;
	    argc--; argv++;

	} else if (strncmp(argv[1], "-multcell=", 10) == 0) {
	    wipe_config();
	    enable_multloop = 1;
	    multloop_maxcell = atol(argv[1]+10);
	    argc--; argv++;
	} else if (strncmp(argv[1], "-multloop=", 10) == 0) {
	    wipe_config();
	    enable_multloop = 1;
	    multloop_maxloop = atol(argv[1]+10);
	    argc--; argv++;
	} else if (strncmp(argv[1], "-multincr=", 10) == 0) {
	    wipe_config();
	    enable_multloop = 1;
	    multloop_maxinc = atol(argv[1]+10);
	    argc--; argv++;

	} else if (strcmp(argv[1], "-N") == 0) {
	    wipe_config();
	    enable_nestloop = 1;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-tri") == 0) {
	    wipe_config();
	    enable_nestloop = 1;
	    enable_trislipnest = 1;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-h") == 0) {
	    fprintf(stderr, "txtbf [options] [file]\n");
	    fprintf(stderr, "-v      Verbose\n");
	    fprintf(stderr, "\n");
	    fprintf(stderr, "-std    Leave stuff that's enabled by default on.\n");
	    fprintf(stderr, "-max    Enable everything -- very slow\n");
	    fprintf(stderr, "-I><    Enable one special string.\n");
	    fprintf(stderr, "-O      Enable easy to optimise codes.\n");
	    fprintf(stderr, "-S      Enable subrange multiply method\n");
	    fprintf(stderr, "-X      Enable builtin special strings\n");
	    fprintf(stderr, "\n");
	    fprintf(stderr, "-i      Add/remove cell init strings and return to cell zero\n");
	    fprintf(stderr, "-c      Add/remove cell clear strings and return to cell zero\n");
	    fprintf(stderr, "-rtz    Set/clear Return to cell zero\n");
	    fprintf(stderr, "-sc/-uc Assume chars are signed/unsigned\n");
	    fprintf(stderr, "-b      Assume cells are bytes\n");
	    fprintf(stderr, "\n");
	    fprintf(stderr, "-q      Enable two cells method\n");
	    fprintf(stderr, "\n");
	    fprintf(stderr, "-mult   Search short multiply loops\n");
	    fprintf(stderr, "-mult2  Search long multiply loops\n");
	    fprintf(stderr, "-slip   Search short slip loops\n");
	    fprintf(stderr, "-slip2  Search long slip loops\n");
	    fprintf(stderr, "-N      Search nested loops v.long\n");
	    fprintf(stderr, "-tri    Search trislip loops\n");
	    fprintf(stderr, "-s10    Try just one subrange multiply size\n");
	    fprintf(stderr, "-M+99   Specify multiply loop max increment\n");
	    fprintf(stderr, "-M*99   Specify multiply loop max loops\n");
	    fprintf(stderr, "-M=99   Specify multiply loop max cells\n");
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
    else
    {
	char * linebuf, * p;
	int linebuf_size = 0;
	int c;
	int eatnl = 0;
	p = linebuf = malloc(linebuf_size = 2048);
	if(!linebuf) { perror("malloc"); exit(1); }

	while((c = fgetc(ifd)) != EOF) {
	    if (!flg_binary) {
		if (c == '\r') { eatnl = 1; c = '\n'; }
		else if (c == '\n' && eatnl) { eatnl = 0; continue; }
		else eatnl = 0;
	    }
	    if (c == 0) continue;

	    *p++ = c;
	    if (p-linebuf >= linebuf_size-1) {
		size_t off = p-linebuf;
		linebuf = realloc(linebuf, linebuf_size += 2048);
		if(!linebuf) { perror("realloc"); exit(1); }
		p = linebuf+off;
	    }

	    if (p-linebuf == blocksize) {
		int t = flg_clear;
		*p++ = 0;
		if (!flg_init) flg_clear = 1;
		find_best_conversion(linebuf);
		p = linebuf;
		flg_clear = t;
	    }
	}
	*p++ = 0;
	if (ifd != stdin) fclose(ifd);

	find_best_conversion(linebuf);
    }

    return 0;
}

void
find_best_conversion(char * linebuf)
{
    best_len = -1;	/* Clear 'best' string */

    if (special_init != 0)
	gen_special(linebuf, special_init, "cmd special", 0);

    if (enable_special1) {
	char ** hellos;
	char namebuf[64];

	if (verbose>2) fprintf(stderr, "Trying special strings\n");

	gen_special(linebuf, "", "Bare cells", 0);

	gen_special(linebuf, RUNNERCODE1, "mult English", 0);
	gen_special(linebuf, RUNNERCODE2, "mult*32 to 128", 0);
	gen_special(linebuf, RUNNERCODE3, "mult*32 to 224", 0);
	gen_special(linebuf, RUNNERCODE4, "mult*32 to 128/-96", 0);

	for (hellos = hello_world; *hellos; hellos++) {
	    sprintf(namebuf, "Hello world %d", hellos-hello_world);
	    gen_special(linebuf, *hellos, namebuf, 0);
	}
    }

    if (enable_special2) {
	char ** hellos;
	char namebuf[64];

	if (verbose>2) fprintf(stderr, "Trying complicated special strings\n");

	gen_special(linebuf, HUGEPREFIX, "big ASCII", 0);

	for (hellos = hello_world2; *hellos; hellos++) {
	    sprintf(namebuf, "Complex world %d", hellos-hello_world2);
	    gen_special(linebuf, *hellos, namebuf, 0);
	}

	if (bytewrap) for (hellos = hello_world_byte; *hellos; hellos++) {
	    sprintf(namebuf, "Hello bytes %d", hellos-hello_world_byte);
	    gen_special(linebuf, *hellos, namebuf, 0);
	}
    }

    if (enable_twocell) {
	if (verbose>2) fprintf(stderr, "Trying two cell routine.\n");
	gen_twoflower(linebuf);
	if (bytewrap) {
	    if (verbose>2) fprintf(stderr, "Trying byte two cell routine.\n");
	    gen_twobyte(linebuf);
	}
    }

    if (subrange_count > 0) {
	reinit_state();
	gen_subrange(linebuf,subrange_count,1,0);
    } else if (subrange_count == 0) {

	if (enable_subrange) {
	    if (verbose>2) fprintf(stderr, "Trying subrange routines.\n");
	    subrange_count=10;
	    reinit_state();
	    gen_subrange(linebuf,subrange_count,1,0);

	    for(subrange_count = 2; subrange_count<33; subrange_count++)
		if (subrange_count!=10) {
		    reinit_state();
		    gen_subrange(linebuf,subrange_count,1,0);
		}

	    if (!flg_zoned) {
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

	subrange_count = 0;
    }

    if (enable_trislipnest) {
	if (verbose>2) fprintf(stderr, "Trying tri-slip routine.\n");
	gen_trislipnest(linebuf);
    }

    if (enable_multloop) {
	if (verbose>2) fprintf(stderr, "Trying multiply loops.\n");
	gen_multonly(linebuf);
    }

    if (enable_multloop && bytewrap) {
	if (verbose>2) fprintf(stderr, "Trying byte multiply loops.\n");
	gen_multbyte(linebuf);
    }

    if (enable_sliploop) {
	if (verbose>2) fprintf(stderr, "Trying 'slipping loop' routine.\n");
	gen_slipnest(linebuf);
    }

    if(enable_nestloop) {
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
    output_str("\n");
}

void
output_str(char * s)
{
static int col = 0;
    int ch;

    while (*s)
    {
	ch = *s++;
	if ((col>=maxcol && maxcol) || ch == '\n') {
	    putchar('\n');
	    col = 0;
	    if (ch == ' ' || ch == '\n') ch = 0;
	}
	if (ch) { putchar(ch & 0xFF); col++; }
    }
}

void
check_if_best(char * buf, char * name)
{
    if (best_len == -1 || best_len > str_next || (best_len == str_next && str_cells_used < best_cells)) {

	memset(cells, 0, sizeof(cells));
	runbf(str_start, 1);
	if (strncmp(buf, bf_print_buf, sizeof(bf_print_buf)) != 0) {
	    fprintf(stderr, "ERROR: Consistancy check for generated BF code FAILED.\n");
	    fprintf(stderr, "Code: '%s'\n", str_start);
	    fprintf(stderr, "Problem: \"%s\"\n", buf);
	    exit(99);
	}

	if (best_str) free(best_str);
	best_str = malloc(str_next+1);
	memcpy(best_str, str_start, str_next+1);
	best_len = str_next;
	best_cells = str_cells_used;
    }

    if (best_len == str_next && str_cells_used == best_cells) {
	/* Note this also shows strings of same length */
	if (verbose>2 || str_next < 63)
	    fprintf(stderr, "Found '%s' len=%d, cells=%d, '%s'\n",
		    name, str_next, str_cells_used, str_start);
	else if (verbose>1)
	    fprintf(stderr, "Found '%s' len=%d, cells=%d, '%.60s'...\n",
		    name, str_next, str_cells_used, str_start);
    }
}

inline void
add_chr(int ch)
{
    int lastch = 0;
    if (ch == ' ' || ch == '\n') return;

    while (str_next+2 > str_max) {
	str_start = realloc(str_start, str_max += 1024);
	if(!str_start) { perror("realloc"); exit(1); }
    }
    if (str_next > 0) lastch = str_start[str_next-1];

    if ( (ch == '>' && lastch == '<') || (ch == '<' && lastch == '>') ) {
	str_start[--str_next] = 0;
	return ;
    }

    str_start[str_next++] = ch;
    str_start[str_next] = 0;

    return ;
}

inline void
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
gen_subrange(char * buf, int subrange, int flg_subzone, int flg_nonl)
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
	    flg_subzone?"zoned":"unzoned",flg_nonl?", nonl":"");

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
	if (subrange>15 && !multloop_no_prefix) {
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
    if (!flg_subzone) {
	gen_print(buf, 0);
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

    check_if_best(buf, name);
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
	    if (toohigh || maxcell == 0) goto return_to_top;
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
	} else if (cellincs[0]>15 && !multloop_no_prefix) {
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

	gen_print(buf, 0);

	check_if_best(buf, "multiply loop");
    }
}

/******************************************************************************/
/*
   This does a brute force search through all the simple multiplier
   loops upto a certain size for BYTE cells only.

   The text is then generated using a closest next function.
 */
void
gen_multbyte(char * buf)
{
    int cellincs[10] = {0};
    int cellincs2[10] = {0};
    int maxcell = 0;
    int currcell=0;
    int i, cnt, hasneg;

static int bestloopset[256];
static int bestloopinit[256];
static int bestloopstep[256];
static int loopcnt[256][256];

    for(;;)
    {
	/* Loop through all possible patterns */
	for(i=0; ; i++) {
	    if (i >= multloop_maxcell+1) return;
	    cellincs[i]++;
	    if (i < 2) {
		if (cellincs[i] <= multloop_maxloop) break;
	    } else {
		if (cellincs[i] <= multloop_maxinc) break;
	    }
	    cellincs[i] = 1;
	}

	for(maxcell=0; cellincs[maxcell] != 0; )
	    maxcell++;

	/* I need the basic structure of a loop */
	if (maxcell < 3) continue;

	maxcell--; currcell=0;

	cellincs2[0] = cellincs[0];
	hasneg = 0;
	for(i=maxcell; i>0; i--) {
	    cellincs2[i] = ((cellincs[i]&1?1:-1) * ((cellincs[i]+1)/2));
	    if (i>1 && cellincs2[i] < 0)
		hasneg = 1;
	}
	hasneg |= (cellincs2[0]<0 || cellincs2[1] != -1);
	if (!hasneg) continue;

	/* How many times will this go round. */
	if ((cnt = loopcnt[cellincs2[0] & 0xFF][cellincs2[1] & 0xFF]) == 0)
	{
	    int sum = cellincs2[0];
	    cnt = 0;
	    while (sum && cnt < 256) {
		cnt++;
		sum = ((sum + (cellincs2[1] & 0xFF)) & 0xFF);
	    }
	    loopcnt[cellincs2[0] & 0xFF][cellincs2[1] & 0xFF] = cnt;
	}
	if (cnt >= 256) continue;

	/* Remeber the shortest pairing that will get us a specific loop count. */
	if (bestloopset[cnt] != 0 ) {
	    if (cellincs2[0] != bestloopinit[cnt] || cellincs2[1] != bestloopstep[cnt]) {
		if (cellincs2[1]+cellincs2[0] >= bestloopinit[cnt]+bestloopstep[cnt])
		    continue;
	    } else {
		if (cellincs2[1]+cellincs2[0] > bestloopinit[cnt]+bestloopstep[cnt])
		    continue;
	    }
	}
	bestloopset[cnt] = 1;
	bestloopinit[cnt] = cellincs2[0];
	bestloopstep[cnt] = cellincs2[1];

	reinit_state();
	/* Clear the working cells */
	if (flg_init) {
	    for(i=maxcell; i>=0; i--) {
		    while(currcell < i) { add_chr('>'); currcell++; }
		    while(currcell > i) { add_chr('<'); currcell--; }
		    add_str("[-]");
		    cells[i] = 0;
	    }
	}

	str_cells_used = maxcell+1;

	if (cellincs2[0] == 127 && bytewrap) {
	    add_str(">++[<+>++]<");
	} else if (cellincs2[0]>15 && !multloop_no_prefix) {
	    char best_factor[] = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
		4, 4, 6, 6, 5, 7, 7, 7, 6, 5, 5, 9, 7, 7, 6, 6,
		8, 8, 8, 7, 6, 6, 6, 6, 8, 8, 7, 7,11, 9, 9, 9};

	    int l,r;
	    int sr = cellincs2[0];
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
	    for(i=0; i<cellincs2[0]; i++) add_chr('+');
	    for(i=0; i<-cellincs2[0]; i++) add_chr('-');
	}

	if (maxcell == 1 && cellincs2[1] == 1) {
	    /* Copy loop */
	    ;
	} else {
	    int j;
	    add_chr('[');

	    for(i=1; i<=maxcell; i++) {
		for(j=0; j<cellincs2[i]; j++) add_chr('+');
		for(j=0; j<-cellincs2[i]; j++) add_chr('-');
		add_chr('>');
	    }

	    for(i=1; i<=maxcell; i++)
		add_chr('<');

	    add_chr(']');
	}

	if (best_len>0 && str_next > best_len) continue;	/* Too big already */

	if (verbose>3)
	    fprintf(stderr, "Trying byte multiply: %s\n", str_start);

	cells[0] = 0;
	for(i=2; i<=maxcell; i++) {
	    cells[i-1] = (0xFF & ( cellincs2[i] * cnt )) ;
	}

	gen_print(buf, 0);

	check_if_best(buf, "byte multiply loop");
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

	gen_print(buf, 0);

	check_if_best(buf, "nested loop");
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
	add_str("[>[->");
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

	gen_print(buf, 0);

	check_if_best(buf, "slipping loop");
    }
}

/******************************************************************************/
/*
   This is a simple 'Special case' text generator.
   Given a piece of BF code to fill in some cells it then uses those cells to
   print the text you give it.

   For computation purposes any '.' and ',' commands are ignored.

   An empty string is a single cell set to zero, '>' is two cells set to zero.

   The cells are used in closest next order.
 */

void
gen_special(char * buf, char * initcode, char * name, int failquiet)
{
    int maxcell = 0;
    int currcell=0;
    int remaining_offset = 0;
    int i;

    reinit_state();

    if (verbose>3)
	fprintf(stderr, "Running '%s' code: %s\n", name, initcode);

    /* Work out what the 'special' code does to the cell array ... How?
     *
     * By running it of course!
     * Use a local interpreter so we can increase the counter and give a better
     * error message.
     */
    if(0) {
	remaining_offset = runbf(initcode, 0);
	if (remaining_offset<0) {
	    fprintf(stderr, "Code '%s' failed '%s'\n", name, initcode);
	    return;
	}
    } else {
	char *codeptr, *b;
	int m=0, nestlvl=0;
	int countdown = 100000;
	if (!failquiet) countdown = 1000000;

	maxcell = -1;
	str_cells_used = maxcell+1;

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
	    if (m>maxcell) { maxcell = m; str_cells_used = maxcell+1; }
	    if (bytewrap) cells[m] &= 255;
	}

	/* Something went wrong; the code is invalid */
	if (*codeptr) {
	    if (!failquiet || verbose>2)
		fprintf(stderr, "Code '%s' failed to run cellptr=%d countdown=%d iptr=%d '%s'\n",
			name, m, countdown, (int)(codeptr-initcode), initcode);
	    return;
	}
	remaining_offset = m;
    }

    if (flg_init) {
	/* Clear the working cells */
	if (maxcell<0) maxcell=0;
	for(i=maxcell; i>=0; i--) {
	    while(currcell < i) { add_chr('>'); currcell++; }
	    while(currcell > i) { add_chr('<'); currcell--; }
	    add_str("[-]");
	}
    }

    add_str(initcode);

    if (best_len>0 && str_next > best_len) return;	/* Too big already */

    gen_print(buf, remaining_offset);

    check_if_best(buf, name);
}

/*******************************************************************************
 * Given a set of cells and a string to generate them already in the buffer
 * this routine uses those cells to make the string.
 *
 * It adds up the number of '<>' and '+-' needed to get each cell to the next
 * value then chooses the lowest. This is only optimal with repeat counts
 * below about 10 and even then for multiple characters it may be better
 * to accept a poor early choice for better results later.
 */
void
gen_unzoned(char * buf, int init_offset)
{
    char * p;
    int i;
    int currcell = init_offset;
    int more_cells = 0;
    if (str_cells_used <= 0) { more_cells=1; str_cells_used=1; }

    /* Print each character, use closest cell. */
    for(p=buf; *p;) {
	int minrange = 999999;
	int c = *p++;
	int usecell = 0, diff;

	if (flg_signed) c = (signed char)c; else c = (unsigned char)c;

	for(i=0; i<str_cells_used+more_cells; i++) {
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

	if (currcell>=str_cells_used) {
	    str_cells_used++;
	    if (flg_init) add_str("[-]");
	}

	diff = c - cells[currcell];
	if (bytewrap) diff = (signed char)diff;
	if (c>=0 && diff<0 && c+3<-diff) { add_str("[-]"); cells[currcell]=0; diff=c; }
	while(diff>0) { add_chr('+'); cells[currcell]++; diff--; }
	while(diff<0) { add_chr('-'); cells[currcell]--; diff++; }
	if (bytewrap) cells[currcell] &= 255;

	add_chr('.');

	if (best_len>0 && str_next > best_len) return;	/* Too big already */
    }

    currcell = clear_tape(currcell);
}

/*******************************************************************************
 * This is like the normal unzoned except for each choice at this time it
 * does a lookahead to see if this single 'wrong' choice gives a better
 * result at the end.
 */
void
gen_lookahead(char * buf, int init_cell)
{
    char * p;
    int i;
    int currcell = init_cell;
    int more_cells = 0;
    if (str_cells_used <= 0) { more_cells=1; str_cells_used=1; }

    /* Print each character, use closest cell. */
    for(p=buf; *p;) {
	int minrange = 999999;
	int c = *p++;
	int usecell = 0, diff;

	if (flg_signed) c = (signed char)c; else c = (unsigned char)c;

	for(i=0; i<str_cells_used+more_cells; i++) {
	    int range = c - cells[i];
	    if (bytewrap) range = (signed char)range;
	    range = abs(range);
	    range += abs(currcell-i);

	    {
		int currcell2 = i;
		int la, j;
		int cells2[MAX_CELLS];
		for(j=0; j<str_cells_used; j++)
		    cells2[j] = cells[j];
		cells2[i] = c;

		for(la=0; p[la]; la++)
		{
		    int minrange2 = 999999;
		    int usecell2 = 0;
		    int c2 = p[la];
		    if (flg_signed) c2 = (signed char)c2; else c2 = (unsigned char)c2;

		    for(j=0; j<str_cells_used; j++) {
			int range2 = c2 - cells2[j];
			if (bytewrap) range2 = (signed char)range2;
			range2 = abs(range2);
			range2 += abs(currcell2-j);
			if (range2 == minrange2) {
			    if (abs(currcell2-j) < abs(usecell2-j)) {
				usecell2 = j;
				minrange2 = range2;
			    }
			} else
			if (range2 < minrange2) {
			    minrange2 = range2;
			    usecell2 = j;
			}
		    }
		    range += minrange2;
		    currcell2 = usecell2;
		    cells2[usecell2] = c2;
		}
	    }

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

	if (currcell>=str_cells_used) {
	    str_cells_used++;
	    if (flg_init) add_str("[-]");
	}

	diff = c - cells[currcell];
	if (bytewrap) diff = (signed char)diff;
	while(diff>0) { add_chr('+'); cells[currcell]++; diff--; }
	while(diff<0) { add_chr('-'); cells[currcell]--; diff++; }
	if (bytewrap) cells[currcell] &= 255;

	add_chr('.');

	if (best_len>0 && str_next > best_len) return;	/* Too big already */
    }

    currcell = clear_tape(currcell);
}

/*******************************************************************************
 * Given a set of cells and a string to generate them already in the buffer
 * this routine uses those cells to make the string.
 *
 * This method uses the cell that was closest when at the start of the run.
 * The result is that the character set is divided into zones and each is
 * assigned to a cell.
 */
void
gen_zoned(char * buf, int init_cell)
{
    char * p;
    int i, st = -1;
    int currcell = init_cell;
    int cells2[MAX_CELLS];

    if (str_cells_used <= 0) str_cells_used=1;

    for(i=0; i<str_cells_used; i++) {
	cells2[i] = cells[i];
	if (cells[i] && st<0) st = i;
    }
    if (st<=0) st = 0; else st--;

    /* Print each character */
    for(p=buf; *p;) {
	int minrange = 999999;
	int c = *p++;
	int usecell = 0, diff;

	if (flg_signed) c = (signed char)c; else c = (unsigned char)c;

	for(i=st; i<str_cells_used; i++) {
	    int range = c - cells2[i];
	    if (bytewrap) range = (signed char)range;
	    range = abs(range);

	    if (range < minrange) {
		usecell = i;
		minrange = range;
	    }
	}

	while(currcell > usecell) { add_chr('<'); currcell--; }
	while(currcell < usecell) { add_chr('>'); currcell++; }

	diff = c - cells[currcell];
	if (bytewrap) diff = (signed char)diff;
	if (c>=0 && diff<0 && c+3<-diff) { add_str("[-]"); cells[currcell]=0; diff=c; }
	while(diff>0) { add_chr('+'); cells[currcell]++; diff--; }
	while(diff<0) { add_chr('-'); cells[currcell]--; diff++; }
	if (bytewrap) cells[currcell] &= 255;

	add_chr('.');

	if (best_len>0 && str_next > best_len) return;	/* Too big already */
    }

    currcell = clear_tape(currcell);
}

void
gen_print(char * buf, int init_offset)
{
    if (flg_zoned)
	gen_zoned(buf, init_offset);
    else if (flg_lookahead)
	gen_lookahead(buf, init_offset);
    else
	gen_unzoned(buf, init_offset);
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

    check_if_best(buf, "twocell");
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
gen_twobyte(char * buf)
{
    /* This table gives the shortest strings (using a byte multiplier form)
     * for all the values upto 255. It was created with a simple brute force
     * search.
     */

static char * convbyte[] = {
    "", /* (0, 1) non-wrapping */
    "+", /* (1, 1) non-wrapping */
    "++", /* (2, 1) non-wrapping */
    "+++", /* (3, 1) non-wrapping */
    "++++", /* (4, 1) non-wrapping */
    "+++++", /* (5, 1) non-wrapping */
    "++++++", /* (6, 1) non-wrapping */
    "+++++++", /* (7, 1) non-wrapping */
    "++++++++", /* (8, 1) non-wrapping */
    "+++++++++", /* (9, 1) non-wrapping */
    "++++++++++", /* (10, 1) non-wrapping */
    "+++++++++++", /* (11, 1) non-wrapping */
    "++++++++++++", /* (12, 1) non-wrapping */
    "+++++++++++++", /* (13, 1) non-wrapping */
    "++++++++++++++", /* (14, 1) non-wrapping */
    "+++++++++++++++", /* (15, 1) non-wrapping */
    ">++++[<++++>-]<", /* (15, 2) non-wrapping */
    ">++++[<++++>-]<+", /* (16, 2) non-wrapping */
    ">+++[<++++++>-]<", /* (16, 2) non-wrapping */
    ">+++[<++++++>-]<+", /* (17, 2) non-wrapping */
    ">++++[<+++++>-]<", /* (16, 2) non-wrapping */
    ">+++[<+++++++>-]<", /* (17, 2) non-wrapping */
    ">+++[<+++++++>-]<+", /* (18, 2) non-wrapping */
    ">++++[<++++++>-]<-", /* (18, 2) non-wrapping */
    ">++++[<++++++>-]<", /* (17, 2) non-wrapping */
    ">+++++[<+++++>-]<", /* (17, 2) non-wrapping */
    ">+++++[<+++++>-]<+", /* (18, 2) non-wrapping */
    ">+++[<+++++++++>-]<", /* (19, 2) non-wrapping */
    ">++++[<+++++++>-]<", /* (18, 2) non-wrapping */
    ">++++[<+++++++>-]<+", /* (19, 2) non-wrapping */
    ">+++++[<++++++>-]<", /* (18, 2) non-wrapping */
    ">+++++[<++++++>-]<+", /* (19, 2) non-wrapping */
    ">++++[<++++++++>-]<", /* (19, 2) non-wrapping */
    ">++++[<++++++++>-]<+", /* (20, 2) non-wrapping */
    ">--[<-->+++++++]<--", /* (19, 2) wrapping */
    ">--[<-->+++++++]<-", /* (18, 2) wrapping */
    ">--[<-->+++++++]<", /* (17, 2) wrapping */
    ">---[<+>+++++++]<", /* (17, 2) wrapping */
    ">---[<+>+++++++]<+", /* (18, 2) wrapping */
    ">---[<+>+++++++]<++", /* (19, 2) wrapping */
    ">--[<+>++++++]<---", /* (18, 2) wrapping */
    ">--[<+>++++++]<--", /* (17, 2) wrapping */
    ">--[<+>++++++]<-", /* (16, 2) wrapping */
    ">--[<+>++++++]<", /* (15, 2) wrapping */
    ">--[<+>++++++]<+", /* (16, 2) wrapping */
    ">--[<+>++++++]<++", /* (17, 2) wrapping */
    ">--[<+>++++++]<+++", /* (18, 2) wrapping */
    ">-[<+>-----]<----", /* (17, 2) wrapping */
    ">-[<+>-----]<---", /* (16, 2) wrapping */
    ">-[<+>-----]<--", /* (15, 2) wrapping */
    ">-[<+>-----]<-", /* (14, 2) wrapping */
    ">-[<+>-----]<", /* (13, 2) wrapping */
    ">-[<+>-----]<+", /* (14, 2) wrapping */
    ">-[<+>-----]<++", /* (15, 2) wrapping */
    ">-[<+>-----]<+++", /* (16, 2) wrapping */
    ">-[<+>-----]<++++", /* (17, 2) wrapping */
    ">-[<+>-----]<+++++", /* (18, 2) wrapping */
    ">-[<+>+++++++++]<", /* (17, 2) wrapping */
    ">-[<+>+++++++++]<+", /* (18, 2) wrapping */
    ">----[<+>----]<----", /* (19, 2) wrapping */
    ">----[<+>----]<---", /* (18, 2) wrapping */
    ">----[<+>----]<--", /* (17, 2) wrapping */
    ">----[<+>----]<-", /* (16, 2) wrapping */
    ">----[<+>----]<", /* (15, 2) wrapping */
    ">----[<+>----]<+", /* (16, 2) wrapping */
    ">----[<+>----]<++", /* (17, 2) wrapping */
    ">----[<+>----]<+++", /* (18, 2) wrapping */
    ">----[<--->----]<", /* (17, 2) wrapping */
    ">----[<--->----]<+", /* (18, 2) wrapping */
    ">----[<--->----]<++", /* (19, 2) wrapping */
    ">-[<+>-------]<---", /* (18, 2) wrapping */
    ">-[<+>-------]<--", /* (17, 2) wrapping */
    ">-[<+>-------]<-", /* (16, 2) wrapping */
    ">-[<+>-------]<", /* (15, 2) wrapping */
    ">-[<+>-------]<+", /* (16, 2) wrapping */
    ">-[<+>-------]<++", /* (17, 2) wrapping */
    ">-[<+>-------]<+++", /* (18, 2) wrapping */
    ">-[<+>---]<--------", /* (19, 2) wrapping */
    ">-[<+>---]<-------", /* (18, 2) wrapping */
    ">-[<+>---]<------", /* (17, 2) wrapping */
    ">-[<+>---]<-----", /* (16, 2) wrapping */
    ">-[<+>---]<----", /* (15, 2) wrapping */
    ">-[<+>---]<---", /* (14, 2) wrapping */
    ">-[<+>---]<--", /* (13, 2) wrapping */
    ">-[<+>---]<-", /* (12, 2) wrapping */
    ">-[<+>---]<", /* (11, 2) wrapping */
    ">-[<+>---]<+", /* (12, 2) wrapping */
    ">-[<+>---]<++", /* (13, 2) wrapping */
    ">-[<+>---]<+++", /* (14, 2) wrapping */
    ">-[<+>---]<++++", /* (15, 2) wrapping */
    ">-[<+>---]<+++++", /* (16, 2) wrapping */
    ">-[<+>---]<++++++", /* (17, 2) wrapping */
    ">-[<+>---]<+++++++", /* (18, 2) wrapping */
    ">-[<+>---]<++++++++", /* (19, 2) wrapping */
    ">-[<+>---]<+++++++++", /* (20, 2) wrapping */
    ">-[<++>-----]<-------", /* (21, 2) wrapping */
    ">-[<++>-----]<------", /* (20, 2) wrapping */
    ">-[<++>-----]<-----", /* (19, 2) wrapping */
    ">-[<++>-----]<----", /* (18, 2) wrapping */
    ">-[<++>-----]<---", /* (17, 2) wrapping */
    ">-[<++>-----]<--", /* (16, 2) wrapping */
    ">-[<++>-----]<-", /* (15, 2) wrapping */
    ">-[<++>-----]<", /* (14, 2) wrapping */
    ">-[<++>-----]<+", /* (15, 2) wrapping */
    ">-[<++>-----]<++", /* (16, 2) wrapping */
    ">-[<++>-----]<+++", /* (17, 2) wrapping */
    ">-[<++>-----]<++++", /* (18, 2) wrapping */
    ">-[<++>-----]<+++++", /* (19, 2) wrapping */
    ">-[<-->-------]<--", /* (18, 2) wrapping */
    ">-[<-->-------]<-", /* (17, 2) wrapping */
    ">-[<-->-------]<", /* (16, 2) wrapping */
    ">-[<-->-------]<+", /* (17, 2) wrapping */
    ">-[<-->-------]<++", /* (18, 2) wrapping */
    ">-[<++>+++++++++]<-", /* (19, 2) wrapping */
    ">-[<++>+++++++++]<", /* (18, 2) wrapping */
    ">-[<++>+++++++++]<+", /* (19, 2) wrapping */
    ">--------[<+++>--]<", /* (19, 2) wrapping */
    ">----[<+++++>--]<-", /* (18, 2) wrapping */
    ">----[<+++++>--]<", /* (17, 2) wrapping */
    ">------[<+++>--]<", /* (17, 2) wrapping */
    ">----[<+++>--]<--", /* (17, 2) wrapping */
    ">----[<+++>--]<-", /* (16, 2) wrapping */
    ">----[<+++>--]<", /* (15, 2) wrapping */
    ">--[<+>--]<----", /* (15, 2) wrapping */
    ">--[<+>--]<---", /* (14, 2) wrapping */
    ">--[<+>--]<--", /* (13, 2) wrapping */
    ">--[<+>--]<-", /* (12, 2) wrapping */
    ">--[<+>--]<", /* (11, 2) wrapping */
    ">--[<->--]<-", /* (12, 2) wrapping */
    ">--[<->--]<", /* (11, 2) wrapping */
    ">--[<->--]<+", /* (12, 2) wrapping */
    ">--[<->--]<++", /* (13, 2) wrapping */
    ">--[<->--]<+++", /* (14, 2) wrapping */
    ">--[<->--]<++++", /* (15, 2) wrapping */
    ">----[<--->--]<", /* (15, 2) wrapping */
    ">----[<--->--]<+", /* (16, 2) wrapping */
    ">----[<--->--]<++", /* (17, 2) wrapping */
    ">------[<--->--]<", /* (17, 2) wrapping */
    ">----[<----->--]<", /* (17, 2) wrapping */
    ">----[<----->--]<+", /* (18, 2) wrapping */
    ">--------[<--->--]<", /* (19, 2) wrapping */
    ">-[<-->+++++++++]<-", /* (19, 2) wrapping */
    ">-[<-->+++++++++]<", /* (18, 2) wrapping */
    ">-[<-->+++++++++]<+", /* (19, 2) wrapping */
    ">-[<++>-------]<--", /* (18, 2) wrapping */
    ">-[<++>-------]<-", /* (17, 2) wrapping */
    ">-[<++>-------]<", /* (16, 2) wrapping */
    ">-[<++>-------]<+", /* (17, 2) wrapping */
    ">-[<++>-------]<++", /* (18, 2) wrapping */
    ">-[<-->-----]<-----", /* (19, 2) wrapping */
    ">-[<-->-----]<----", /* (18, 2) wrapping */
    ">-[<-->-----]<---", /* (17, 2) wrapping */
    ">-[<-->-----]<--", /* (16, 2) wrapping */
    ">-[<-->-----]<-", /* (15, 2) wrapping */
    ">-[<-->-----]<", /* (14, 2) wrapping */
    ">-[<-->-----]<+", /* (15, 2) wrapping */
    ">-[<-->-----]<++", /* (16, 2) wrapping */
    ">-[<-->-----]<+++", /* (17, 2) wrapping */
    ">-[<-->-----]<++++", /* (18, 2) wrapping */
    ">-[<-->-----]<+++++", /* (19, 2) wrapping */
    ">-[<-->-----]<++++++", /* (20, 2) wrapping */
    ">-[<-->-----]<+++++++", /* (21, 2) wrapping */
    ">-[<->---]<---------", /* (20, 2) wrapping */
    ">-[<->---]<--------", /* (19, 2) wrapping */
    ">-[<->---]<-------", /* (18, 2) wrapping */
    ">-[<->---]<------", /* (17, 2) wrapping */
    ">-[<->---]<-----", /* (16, 2) wrapping */
    ">-[<->---]<----", /* (15, 2) wrapping */
    ">-[<->---]<---", /* (14, 2) wrapping */
    ">-[<->---]<--", /* (13, 2) wrapping */
    ">-[<->---]<-", /* (12, 2) wrapping */
    ">-[<->---]<", /* (11, 2) wrapping */
    ">-[<->---]<+", /* (12, 2) wrapping */
    ">-[<->---]<++", /* (13, 2) wrapping */
    ">-[<->---]<+++", /* (14, 2) wrapping */
    ">-[<->---]<++++", /* (15, 2) wrapping */
    ">-[<->---]<+++++", /* (16, 2) wrapping */
    ">-[<->---]<++++++", /* (17, 2) wrapping */
    ">-[<->---]<+++++++", /* (18, 2) wrapping */
    ">-[<->---]<++++++++", /* (19, 2) wrapping */
    ">-[<->-------]<---", /* (18, 2) wrapping */
    ">-[<->-------]<--", /* (17, 2) wrapping */
    ">-[<->-------]<-", /* (16, 2) wrapping */
    ">-[<->-------]<", /* (15, 2) wrapping */
    ">-[<->-------]<+", /* (16, 2) wrapping */
    ">-[<->-------]<++", /* (17, 2) wrapping */
    ">-[<->-------]<+++", /* (18, 2) wrapping */
    ">----[<+++>----]<--", /* (19, 2) wrapping */
    ">----[<+++>----]<-", /* (18, 2) wrapping */
    ">----[<+++>----]<", /* (17, 2) wrapping */
    ">----[<->----]<---", /* (18, 2) wrapping */
    ">----[<->----]<--", /* (17, 2) wrapping */
    ">----[<->----]<-", /* (16, 2) wrapping */
    ">----[<->----]<", /* (15, 2) wrapping */
    ">----[<->----]<+", /* (16, 2) wrapping */
    ">----[<->----]<++", /* (17, 2) wrapping */
    ">----[<->----]<+++", /* (18, 2) wrapping */
    ">----[<->----]<++++", /* (19, 2) wrapping */
    ">-[<->+++++++++]<-", /* (18, 2) wrapping */
    ">-[<->+++++++++]<", /* (17, 2) wrapping */
    ">-[<->-----]<-----", /* (18, 2) wrapping */
    ">-[<->-----]<----", /* (17, 2) wrapping */
    ">-[<->-----]<---", /* (16, 2) wrapping */
    ">-[<->-----]<--", /* (15, 2) wrapping */
    ">-[<->-----]<-", /* (14, 2) wrapping */
    ">-[<->-----]<", /* (13, 2) wrapping */
    ">-[<->-----]<+", /* (14, 2) wrapping */
    ">-[<->-----]<++", /* (15, 2) wrapping */
    ">-[<->-----]<+++", /* (16, 2) wrapping */
    ">-[<->-----]<++++", /* (17, 2) wrapping */
    ">--[<->++++++]<---", /* (18, 2) wrapping */
    ">--[<->++++++]<--", /* (17, 2) wrapping */
    ">--[<->++++++]<-", /* (16, 2) wrapping */
    ">--[<->++++++]<", /* (15, 2) wrapping */
    ">--[<->++++++]<+", /* (16, 2) wrapping */
    ">--[<->++++++]<++", /* (17, 2) wrapping */
    ">--[<->++++++]<+++", /* (18, 2) wrapping */
    ">---[<->+++++++]<--", /* (19, 2) wrapping */
    ">---[<->+++++++]<-", /* (18, 2) wrapping */
    ">---[<->+++++++]<", /* (17, 2) wrapping */
    ">--[<++>+++++++]<", /* (17, 2) wrapping */
    ">--[<++>+++++++]<+", /* (18, 2) wrapping */
    ">--[<++>+++++++]<++", /* (19, 2) wrapping */
    ">----[<-------->+]<-", /* (20, 2) wrapping */
    ">----[<-------->+]<", /* (19, 2) wrapping */
    ">-----[<------>+]<-", /* (19, 2) wrapping */
    ">-----[<------>+]<", /* (18, 2) wrapping */
    ">----[<------->+]<-", /* (19, 2) wrapping */
    ">----[<------->+]<", /* (18, 2) wrapping */
    ">---[<--------->+]<", /* (19, 2) wrapping */
    ">-----[<----->+]<-", /* (18, 2) wrapping */
    ">-----[<----->+]<", /* (17, 2) wrapping */
    ">----[<------>+]<", /* (17, 2) wrapping */
    ">----[<------>+]<+", /* (18, 2) wrapping */
    ">---[<------->+]<-", /* (18, 2) wrapping */
    ">---[<------->+]<", /* (17, 2) wrapping */
    ">----[<----->+]<", /* (16, 2) wrapping */
    ">---[<------>+]<-", /* (17, 2) wrapping */
    ">---[<------>+]<", /* (16, 2) wrapping */
    ">----[<---->+]<-", /* (16, 2) wrapping */
    ">----[<---->+]<", /* (15, 2) wrapping */
    "---------------", /* (15, 1) wrapping */
    "--------------", /* (14, 1) wrapping */
    "-------------", /* (13, 1) wrapping */
    "------------", /* (12, 1) wrapping */
    "-----------", /* (11, 1) wrapping */
    "----------", /* (10, 1) wrapping */
    "---------", /* (9, 1) wrapping */
    "--------", /* (8, 1) wrapping */
    "-------", /* (7, 1) wrapping */
    "------", /* (6, 1) wrapping */
    "-----", /* (5, 1) wrapping */
    "----", /* (4, 1) wrapping */
    "---", /* (3, 1) wrapping */
    "--", /* (2, 1) wrapping */
    "-", /* (1, 1) wrapping */
};
    const int maxcell = 1;
    int currcell=0;
    char * p;

    reinit_state();

    /* Clear the working cells */
    if (flg_init) add_str(">[-]<[-]");

    str_cells_used = maxcell+1;

    /* Print each character */
    for(p=buf; *p; p++) {
	int ch = *p & 255;
	int diff = (ch-cells[0]) & 255;
	int l1 = strlen(convbyte[diff]);
	int l2 = strlen(convbyte[ch & 255])+3;

	if (l2<l1) {
	    add_str("[-]");
	    add_str(convbyte[ch & 255]);
	} else
	    add_str(convbyte[diff]);
	cells[0] = ch;

	add_chr('.');
	if (best_len>0 && str_next > best_len) return;	/* Too big already */
    }

    currcell = clear_tape(currcell);

    check_if_best(buf, "twobyte");
}

/******************************************************************************/
/* >+>++>+>+>+++[>[->+++>[->++<]<<<++>]<<] */

void
gen_trislipnest(char * buf)
{
    int cellincs[8] = {0};
    int maxcell = 0;
    int currcell=0;
    int i;

    for(;;)
    {
return_to_top:
	{
	    for(i=0; ; i++) {
		if (i == 8) return;
		cellincs[i]++;
		if (cellincs[i] > 4)
		    ;
		else
		    break;
		cellincs[i] = 1;
	    }

	    maxcell = 8;
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

	for(i=3;i<8 && cellincs[i];i++) {
	    int j;
	    add_chr('>');
	    for(j=0; j<cellincs[i]; j++)
		add_chr('+');
	}
	add_str("[>[->");
	for(i=0; i<cellincs[0]; i++)
	    add_chr('+');
	add_str(">[->");
	for(i=0; i<cellincs[1]; i++)
	    add_chr('+');
	add_str("<]<<<");
	for(i=0; i<cellincs[2]; i++)
	    add_chr('+');
	add_str(">]<<]");

	if (verbose>3)
	    fprintf(stderr, "Running triloop: %s\n", str_start);

	if (best_len>0 && str_next > best_len) goto return_to_top;	/* Too big already */

	/* This is the most reliable way of making sure we have the correct
	 * cells position as produced by the string we've built so far.
	 */
	if (runbf(str_start, 0) != 0) continue;

	if (verbose>3)
	    fprintf(stderr, "Counting triloop\n");

	gen_print(buf, 0);

	check_if_best(buf, "tri sliping loop");
    }
}

/******************************************************************************/

struct bfi { int mov; int cmd; int arg; } *pgm = 0;
int pgmlen = 0;

int
runbf(char * prog, int longrun)
{
    int m= 0, p= -1, n= -1, j= -1, ch;
    int maxcell = 0, countdown = (longrun?1000000:10000);
    while((ch = *prog++)) {
	int r = (ch == '<' || ch == '>' || ch == '+' || ch == '-');
	if (r || (ch == ']' && j>=0) || ch == '[' || ch == '.') {
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
    bf_print_off = 0;

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
	    case '.':
		if (bf_print_off < sizeof(bf_print_buf))
		    bf_print_buf[bf_print_off++] = cells[m];
		break;
        }
    }
}
