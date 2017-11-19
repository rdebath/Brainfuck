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
 *  NOTE: Null bytes are ignored and CR is normally converted.
 *
 *  Also:
 *	+++++++++[>++++++++++>+++++>>+<<<<-]>+>>>+<
 *	,[<<.>.--<++.-->>[-<.>]<+++.->>.<,]
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#if _POSIX_VERSION >= 199506L || defined(LLONG_MAX)
#include <inttypes.h>
#endif

#ifndef NOQTABLE
#include "qtable.h"
#endif

void find_best_conversion(char * linebuf);

int gen_print(char * buf, int init_offset);
void check_if_best(char * buf, char * name);

void gen_subrange(char * buf, int subrange, int flg_zoned);
void gen_ascii(char * buf);
void gen_multonly(char * buf);
void gen_multbyte(char * buf);
void gen_nestloop(char * buf);
void gen_nestloop8(char * buf);
void gen_slipnest(char * buf);
void gen_special(char * buf, char * initcode, char * name, int usercode);
void gen_twoflower(char * buf);
void gen_twincell(char * buf);
void gen_trislipnest(char * buf);
void gen_countslide(char * linebuf);

int runbf(char * prog, int longrun);

#define MAX_CELLS 512
#define MAX_STEPS 1000000
#define PREV_FOUND 32

/* These setup strings have, on occasion, given good results */


/* These are some very generic multiply loops, the first is vaguely based on
 * English language probilities, the others are cell value coverage loops.
 */
#define RUNNERCODE1 "++++++++++++++[>+++++>+++++++>++++++++>++>++++++<<<<<-]"
#define RUNNERCODE2 ">+++++[<++++++>-]<++[>+>++>+++>++++<<<<-]"
#define RUNNERCODE3 ">+++++[<++++++>-]<++[>+>++>+++>++++>+++++>++++++>+++++++<<<<<<<-]"
#define RUNNERCODE4 ">+++++[<++++++>-]<++[>+>++>+++>++++>->-->---<<<<<<<-]"
#define RUNNERCODE5 ">+++++[<+++++++>-]<[>+>++>+++<<<-]"

/* Some previously found "Hello World" prefixes, so you don't have to do big runs. */
char * hello_world[] = {
    "+++++++++++++++[>++++++++>+++++++>++>+++<<<<-]",
    "+++++++++++++++[>+++++++>++++++++>++>+++<<<<-]",
    "+++++++++++++++[>+++++>+++++>++++++>+++>++<<<<<-]",
    "+++++++++++++++[>+++++>+++++>++++++>++>+++<<<<<-]",
    "++++++++++++++[>+++++>+++++++>++++++>+++<<<<-]",
    "++++++++++++++[>+++++>+++++++>++++++>+++>++<<<<<-]",
    "++++++++++++++[>+++++>+++++++>++++++>++>+++<<<<<-]",
    "++++++++++++++[>+++++>+++++++>+++>++++++<<<<-]",
    "+++++++++++++[>+++++>++++++++>+<<<-]",
    "+++++++++++++[>++++>++++>++++>+<<<<-]",
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
    ">++++[<++++>-]<[>+++>+++>+++>+++<<<<-]",
    ">+++++++[<+++>-]<++[>+>++>+++>++++>+++++>+++++++<<<<<<-]",
    ">+++++++[<++++>-]<[>+++>++++>++++<<<-]",

    0 };

char * hello_world2[] = {
    "++++[>+++++<-]>[>[++++++>]+[<]>-]",
    "++++[>+++++<-]>[>[++++++>]++[<]>-]",
    "+++[>++++++<-]>[>[+++++++>]++[<]>-]",
    "++++[>++++++<-]>[>[+++++>]+++[<]>-]",

    "++++[>++++++<-]>[>++>[+++++>]+++[<]>-]",
    "++++[>++++<-]>[>++>[++++++++>]++[<]>-]",
    "++++[>++++<-]>[>++>[++++++++>]+[<]>-]",
    "++++[>++++<-]>[>++>[++++++>]++[<]>-]",
    "++++[>++++<-]>[>+>++>[+++++++>]+++[<]>-]",
    "++++[>++++<-]>[>[++++++>]++[<]>-]",
    "+++[>++++++<-]>[>++>[+++++>]+[<]>-]",
    "+++[>++++++<-]>[>[++++++++>]++[<]>-]",
    "+++[>++++++<-]>[>[+++++++>]+++[<]>-]",

    "++++[>++++++<-]>[>++>[+++++>]+++>[-]<[<]>-]",
    "++++[>++++++<-]>[>[+++++>]+++>[-]<[<]>-]",
    "++++[>+++++<-]>[>[++++++>]++>[-]<[<]>-]",
    "++++[>+++++<-]>[>[++++++>]+>[-]<[<]>-]",
    "++++[>++++<-]>[>++>[++++++++>]++>[-]<[<]>-]",
    "++++[>++++<-]>[>++>[++++++++>]+>[-]<[<]>-]",
    "++++[>++++<-]>[>++>[++++++>]++>[-]<[<]>-]",
    "++++[>++++<-]>[>+>++>[+++++++>]+++>[-]<[<]>-]",
    "++++[>++++<-]>[>[++++++>]++>[-]<[<]>-]",
    "+++[>++++++<-]>[>++>[+++++>]+>[-]<[<]>-]",
    "+++[>++++++<-]>[>[++++++++>]++>[-]<[<]>-]",
    "+++[>++++++<-]>[>[+++++++>]+++>[-]<[<]>-]",
    "+++[>++++++<-]>[>[+++++++>]++>[-]<[<]>-]",

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
    "++++[>+++++++[>+++>+>++++>++++<<<<-]<-]",
    "+++[>+++++[>+++++>+++++>++++++>+++>++<<<<<-]>-<<-]",
    "+++[>+++++[>+++++>+++++>++++++>++>+++<<<<<-]>-<<-]",

    ">+>++++>+>+>+++[>[->+++<<++>]<<]",
    ">+>++++>+>+>+++[>[->+++>[->++<]<<<++>]<<]",
    ">+>++++>+>+[>[->+++++<<++++>]<<]",
    ">+>+++>++>+[>[->++++<<++++>]<<]",
    ">+>+++>+>+++[>[->+++<<+++>]<<]",
    ">+>+++>+>+[>[->+++++<<++++>]<<]",
    ">+>++>++>+++[>[->+++<<+++>]<<]",
    ">+>++>++>+[>[->++++<<++++>]<<]",
    ">+>++>++>+[>[->+++<<++++>]<<]",
    ">+>++>+>+++[>[->+++<<+++>]<<]",
    ">+>++>+>++>+++++[>[->++<<++>]<<]",
    ">+>++>+>++[>[->+++++<<+++>]<<]",
    ">+>++>+>+[>[->+++++<<++++>]<<]",
    ">+>+>+++>+++[>[->+++<<+++>]<<]",
    ">+>+>+++>++[>[->++++<<+++>]<<]",
    ">+>+>+++>+[>[->+++<<++++>]<<]",
    ">+>+>++>+++[>[->+++<<+++>]<<]",
    ">+>+>++>+++[>[->++<<+++>]<<]",
    ">+>+>++>++[>[->++++<<+++>]<<]",
    ">+>+>++>+[>[->++++<<++++>]<<]",
    ">+>+>++>+[>[->+++<<++++>]<<]",
    ">+>+>+>++[>[->+++++<<+++>]<<]",
    ">+>+>+>++[>[->++++<<+++>]<<]",
    ">+>+>+>+[>[->+++++<<++++>]<<]",

    0 };

char * hello_world3[] = {

    "->+>+>>+>---[++++++++++++[>+++++>++>+<<<-]+<+]", /* https://esolangs.org/wiki/Talk:Brainfuck#Shortest_known_.22hello_world.22_program. */
    "--->->->>+>+>>+[++++[>+++[>++++>-->+++<<<-]<-]<+++]",  /* https://esolangs.org/wiki/Talk:Brainfuck#Shortest_known_.22hello_world.22_program. */

    0 };

/* Bytewrap strings
 * Some of these are the results of "Hello World" searches, others (the
 * initial sequences) are NOT generated by this program. Most of those
 * are generated from brute force searches and give "interesting" looking
 * patterns in memory.
 */
char * hello_world_byte[] = {
    "+[[->]-[<]>-]",
    "-[[+>]+[<]>+]",

    "+[[->]-[-<]>-]",
    "-[[+>]+[+<]>+]",

    "+[->>++>+[<]<-]",
    "+[[->]-[-<]>--]",
    "+[[->]-[<]>+>-]",
    "-[[+>]+[<]>+++]",
    ">+[[+>]+[<]>++]",
    ">+[[>]-[---<]>]",
    ">-[[>]+[++<]>+]",

    "+[+>+>->>-[<-]<]",
    "+[>>-->->-[<]<+]",
    "+[>>>+++<[-<]<+]",
    "+[[->]+[----<]>]",
    "+[[->]+[--<]>--]",
    "-[[+>]+[+++<]>+]",
    "-[[+>]+[+++<]>-]",
    ">-[-[->]-[-<]>>]",
    ">-[-[>]+[+++<]>]",

    "++[-[->]-[---<]>]",
    "+[+>+>->>>-[<-]<]",
    "+[->[+>]-[++<]>+]",
    "+[[->]+[----<]>-]",
    "+[[->]+[---<]>--]",
    "+[[->]-[-<]>++>-]",
    "-[+>+++>++[-<]>>]",
    "-[+[+<]>>>>->+>+]",
    ">+[++>++[<]>>->+]",
    ">+[++[<]>->->+>+]",
    ">+[[+>]+[+++<]>-]",
    ">+[[--->]+[--<]>]",
    ">+[[>]++<[---<]>]",
    ">-[-[+>]+[+++<]>]",
    ">-[>[-->]-<[-<]>]",
    ">-[[--->]++[<]>-]",
    ">-[[>]+[+++<]>++]",
    ">-[[>]+[---<]>>-]",

    ">+[>[------>]+[<]>+]",
    "+[------->->->+++<<<]",
    ">+[[++++++++>]+[<]>+]",
    "-[->>[-------->]+[<]<]",
    ">++[[++++++++>]+[<]>+]",
    ">+[>[-------->]+[<]>+]",
    "+>-[>>+>+[++>-<<]-<+<+]",		/* http://inversed.ru/InvMem.htm#InvMem_7 */
    ">++[[++++++++>]+[<]>++]",
    ">+++[>[-------->]-[<]>+]",
    ">++++[[++++++++>]++[<]>+]",
    ">>+[>[-------->]+[<<]>>+]",
    "+[-[<<[+[--->]-[<<<]]]>>>-]",	/* http://inversed.ru/InvMem.htm#InvMem_7 */
    ">>+[++[<+++>->+++<]>+++++++]",	/* https://codegolf.stackexchange.com/questions/55422/hello-world */
    ">-[++[>+++>+++<<<++>-]---->+]",	/* https://codegolf.stackexchange.com/questions/55422/hello-world */
    ">+>+>+>+>++++[>[->+++<<+++>]<<]",
    ">+>>->--<<<[+[>->->-<<<<+>---]>]", /* https://codegolf.stackexchange.com/questions/55422/hello-world */
    "++[+++++++>++++>->->->++>-<<<<<<]",
    "++[+++++++>++++>->->->--->++<<<<<<]",
    "++++[------->-->--->--->->++++<<<<<]",
    ">+>+>+++++++>++>+++[>[->+++<<+++>]<<]",
    "++[--------------->-->--->->+++++<<<<]",
    ">++++[<++++>-]<[++>+++>+++>---->+<<<<]",
    "++++++[+++++++>->++>++>---->+++>-<<<<<<]",
    ">+++++[<++++>-]<[++>+++++>+++>---->+<<<<]",
    "+++++++++++++[+++++++>++>++>----->---->+++<<<<<]",
    "+++++++++++++[------->-->-->+++++>--->++++<<<<<]",
    "+++++++++++++[+++++++>->++>++>----->---->+++<<<<<<]",
    "+++++++++++++[------->+>-->-->+++++>--->++++<<<<<<]",

#if (MAX_CELLS>2814) && (MAX_STEPS>55000000)
    /* Too many steps and too many cells */
    ">-[[-------->]-[--<]>-]",
    ">+[[++++++++>]+[++<]>+]",
#endif

    0 };

int flg_binary = 0;
int flg_init = 0;
int flg_clear = 0;
int flg_signed = 0;
int flg_rtz = 0;

int done_config_wipe = 0;

int enable_multloop = 0;
int multloop_maxcell = 5;
int multloop_maxloop = 40;
int multloop_maxinc = 10;
int multloop_no_prefix = 0;

int enable_sliploop = 0;
int sliploop_maxcell = 7;
int sliploop_maxind = 7;

int enable_subrange = 1;
int subrange_count = 0;

int enable_twocell = 1;
int enable_twincell = 1;
int enable_countslide = 0;
int enable_nestloop = 0;
int enable_trislipnest = 0;
int enable_special = 1;
int enable_rerun = 1;

int flg_fliptwocell = 0;
int flg_lookahead = 0;
int flg_zoned = 0;
int flg_optimisable = 0;
int flg_morefound = 0;

int verbose = 0;
int bytewrap = 0;
int maxcol = 72;
int blocksize = 0;
int cell_limit = MAX_CELLS;

void reinit_state(void);
void output_str(char * s);
void add_chr(int ch);
void add_str(char * p);

char * str_start = 0;
int str_max = 0, str_next = 0, str_mark = 0;
int str_cells_used = 0;

char bf_print_buf[64];
int bf_print_off = 0;

char * best_str = 0;
int best_len = -1;
int best_cells = 1;
int best_mark = 0;
char * best_name = 0;

char * found_init[PREV_FOUND];
char * found_nmid[PREV_FOUND];
int found_len[PREV_FOUND];

char * special_init = 0;

static int cells[MAX_CELLS];

/* Simple fake intmax_t, be careful it's not perfect. */
#ifndef INTMAX_MAX
#define intmax_t double
#define INTMAX_MAX	4503599627370496.0
#define PRIdMAX		".0f"
#endif
intmax_t patterns_searched = 0;

void
wipe_config()
{
    if (done_config_wipe) return;
    done_config_wipe = 1;

    enable_multloop = 0;
    enable_nestloop = 0;
    enable_trislipnest = 0;
    enable_sliploop = 0;
    enable_countslide = 0;
    enable_twocell = 0;
    enable_twincell = 0;
    enable_subrange = 0;
    enable_special = 0;
    enable_rerun = 0;

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
	} else if (strcmp(argv[1], "-w") == 0) {
	    maxcol = 0;
	    argc--; argv++;
	} else if (strncmp(argv[1], "-B", 2) == 0 && argv[1][2] >= '0' && argv[1][2] <= '9') {
	    blocksize = atol(argv[1]+2);
	    argc--; argv++;
	} else if (strcmp(argv[1], "-B") == 0) {
	    blocksize = 8192;
	    argc--; argv++;
	} else if (strncmp(argv[1], "-L", 2) == 0 && argv[1][2] >= '0' && argv[1][2] <= '9') {
	    cell_limit = atol(argv[1]+2);
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
	} else if (strcmp(argv[1], "-rerun") == 0) {
	    enable_rerun = 1;
	    argc--; argv++;
	} else if (strcmp(argv[1], "-F") == 0) {
	    flg_morefound = flg_morefound*2+1;
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
	    multloop_maxloop = 40;
	    multloop_maxinc = 12;
	    enable_twocell = 1;
	    enable_twincell = 1;
	    enable_countslide = 1;
	    enable_subrange = 1;
	    enable_nestloop = 1;
	    enable_trislipnest = 1;
	    enable_special = 1;
	    enable_rerun = 1;
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

	} else if (strcmp(argv[1], "-0") == 0) {
	    flg_optimisable = !flg_optimisable;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-O0") == 0) {
	    wipe_config();
	    enable_subrange = 1;
	    enable_twocell = 1;
	    enable_twincell = 1;
	    enable_special = 1;
	    flg_optimisable = 1;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-O") == 0) {
	    wipe_config();
	    enable_multloop = 1;
	    enable_subrange = 1;
	    enable_twocell = 1;
	    enable_twincell = 1;
	    enable_special = 1;
	    flg_optimisable = 1;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-O2") == 0) {
	    wipe_config();
	    enable_multloop = 1;
	    multloop_maxcell = 7;
	    multloop_maxloop = 40;
	    multloop_maxinc = 12;
	    enable_subrange = 1;
	    enable_twocell = 1;
	    enable_twincell = 1;
	    enable_special = 1;
	    flg_optimisable = 1;
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

	} else if (strcmp(argv[1], "-slide") == 0) {
	    wipe_config();
	    enable_countslide = !enable_countslide;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-slip") == 0) {
	    wipe_config();
	    enable_sliploop = !enable_sliploop;
	    sliploop_maxcell = 7;
	    sliploop_maxind = 6;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-slip2") == 0) {
	    wipe_config();
	    enable_sliploop = 1;
	    sliploop_maxcell = 8;
	    sliploop_maxind = 7;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-slip3") == 0) {
	    wipe_config();
	    enable_sliploop = 1;
	    sliploop_maxcell = 10;
	    sliploop_maxind = 9;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-mult") == 0) {
	    wipe_config();
	    enable_multloop = !enable_multloop;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-mult2") == 0) {
	    wipe_config();
	    enable_multloop = 1;
	    multloop_maxcell = 7;
	    multloop_maxloop = 40;
	    multloop_maxinc = 12;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-mult3") == 0) {
	    wipe_config();
	    enable_multloop = 1;
	    multloop_maxcell = 10;
	    multloop_maxloop = 40;
	    multloop_maxinc = 7;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-mult-bare") == 0) {
            multloop_no_prefix = 1;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-Q") == 0) {
	    wipe_config();
	    enable_twocell = !enable_twocell;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-Q+") == 0) {
	    wipe_config();
	    flg_fliptwocell = !flg_fliptwocell;
	    if (flg_fliptwocell) enable_twocell = 1;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-q") == 0) {
	    wipe_config();
	    enable_twincell = !enable_twincell;
	    if (enable_twincell && !enable_twocell)
		flg_fliptwocell = enable_twocell = 1;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-X") == 0) {
	    wipe_config();
	    enable_special = !enable_special;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-S") == 0) {
	    wipe_config();
	    enable_subrange = !enable_subrange;
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
	    enable_nestloop ++;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-NN") == 0) {
	    wipe_config();
	    enable_nestloop +=2;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-tri") == 0) {
	    wipe_config();
	    enable_trislipnest = 1;
	    argc--; argv++;

	} else if (strcmp(argv[1], "-h") == 0) {
	    fprintf(stderr, "txtbf [options] [file]\n");
	    fprintf(stderr, "-v      Verbose\n");
	    fprintf(stderr, "\n");
	    fprintf(stderr, "-std    Enable the stuff that's enabled by default.\n");
	    fprintf(stderr, "-max    Enable everything -- very slow\n");
	    fprintf(stderr, "-I><    Enable one special string.\n");
	    fprintf(stderr, "-O      Enable easy to optimise codes.\n");
	    fprintf(stderr, "-S      Toggle subrange multiply method\n");
	    fprintf(stderr, "-X      Toggle builtin special strings\n");
	    fprintf(stderr, "\n");
	    fprintf(stderr, "-i      Add/remove cell init strings and return to cell zero\n");
	    fprintf(stderr, "-c      Add/remove cell clear strings and return to cell zero\n");
	    fprintf(stderr, "-rtz    Set/clear Return to cell zero\n");
	    fprintf(stderr, "-sc/-uc Assume input chars are signed/unsigned\n");
	    fprintf(stderr, "-b      Assume cells are bytes\n");
	    fprintf(stderr, "-nb     Assume cells are non-wrapping unsigned bytes\n");
	    fprintf(stderr, "\n");
	    fprintf(stderr, "-q      Toggle two cells method\n");
	    fprintf(stderr, "-Q      Toggle simple two cells method\n");
	    fprintf(stderr, "\n");
	    fprintf(stderr, "-mult   Toggle multiply loops\n");
	    fprintf(stderr, "-mult2  Search long multiply loops (slow)\n");
	    fprintf(stderr, "-mult3  Search long multiply loops (slow)\n");
	    fprintf(stderr, "-slip   Toggle short slip loops\n");
	    fprintf(stderr, "-slip2  Search long slip loops (slow)\n");
	    fprintf(stderr, "-N      Toggle nested loops (very slow)\n");
	    fprintf(stderr, "-tri    Search nested and tri-slip/nest loops\n");
	    fprintf(stderr, "-s10    Try just one subrange multiply size\n");
	    fprintf(stderr, "-M+99   Specify multiply loop max increment\n");
	    fprintf(stderr, "-M*99   Specify multiply loop max loops\n");
	    fprintf(stderr, "-M=99   Specify multiply loop max cells\n");
	    fprintf(stderr, "-zoned  Use zones to pick cell to use.\n");
	    fprintf(stderr, "-lookahead Use lookahead when picking cells.\n");
	    exit(0);
	} else {
	    fprintf(stderr, "Unknown option '%s'\n", argv[1]);
	    exit(1);
	}
    }
    flg_rtz = (flg_rtz || flg_init || flg_clear);
    if (bytewrap) flg_signed = 0;

    if (multloop_maxcell > cell_limit) multloop_maxcell = cell_limit;
    if (sliploop_maxcell > cell_limit) sliploop_maxcell = cell_limit;

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
		*p++ = 0;
		if (!flg_clear) flg_init = 1;
		find_best_conversion(linebuf);
		output_str("\n");
		p = linebuf;
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
    patterns_searched = 0;

    if (special_init != 0)
	gen_special(linebuf, special_init, "from -I option", 1);

    if (enable_special) {
	if (verbose>2) fprintf(stderr, "Trying special strings\n");

	gen_special(linebuf, RUNNERCODE1, "mult English", 0);
	gen_special(linebuf, RUNNERCODE2, "mult*32 to 128", 0);
	if (!flg_signed)
	    gen_special(linebuf, RUNNERCODE3, "mult*32 to 224", 0);
	if (flg_signed || bytewrap)
	    gen_special(linebuf, RUNNERCODE4, "mult*32 to 128/-96", 0);
	gen_special(linebuf, RUNNERCODE5, "Fourcell", 0);

	if (!flg_zoned) {
	    flg_zoned = 1;
	    gen_special(linebuf, RUNNERCODE5, "Zoned fourcell", 0);
	    flg_zoned = 0;
	}

	gen_special(linebuf, "", "Bare cells", 0);
    }

    if (enable_twocell) {
	if (verbose>2) fprintf(stderr, "Trying two cell routine.\n");
	gen_twoflower(linebuf);
    }

#ifndef NOQTABLE
    if (enable_twincell) {
	if (verbose>2) fprintf(stderr, "Trying generic two cell routine.\n");
	gen_twincell(linebuf);
    }
#endif

    if (enable_special) {
	char ** hellos;
	char namebuf[64];
	if (verbose>2) fprintf(stderr, "Trying non-wrap hello-worlds\n");

	for (hellos = hello_world; *hellos; hellos++) {
	    sprintf(namebuf, "Hello world %d", (int)(hellos-hello_world));
	    gen_special(linebuf, *hellos, namebuf, 0);
	}
    }

    if (enable_special && !flg_optimisable) {
	char ** hellos;
	char namebuf[64];
	if (verbose>2) fprintf(stderr, "Trying complex hello-worlds\n");

	for (hellos = hello_world2; *hellos; hellos++) {
	    sprintf(namebuf, "Complex world %d", (int)(hellos-hello_world2));
	    gen_special(linebuf, *hellos, namebuf, 0);
	}

	if (flg_signed || bytewrap)
	    for (hellos = hello_world3; *hellos; hellos++) {
		sprintf(namebuf, "Complex world (-ve) %d", (int)(hellos-hello_world3));
		gen_special(linebuf, *hellos, namebuf, 0);
	    }

	if (bytewrap) for (hellos = hello_world_byte; *hellos; hellos++) {
	    sprintf(namebuf, "Hello bytes %d", (int)(hellos-hello_world_byte));
	    gen_special(linebuf, *hellos, namebuf, 0);
	}
    }

    if (subrange_count > 0) {
	reinit_state();
	gen_subrange(linebuf,subrange_count,flg_zoned);
    } else if (subrange_count == 0) {

	if (enable_subrange) {
	    if (verbose>2)
		fprintf(stderr, "Trying subrange routines @%"PRIdMAX".\n", patterns_searched);

	    if (!flg_zoned) {
		subrange_count=10;
		reinit_state();
		gen_subrange(linebuf,subrange_count,0);

		for(subrange_count = 2; subrange_count<45; subrange_count++) {
		    if (subrange_count == 10) continue;
		    reinit_state();
		    gen_subrange(linebuf,subrange_count,0);
		}
	    }

	    for(subrange_count = 2; subrange_count<45; subrange_count++) {
		reinit_state();
		gen_subrange(linebuf,subrange_count,1);
	    }
	}

	subrange_count = 0;
    }

    if (enable_countslide) {
	if (verbose>2) fprintf(stderr, "Trying 'Counted Slide' routine @%"PRIdMAX".\n", patterns_searched);
	gen_countslide(linebuf);
    }

    if (enable_sliploop) {
	if (verbose>2) fprintf(stderr, "Trying 'slipping loop' routine @%"PRIdMAX".\n", patterns_searched);
	gen_slipnest(linebuf);
    }

    if (enable_trislipnest) {
	if (verbose>2) fprintf(stderr, "Trying tri-slip routine @%"PRIdMAX".\n", patterns_searched);
	gen_trislipnest(linebuf);
    }

    if (enable_multloop) {
	if (verbose>2) fprintf(stderr, "Trying multiply loops @%"PRIdMAX".\n", patterns_searched);
	/* Hide the unlikey ones */
	if (!enable_special && multloop_maxloop >= 35 &&
		multloop_maxcell >= 4 && multloop_maxinc >= 3)
	    gen_special(linebuf, RUNNERCODE5, "Fourcell", 0);
	gen_multonly(linebuf);
    }

    if (enable_multloop && bytewrap) {
	if (verbose>2) fprintf(stderr, "Trying byte multiply loops @%"PRIdMAX".\n", patterns_searched);
	gen_multbyte(linebuf);
    }

    if(enable_nestloop) {
	if (verbose>2) fprintf(stderr, "Trying 'nested loop' routine @%"PRIdMAX".\n", patterns_searched);
	reinit_state();
	if (enable_nestloop>1)
	    gen_nestloop(linebuf);
	else
	    gen_nestloop8(linebuf);
    }

    if (enable_rerun && !flg_lookahead && !flg_zoned) {
	int i;
	char *newname;
	if (verbose>1) fprintf(stderr, "Rerunning with lookahead on saved list\n");
	flg_lookahead = 1;

	for(i=0; i<PREV_FOUND; i++)
	    if (found_init[i] && found_nmid[i]) {
		newname = malloc(strlen(found_nmid[i]) + 32);
		strcpy(newname, found_nmid[i]);
		strcat(newname, " (rerun)");
		gen_special(linebuf, found_init[i], newname, 1);
		free(newname);
	    }

	flg_lookahead = 0;
    }

    if (verbose>1 && patterns_searched>1)
	fprintf(stderr, "Total of %"PRIdMAX" patterns searched.\n", patterns_searched);

    if (best_len>=0) {
	if (verbose)
	    fprintf(stderr, "BF Size = %d, %.2f bf/char, cells = %d, '%s'\n",
		best_len, best_len * 1.0/ strlen(linebuf),
		best_cells, best_name);

	output_str(best_str);
	free(str_start); str_start = 0; str_max = str_next = str_mark = 0;
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
    if (str_cells_used > cell_limit && cell_limit > 0) return;

    if (best_len == -1 || best_len >= str_next) {
	/* We have a short string; check if it actually does the job! */
	int clr_ok = 1, run_ok = 1, rv;

	if (flg_init) {
	    int i;
	    for(i=0; i<MAX_CELLS; i++)
		cells[i] = 0x55;
	} else
	    memset(cells, 0, sizeof(cells));

	rv = runbf(str_start, 1);
	if (rv < -1) run_ok = 0;  /* Will timeout for large inputs */

	/* Only check the clear if we ran to completion */
	if (rv >= 0 && strncmp(buf, bf_print_buf, bf_print_off) == 0) {

	    if (flg_clear) {
		int i, clean_unknown = 0;

		if (flg_init) {
		    memset(cells, 0, sizeof(cells));
		    clean_unknown = (runbf(str_start, 1) < 0);
		}

		if (!clean_unknown)
		    for(i=0; i<str_cells_used; i++)
			if (cells[i] != 0)
			    clr_ok = 0;
	    }
	}

	if (flg_init)
	    memset(cells, 0, sizeof(cells));

	if (!clr_ok || !run_ok || strncmp(buf, bf_print_buf, bf_print_off) != 0) {
	    int i;
	    fprintf(stderr, "ERROR: Consistancy check for generated BF code FAILED.\n");
	    if (!run_ok)
		fprintf(stderr, "ERROR: Interpreter range error.\n");
	    if (!clr_ok)
		fprintf(stderr, "ERROR: Cell clear function failed.\n");
	    fprintf(stderr, "Name: '%s'\n", name);
	    fprintf(stderr, "Code: '%s'\n", str_start);
	    fprintf(stderr, "Requested output: \"");
	    for(i=0; buf[i]; i++) {
		int c = buf[i];
		if (c>=' ' && c<='~' && c!='\\' && c!='"')
		    fprintf(stderr ,"%c", c);
		else if (c=='\n')
		    fprintf(stderr ,"\\n");
		else if (c=='"' || c=='\\')
		    fprintf(stderr ,"\\%c", c);
		else
		    fprintf(stderr ,"\\%03o", c);
	    }
	    fprintf(stderr, "\"\n");
	    exit(99);
	}
    }

    if (best_len == -1 || best_len > str_next ||
       (best_len == str_next && (
	    (str_cells_used < best_cells) ||
	    (str_cells_used == best_cells && str_mark != 0 && str_mark < best_mark)
	    ))) {
	if (best_str) free(best_str);
	best_str = malloc(str_next+1);
	memcpy(best_str, str_start, str_next+1);
	if (best_name) free(best_name);
	best_name = malloc(strlen(name)+1);
	strcpy(best_name, name);
	best_len = str_next;
	best_mark = str_mark;
	best_cells = str_cells_used;
    }

    if (best_len+flg_morefound >= str_next) {
	if (!enable_rerun || !str_mark) {
	    if (best_len != str_next) ; else
	    if (verbose>2 || (verbose == 2 && str_next < 63))
		fprintf(stderr, "Found '%s' len=%d, cells=%d, '%s'\n",
			name, str_next, str_cells_used, str_start);
	    else if (verbose>1) {
		if (str_mark)
		    fprintf(stderr, "Found '%s' len=%d, cells=%d, '%.*s' ...\n",
			name, str_next, str_cells_used, str_mark, str_start);
		else
		    fprintf(stderr, "Found '%s' len=%d, cells=%d, '%.60s ...\n",
			name, str_next, str_cells_used, str_start);
	    }
	} else if (str_mark) {
	    int i, skip_it = 0;
	    char * str;
	    int use = 0, use_len = found_len[use];

	    str = malloc(str_mark+1);
	    memcpy(str, str_start, str_mark);
	    str[str_mark] = 0;

	    for(i=0; i<PREV_FOUND; i++) {
		if (found_init[i] == 0) {
		    use = i;
		    use_len = 0;
		    break;
		}
		if (found_len[i] > use_len) {
		    use = i;
		    use_len = found_len[use];
		}
	    }

	    for(i=0; i<PREV_FOUND; i++) {
		if (found_init[i] != 0 && strcmp(found_init[i], str) == 0) {
		    skip_it = 1;
		    break;
		}
	    }
	    if (skip_it || (use_len != 0 && use_len <= str_next)) {
		/* Duplicates can happen due to zoned vs unzoned. */
		free(str);
	    } else {
		if (found_init[use] != 0) free(found_init[use]);
		if (found_nmid[use] != 0) free(found_nmid[use]);
		found_nmid[use] = malloc(strlen(name)+1);
		strcpy(found_nmid[use], name);

		found_init[use] = str;
		found_len[use] = str_next;

		if (verbose>2 || (verbose == 2 && str_next < 63))
		    fprintf(stderr, "Saved '%s' len=%d, cells=%d, '%s'\n",
			    name, str_next, str_cells_used, str_start);
		else if (verbose>1) {
		    fprintf(stderr, "Saved '%s' len=%d, cells=%d, '%.*s' ...\n",
			name, str_next, str_cells_used, str_mark, str_start);
		}
	    }
	}
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
    str_next = str_mark = 0;
    if (str_start) str_start[str_next] = 0;
    else add_str("<>");
}

int
clear_tape(int currcell)
{
    if (!flg_optimisable && flg_clear && cells[0] == 0) {
	int usefn = 1;
	int cc = currcell, i, cval = 0;

	while (cells[cc] && cc<str_cells_used) cc++;
	if (cc>=str_cells_used && flg_init) usefn = 0;	/* Don't expand the array */
	if (usefn)
	    for(i=cc; i<str_cells_used; i++)
		if (cells[i]) {usefn = 0; break;}
	if (usefn) {
	    cc--;
	    while(cc>0) {
		if (!bytewrap && cells[cc] < 0) break;
		if (cells[cc] == 0) break;
		cc--; cval+=5;
	    }
	    if (cc<0 || cells[cc]) usefn = 0;
	}
	if (usefn && cval > 10) {
	    add_str("[>]<[[-]<]");
	    currcell = cc;
	    for(i=cc; i<str_cells_used;i++)
		cells[i] = 0;
	}
    }

    if (flg_clear) {
	int i;

	/* Clear the working cells */
	for(i=MAX_CELLS-1; i>=0; i--) {
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
    }

    /* Put the pointer back to the zero cell */
    if (flg_clear || flg_rtz || flg_init)
    {
	if (!flg_optimisable && cells[0] == 0 && currcell>3) {
	    int usefn = 1;
	    int cc = currcell, cval = 0;

	    while(cc>0) {
		if (cells[cc] == 0) break;
		cc--; cval++;
	    }
	    if (cc<0 || cells[cc]) usefn = 0;

	    if (usefn && cval > 3) {
		add_str("[<]");
		currcell = cc;
	    }
	}

	while(currcell > 0) { add_chr('<'); currcell--; }
    }

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

 */

void
gen_subrange(char * buf, int subrange, int flg_subzone)
{
    int counts[256] = {};
    int rng[256] = {};
    int currcell=0, usecell=0;
    int maxcell = 0;
    char * p;
    int i, j, rv = 0;
    char name[256];

    sprintf(name, "Subrange %d%s", subrange, flg_subzone?", zoned":"");

    patterns_searched++;

    /* Count up a frequency table */
    for(p=buf; *p;) {
	int c = *p++;
	int r;
	if (flg_signed) c = (signed char)c; else c = (unsigned char)c;
	r = (c+subrange/2)/subrange;
	usecell = (r & 0xFF);
	rng[usecell] = r;

	if (usecell > maxcell) maxcell = usecell;

	if (r == 0) continue;
	counts[usecell] ++;
    }

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

    str_cells_used = 0;
    if (maxcell>1)
	for(i=maxcell; i>=0; i--)
	    if ((counts[i] || i == 0) && i>str_cells_used)
		str_cells_used = i;
    maxcell = str_cells_used++;

    if (flg_init) {
	/* Clear the working cells */
	for(i=0; i<=maxcell; i++) {
	    if (counts[i] || i == 0) {
		while(currcell < i) { add_chr('>'); currcell++; }
		while(currcell > i) { add_chr('<'); currcell--; }
		add_str("[-]");
		cells[i] = 0;
	    }
	}
    }

    /* Generate the init strings */
    if (maxcell > 1) {
	int f = 0, loopcell = 0;
	if (currcell == maxcell && !flg_subzone) {
	    f = 2;
	    loopcell = maxcell;
	}

	while(currcell > loopcell) { add_chr('<'); currcell--; }
	while(currcell < loopcell) { add_chr('>'); currcell++; }

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
	    if(r>1) add_chr('>'^f);
	    for(i=0; i<l; i++) add_chr('+');
	    if(r>1) {
		add_chr('['); add_chr('<'^f);
		for(i=0; i<r; i++) add_chr('+');
		add_chr('>'^f); add_chr('-'); add_chr(']'); add_chr('<'^f);
	    }
	    for(i=l*r; i<subrange; i++) add_chr('+');
	} else {
	    for(i=0; i<subrange; i++)
		add_chr('+');
	}
	add_chr('[');
	for(i=1; i<=maxcell; i++)
	{
	    int c = i;
	    if (f) c = maxcell-i;
	    if (counts[c] || c == 0) {
		while(currcell < c) { add_chr('>'); currcell++; }
		while(currcell > c) { add_chr('<'); currcell--; }
		cells[currcell] += subrange * rng[c];
		if(bytewrap) cells[currcell] &= 255;
		for(j=0; j<rng[c]; j++) add_chr('+');
		for(j=0; j<-rng[c]; j++) add_chr('-');
	    }
	}
	while(currcell > loopcell) { add_chr('<'); currcell--; }
	while(currcell < loopcell) { add_chr('>'); currcell++; }
	add_chr('-');
	add_chr(']');
    }

    if (best_len>0 && str_next > best_len) return;	/* Too big already */

    if (verbose>3)
	fprintf(stderr, "Trying %s: %s\n", name, str_start);

    /* We have two choices on how to choose which cell to use; either pick
     * whatever is best right now, or stick with the ranges we chose above.
     * Either could end up better by the end of the string.
     */
    if (!flg_subzone) {
	rv = gen_print(buf, currcell);
    } else {
	/* Set the translation table */
	int txn[256] = {};
	for(i=0; i<=maxcell; i++) txn[rng[i] & 0xFF] = i;
	str_mark = str_next;

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

    if (rv>=0)
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
    int cellincs[MAX_CELLS] = {0};
    int maxcell = 0;
    int currcell=0;
    int i;
    int max_ch = -128, min_ch = 255;
    char * p;

    for(p=buf; *p;) {
        int c = *p++;
        if (flg_signed) c = (signed char)c; else c = (unsigned char)c;
	if (c<min_ch) min_ch = c;
	if (c>max_ch) max_ch = c;
    }

    for(;;)
    {
return_to_top:
	{
	    int toohigh;
	    for(i=0; ; i++) {
		toohigh = 0;
		if (i >= multloop_maxcell) return;
		cellincs[i]++;
		if (i == 0) {
		    if (cellincs[i] <= multloop_maxloop) break;
		    if (cellincs[i] < 127 && bytewrap) {
			cellincs[i] = 127;
			break;
		    }
		} else {
		    if (!bytewrap && (
			    cellincs[i]*cellincs[0] > 255 ||
			    (cellincs[i]-1)*cellincs[0] >= max_ch ||
			    (cellincs[i]+1)*cellincs[0] < min_ch
			    ))
			toohigh = 1;
		    if (cellincs[i] <= multloop_maxinc) break;
		}
		cellincs[i] = 1;
	    }

	    if (toohigh) goto return_to_top;

	    for(maxcell=0; cellincs[maxcell] != 0; )
		maxcell++;

	    maxcell--; currcell=0;
	    if (maxcell == 0) goto return_to_top;
	}

	patterns_searched++;

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

	if (gen_print(buf, 0) >=0 )
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
    int cellincs[MAX_CELLS] = {0};
    int cellincs2[MAX_CELLS] = {0};
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

	patterns_searched++;

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

	if (gen_print(buf, 0) >= 0)
	    check_if_best(buf, "byte multiply loop");
    }
}

/******************************************************************************/
/*
   This searches through double multipler loops, with an extra addition or
   subtraction on each cell round the outer loop.

   The configurations are are in the defines below for the moment.

   The text is then generated using a closest next function.

   This currently searches far too many patterns.
 */
#define ADDINDEX 5
#define MAXCELLINCS 8
#define MAXCELLVALA 11
#define MAXCELLVALB 5
#define MAXCELLVALC 4
#define MAXCELLBASE 128
void
gen_nestloop(char * buf)
{
    int cellincs[16] = {0};
    int cellinner[16] = {0};
    int cellouter[16] = {0};
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
		if (i == 0 && cellincs[i] <= MAXCELLVALA) break;
		if (i == 1 && cellincs[i] <= MAXCELLVALB) break;
		if (i >= 2 && cellincs[i] < (MAXCELLVALC+1)*ADDINDEX) break;
		cellincs[i] = 1;
		if (i==0) cellincs[i] = 4;
		if (i==1) cellincs[i] = 2;
	    }

	    for(maxcell=0; cellincs[maxcell] != 0; ) {
		i = maxcell++;

		if (i>=2) {
		    cellinner[i] = (cellincs[i])/ADDINDEX;
		    cellouter[i] = (cellincs[i])%ADDINDEX - (ADDINDEX/2);
		    j = cellincs[0]*(cellincs[1]*cellinner[i]+cellouter[i]);

		    if (j >= MAXCELLBASE || j < -128 || j == 0)
			goto return_to_top;
		}
	    }

	    maxcell--; currcell=0;
	}

	if (maxcell>cell_limit) continue;

	patterns_searched++;

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

	for(i=2; i<=maxcell; i++) {
	    cells[i] = cellincs[0]*(cellincs[1]*cellinner[i]+cellouter[i]);
	    if (cells[i] > 255) goto return_to_top;
	}

	str_cells_used = maxcell+1;

	if (verbose>3 || (verbose>2 && patterns_searched%10000000 == 0))
	    fprintf(stderr, "Trying nestloop[%" PRIdMAX "]: "
			    "%d*(%d*[%d,%d %d,%d %d,%d %d,%d %d,%d %d,%d]\n",
		patterns_searched,
		cellincs[0], cellincs[1],
		cellinner[2], cellouter[2],
		cellinner[3], cellouter[3],
		cellinner[4], cellouter[4],
		cellinner[5], cellouter[5],
		cellinner[6], cellouter[6],
		cellinner[7], cellouter[7]);

	for(j=0; j<cellincs[0]; j++)
	    add_chr('+');
	add_chr('[');
	add_chr('>');
	for(j=0; j<cellincs[1]; j++)
	    add_chr('+');
	add_chr('[');

	for(i=2; i<=maxcell; i++) {
	    add_chr('>');
	    for(j=0; j<cellinner[i]; j++)
		add_chr('+');
	}

	for(i=2; i<=maxcell; i++)
	    add_chr('<');
	add_chr('-');
	add_chr(']');

	currcell=1;
	for(i=2; i<=maxcell; i++) {
	    j = cellouter[i];
	    if (j) {
		while (currcell<i) {
		    add_chr('>');
		    currcell++;
		}
		while (j>0) {add_chr('+'); j--; }
		while (j<0) {add_chr('-'); j++; }
	    }
	}

	if (currcell <= 4 || flg_optimisable) {
	    while(currcell > 0) {
		add_chr('<');
		currcell--;
	    }
	} else {
	    add_str("[<]<");
	}
	add_str("-]");

	if (best_len>0 && str_next > best_len) goto return_to_top;	/* Too big already */

	if (verbose>3)
	    fprintf(stderr, "Counting nestloop\n");

	if (gen_print(buf, 0) >= 0)
	    check_if_best(buf, "nested loop");
    }
}

/*
   This searches through double multipler loops, with an extra addition or
   subtraction on each cell round the outer loop.

   The text is then generated using a closest next function.

   The initial two cells are fixed to 8 and 4 and the ranges of the other
   increments are set so that all multiples of 8 are available for all
   the cells used from 2..N.

   This seem to give a good collection of starters that can be searched
   in a few seconds.
 */

void
gen_nestloop8(char * buf)
{
    int cellincs[8] = {0};
    int cellinner[8] = {0};
    int cellouter[8] = {0};
    int maxcell = 0;
    int currcell=0;
    int i,j;

    for(;;)
    {
return_to_top:
	{
	    for(i=0; ; i++) {
		if (i == 6) return;
		cellincs[i]++;
		if (cellincs[i] < 17) break;
		cellincs[i] = 1;
	    }

	    for(maxcell=0; cellincs[maxcell] != 0; ) {
		i= maxcell++;
		cellinner[i] = (cellincs[i]+1)/4;
		cellouter[i] = (cellincs[i]+1)%4-1;
	    }

	    maxcell++; currcell=0;
	}

	if (maxcell>cell_limit) continue;

	patterns_searched++;

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

	for(i=2; i<=maxcell; i++) {
	    cells[i] = 8*(4*cellinner[i-2]+cellouter[i-2]);
	    if (cells[i] > 255) goto return_to_top;
	}

	str_cells_used = maxcell+1;

	if (verbose>3)
	    fprintf(stderr, "Trying nestloop8: "
			    "8*(4*[%d,%d %d,%d %d,%d %d,%d %d,%d %d,%d]\n",
		cellinner[0], cellouter[0],
		cellinner[1], cellouter[1],
		cellinner[2], cellouter[2],
		cellinner[3], cellouter[3],
		cellinner[4], cellouter[4],
		cellinner[5], cellouter[5]);

	add_str("++++++++[>++++[");

	for(i=2; i<=maxcell; i++) {
	    add_chr('>');
	    for(j=0; j<cellinner[i-2]; j++)
		add_chr('+');
	}

	for(i=2; i<=maxcell; i++)
	    add_chr('<');
	add_chr('-');
	add_chr(']');

	currcell=1;
	for(i=2; i<=maxcell; i++) {
	    j = cellouter[i-2];
	    if (j) {
		while (currcell<i) {
		    add_chr('>');
		    currcell++;
		}
		while (j>0) {add_chr('+'); j--; }
		while (j<0) {add_chr('-'); j++; }
	    }
	}

	if (currcell <= 4 || flg_optimisable) {
	    while(currcell > 0) {
		add_chr('<');
		currcell--;
	    }
	} else {
	    add_str("[<]<");
	}
	add_str("-]");

	if (best_len>0 && str_next > best_len) goto return_to_top;	/* Too big already */

	if (verbose>3)
	    fprintf(stderr, "Counting nestloop8\n");

	if (gen_print(buf, 0) >= 0)
	    check_if_best(buf, "nested loop8");
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

	patterns_searched++;

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

	if (gen_print(buf, 0) >= 0)
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
gen_special(char * buf, char * initcode, char * name, int usercode)
{
    int maxcell = 0;
    int currcell=0;
    int remaining_offset = 0;
    int i;
    int bytewrap_override = 0;
    char init_mode[MAX_CELLS];

    reinit_state();

    if (verbose>3)
	fprintf(stderr, "Running '%s': %s\n", name, initcode);

    patterns_searched++;

    /* Work out what the 'special' code does to the cell array ... How?
     *
     * By running it of course!
     * Use a local interpreter so we can give a better error message.
     */
    if(!usercode && !flg_init) {
	remaining_offset = runbf(initcode, 1);
	if (remaining_offset<0) {
	    fprintf(stderr, "Code '%s' failed '%s'\n", name, initcode);
	    return;
	}
	maxcell = str_cells_used-1;
    } else {
	char *p, *b;
	int m=0, nestlvl=0;
	int countdown = MAX_STEPS;

	maxcell = -1;
	str_cells_used = maxcell+1;
	init_mode[0] = (*initcode == '\0');

	for(p=b=initcode;*p;p++) {
	    switch(*p) {
		case '>': m++;break;
		case '<': m--;break;
		case '+': cells[m]++; if(!init_mode[m]) init_mode[m] = 1; break;
		case '-': cells[m]--; if(!init_mode[m]) init_mode[m] = 1; break;
		case '[':
		    if(!init_mode[m]) {
			if(p[1] == '-' && p[2] == ']')
			    init_mode[m] = 2;
			else
			    init_mode[m] = 1;
		    }
		    if(cells[m]==0)while((nestlvl+=(*p=='[')-(*p==']'))&&p[1])p++;
		    break;
		case ']':
		    if(!init_mode[m]) init_mode[m] = 1;
		    if(cells[m]!=0)while((nestlvl+=(*p==']')-(*p=='['))&&p>b)p--;
		    break;
	    }
	    if (m<0 || m>= MAX_CELLS || --countdown == 0) break;
	    if (m>maxcell) {
		maxcell = m; str_cells_used = maxcell+1;
		if (m>0) init_mode[m] = 0;
	    }
	    if (bytewrap || bytewrap_override) cells[m] &= 255;
	    else if ((cells[m] & 255) == 0 && cells[m] != 0) {
		bytewrap_override = 1;
		cells[m] &= 255;
	    }
	}

	/* Something went wrong; the code is invalid */
	if (*p) {
	    fprintf(stderr, "Code '%s' failed to run cellptr=%d countdown=%d iptr=%d '%s'\n",
		    name, m, countdown, (int)(p-initcode), initcode);
	    return;
	}
	remaining_offset = m;
    }

    if (bytewrap_override)
	fprintf(stderr,
		"WARNING: Assuming bytewrapped interpreter for code %s\n",
		name);

    if (flg_init) {
	int init_done = 0;

	if (maxcell>10 && maxcell < 256 && !flg_optimisable) {
	    int dirty = 0;
	    for(i=0; i<maxcell+1; i++)
		if (init_mode[i] == 1)
		    dirty++;

	    if (dirty>10) {
		/* This is a "self cleaning rail", it fills N cells with '1'
		 * completely ignoring the previous contents. It then removes
		 * the rail to leave clean cells.
		 */
		int v = maxcell-2;
		add_str("[-]");
		if (v>16) {
		    add_str(">[-]");
		    for(i=0; i<v/8; i++)
			add_chr('+');
		    add_str("[-<++++++++>]<");
		    v = v%8;
		}
		for(i=0;i<v; i++)
		    add_chr('+');
		add_str(">[-]>[-]<<[->>[>]+>[-]<[<]<]");
		add_str(">>[>]<[-<]<");
		init_done = 1;
	    }
	}

	/* Clear the working cells */
	if (!init_done) {
	    if (maxcell<0) maxcell=0;
	    for(i=maxcell; i>=0; i--) {
		if(init_mode[i] == 1 || i == 0) {
		    while(currcell < i) { add_chr('>'); currcell++; }
		    while(currcell > i) { add_chr('<'); currcell--; }
		}
		if(init_mode[i] == 1) {
		    add_str("[-]");
		}
	    }
	}
    }

    add_str(initcode);

    if (best_len>0 && str_next > best_len) return;	/* Too big already */

    if (bytewrap_override) bytewrap = 1;
    if (gen_print(buf, remaining_offset) >= 0)
	check_if_best(buf, name);
    if (bytewrap_override) bytewrap = 0;
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
int
gen_unzoned(char * buf, int init_offset)
{
    char * p;
    int i;
    int currcell = init_offset;
    int more_cells = 0;
    int min_cell = 0;

    if (!flg_init && !flg_optimisable && str_cells_used > 0)
	if (str_cells_used < cell_limit || cell_limit <= 0)
	    str_cells_used++;

    if (str_cells_used <= 0) { more_cells=1; str_cells_used=1; }
    if (str_cells_used >= cell_limit) more_cells = 0;
    str_mark = str_next;

    if (flg_clear && !flg_optimisable && str_cells_used > 4 && cells[0] == 0)
	min_cell = 1;

    /* Print each character, use closest cell. */
    for(p=buf; *p;) {
	int minrange = 999999;
	int c = *p++;
	int usecell = 0, diff;

	if (flg_signed) c = (signed char)c; else c = (unsigned char)c;

	for(i=min_cell; i<str_cells_used+more_cells; i++) {
	    int range = c - cells[i];
	    if (bytewrap) range = (signed char)range;
	    range = abs(range);
	    range += abs(currcell-i);

	    if (range < minrange) {
		usecell = i;
		minrange = range;
	    }
	}

	if (!flg_optimisable && currcell > usecell && currcell-3 > usecell) {
	    int tcell = currcell;
	    while(tcell>0) {if (cells[tcell] == 0) break; tcell--;}
	    if (cells[tcell] == 0 && abs(usecell-tcell)+3 < currcell-usecell) {
		currcell = tcell;
		add_chr('[');
		add_chr('<');
		add_chr(']');
	    }
	}

	if (!flg_optimisable && currcell < usecell && currcell+3 < usecell) {
	    int tcell = currcell;
	    if (flg_init) {
		while(tcell<str_cells_used-1) {if (cells[tcell] == 0) break; tcell++;}
	    } else {
		while(tcell<str_cells_used) {if (cells[tcell] == 0) break; tcell++;}
	    }
	    if (cells[tcell] == 0 && abs(usecell-tcell)+3 < usecell-currcell) {
		currcell = tcell;
		add_chr('[');
		add_chr('>');
		add_chr(']');
	    }
	}

	while(currcell > usecell) { add_chr('<'); currcell--; }
	while(currcell < usecell) { add_chr('>'); currcell++; }

	if (currcell>=str_cells_used) {
	    str_cells_used++;
	    if (str_cells_used >= cell_limit) more_cells = 0;
	    if (flg_init) add_str("[-]");
	}

	diff = c - cells[currcell];
	if (bytewrap) diff = (signed char)diff;
	if (c>=0 && diff<0 && c+3<-diff) { add_str("[-]"); cells[currcell]=0; diff=c; }
	while(diff>0) { add_chr('+'); cells[currcell]++; diff--; }
	while(diff<0) { add_chr('-'); cells[currcell]--; diff++; }
	if (bytewrap) cells[currcell] &= 255;

	add_chr('.');

	if (best_len>0 && str_next > best_len+flg_morefound) return -1; /* Too big already */
    }

    currcell = clear_tape(currcell);
    return 0;
}

/*******************************************************************************
 * This is like the normal unzoned except for each choice at this time it
 * does a lookahead to see if this single 'wrong' choice gives a better
 * result at the end.
 */
int
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
		int la, j, mc = 15;
		int cells2[MAX_CELLS];
		for(j=0; j<str_cells_used; j++)
		    cells2[j] = cells[j];
		cells2[i] = c;

		for(la=0; p[la] && mc>0 && range < minrange; la++, mc--)
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

	    if (range < minrange) {
		usecell = i;
		minrange = range;
	    }
	}

	if (!flg_optimisable && currcell > usecell && currcell-3 > usecell) {
	    int tcell = currcell;
	    while(tcell>0) {if (cells[tcell] == 0) break; tcell--;}
	    if (cells[tcell] == 0 && abs(usecell-tcell)+3 < currcell-usecell) {
		currcell = tcell;
		add_chr('[');
		add_chr('<');
		add_chr(']');
	    }
	}

	if (!flg_optimisable && currcell < usecell && currcell+3 < usecell) {
	    int tcell = currcell;
	    if (flg_init) {
		while(tcell<str_cells_used-1) {if (cells[tcell] == 0) break; tcell++;}
	    } else {
		while(tcell<str_cells_used) {if (cells[tcell] == 0) break; tcell++;}
	    }
	    if (cells[tcell] == 0 && abs(usecell-tcell)+3 < usecell-currcell) {
		currcell = tcell;
		add_chr('[');
		add_chr('>');
		add_chr(']');
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

	if (best_len>0 && str_next > best_len+flg_morefound) return -1; /* Too big already */
    }

    currcell = clear_tape(currcell);
    return 0;
}

/*******************************************************************************
 * Given a set of cells and a string to generate them already in the buffer
 * this routine uses those cells to make the string.
 *
 * This method uses the cell that was closest when at the start of the run.
 * The result is that the character set is divided into zones and each is
 * assigned to a cell.
 */
int
gen_zoned(char * buf, int init_cell)
{
    char * p;
    int i, st = -1;
    int currcell = init_cell;
    int cells2[MAX_CELLS];

    if (str_cells_used <= 0) str_cells_used=1;
    str_mark = str_next;

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
	    range = abs(range) * 4;
	    range += abs(currcell-i);

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

	if (best_len>0 && str_next > best_len+flg_morefound) return -1; /* Too big already */
    }

    currcell = clear_tape(currcell);
    return 0;
}

int
gen_print(char * buf, int init_offset)
{
    if (flg_zoned)
	return gen_zoned(buf, init_offset);
    else if (flg_lookahead)
	return gen_lookahead(buf, init_offset);
    else
	return gen_unzoned(buf, init_offset);
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

/* This table gives the shortest strings (using a simple multiplier form)
 * for all the values upto 255. It was created with a simple brute force
 * search.
 */
static char bestfactor[] = {
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

void
gen_twoflower(char * buf)
{

    const int maxcell = 1;
    int currcell=0;
    int i;
    char * p;

    reinit_state();
    patterns_searched++;

    /* Clear the working cells */
    if (flg_init) {
	add_str("[-]>[-]");
	if (flg_fliptwocell)
	    currcell=!currcell;
	else
	    add_chr('<');
    }

    str_cells_used = maxcell+1;

    /* Print each character */
    for(p=buf; *p;) {
	int cdiff,t;
	int a,b,c = *p++;
	if (flg_signed) c = (signed char)c; else c = (unsigned char)c;

	cdiff = c - cells[currcell];
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
		if(cells[currcell]>0) add_str("[-]");
		if(cells[currcell]<0) add_str("[+]");
		cells[currcell] = 0;
	    }
	}

	if (a>1) {
	    if (cdiff<0) a= -a;

	    if (flg_fliptwocell && cells[currcell] == cells[!currcell])
		currcell = !currcell;
	    else if (currcell)
		add_chr('<');
	    else
		add_chr('>');

	    for(i=0; i<b; i++) add_chr('+');

	    if (currcell) add_str("[>"); else add_str("[<");
	    while(a > 0) { add_chr('+'); a--; cells[currcell] += b; }
	    while(a < 0) { add_chr('-'); a++; cells[currcell] -= b; }
	    if (currcell) add_str("<-]>"); else add_str(">-]<");
	}

	while(cells[currcell] < c) { add_chr('+'); cells[currcell]++; }
	while(cells[currcell] > c) { add_chr('-'); cells[currcell]--; }

	add_chr('.');
	if (best_len>0 && str_next > best_len) return;	/* Too big already */
    }

    currcell = clear_tape(currcell);

    check_if_best(buf, "twocell");
}

/*******************************************************************************
 * This is a generator that uses two cells usually one is a multipler which
 * is used to get a second to the correct value. It uses precalculated tables
 * to remove the need for an explicit search.
 *
 * It's the classic algorithm but only rarely is it the shortest.
 */

#ifndef NOQTABLE
void
gen_twincell(char * buf)
{
    const int maxcell = 1;
    int currcell=0;
    char * p;
    int curr = 0;

    reinit_state();
    patterns_searched++;

    /* Clear the working cells */
    if (flg_init) add_str(">[-]<[-]");

    str_cells_used = maxcell+1;

    /* Print each character */
    for(p=buf; *p; p++) {
	char *s;
	int nextc = (unsigned char) *p;
	if (bytewrap) s = bftable_b[curr][nextc];
	else if (flg_signed) s = bftable_s[curr][nextc];
	else s = bftable_u[curr][nextc];
	curr = nextc;

	while(*s) {
	    if (*s == '>' || *s == '<') {
		if (currcell) add_chr('<');
		else add_chr('>');
		currcell = !currcell;
	    } else
		add_chr(*s);
	    s++;
	}

	add_chr('.');
    }

    cells[currcell] = curr;
    currcell = clear_tape(currcell);

    check_if_best(buf, "twincell");
}
#endif

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

	patterns_searched++;
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

/* This generates some sequences that create a ramp of values from large
 * to small. The collection of values is usually scattered nicely over
 * the ASCII range (or the full unsigned byte range). The property that
 * usually makes these special is that they are a rather short way to
 * generate this sort of pattern.
 */
void
gen_countslide(char * linebuf)
{
static char codebuf[128], *s;
static char namebuf[128];
    int p1,p2,p3,p4;

static char * extra_cell[] = {
    "[>[",
    "[>+>[",
    "[>++>[",
    "[>+++>[",
    "[>+>++>[",
    "[>++>+>[",
    0};

    for(p2=0; p2<6; p2++)
	for(p4=-10; p4<10; p4++)
	    if (p4 != 0 && (flg_signed || p4>0))
		for(p3=3; p3<20; p3++)
		    for(p1=7; p1<65; p1++)
		    {
			int i,t,a,b,c=0;
			if ( (p1-1)*p3+p4 > 255) continue;
			if ( p4<0 && p3 <= -p4) continue;

			s = codebuf;

			t = bestfactor[p1]; a =(t&0xF); t = !!(t&0x10);
			b = p1/a+t;

			if (a>1) {
			    for(i=0; i<b; i++) *s++ = '+';

			    *s++ = '['; *s++ = '>';
			    while(a > 0) { *s++ = '+'; a--; c += b; }
			    *s++ = '<'; *s++ = '-'; *s++ = ']';
			}
			*s++ = '>';

			while(c < p1) { *s++ = '+'; c++; }
			while(c > p1) { *s++ = '-'; c--; }

			strcpy(s, extra_cell[p2]);
			s = codebuf + strlen(codebuf);

			for(i=0; i<p3; i++) *s++ = '+';
			*s++ = '>'; *s++ = ']';
			if (p4>0)
			for(i=0; i<p4; i++) *s++ = '+';
			if (p4<0)
			for(i=0; i>p4; i--) *s++ = '-';
			*s = 0;

			if (flg_init)
			    strcat(codebuf, ">[-]<");

			strcat(codebuf, "[<]>-]");

			sprintf(namebuf, "Counted Slope %d,%d,%d,%d%s",
				p1,p2,p3,p4,flg_init?" (i)":"");
			gen_special(linebuf, codebuf, namebuf, 0);

		    }
}

/******************************************************************************/

struct bfi { int mov; int cmd; int arg; } *pgm = 0;
int pgmlen = 0;

int
runbf(char * prog, int longrun)
{
    int m= 0, p= -1, n= -1, j= -1, ch;
    int maxcell = 0, countdown = (longrun?MAX_STEPS:10000);
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
	if (m<0 || m>=MAX_CELLS) return -2;
	if (--countdown == 0) return -1;
	if (m>maxcell) maxcell = m;

        switch(pgm[n].cmd)
        {
            case 0:     str_cells_used = maxcell+1;
			return m;
            case '+':   cells[m] += pgm[n].arg; break;
            case '[':   if (cells[m] == 0) n=pgm[n].arg; break;
            case ']':   if (cells[m] != 0) n=pgm[n].arg; break;
            case '>':   m += pgm[n].arg; break;
	    case '.':
		if (bf_print_off < (int)sizeof(bf_print_buf))
		    bf_print_buf[bf_print_off++] = cells[m];
		break;
        }

	if (bytewrap) cells[m] &= 255;
	else {
	    if (cells[m] > 255 ||
		(!flg_signed && cells[m] < 0) ||
		(flg_signed && cells[m] < -128))
		return -3;
	}
    }
}
