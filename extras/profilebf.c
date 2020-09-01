/*
 *
 * Robert de Bath (c) 2014,2015 GPL v2 or later.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#include <limits.h>
#if _POSIX_VERSION >= 199506L || defined(LLONG_MAX)
# include <inttypes.h>
#endif

#include "ov_int.h"

#ifndef INTMAX_MAX
# define intmax_t long
# define PRIdMAX  "ld"
#endif

#define CELL int
#define MINOFF (0)
#define MAXOFF (10)
#define MINALLOC 1024

#define UM(vx) ((vx) & cell_mask)
#define SM(vx) ((UM(vx) ^ cell_smask) - cell_smask)

const char * progname = "";
CELL * mem = 0;
int memsize = 0;
int memshift = 0;

void run(void);
void optimise(void);
void print_summary(void);
void hex_output(FILE * ofd, int ch, int eof);
int ranged_mask(int);
void start_runclock(void);
void finish_runclock(double * prun_time, double *pwait_time);
void pause_runclock(void);
void unpause_runclock(void);
void oututf8(int input_chr);

int hex_bracket = -1;

struct bfi { int cmd; int arg; } *pgm = 0;
int pgmlen = 0, on_eof = 1, debug = 0;

int physical_overflow = 0;	/* Check overflows on +/- */
int physical_min = 0;
int physical_max = 255;
int quick_summary = 0;
int quick_with_counts = 0;
int cell_mask = 255;
int cell_smask = 0;
int all_cells = 0;
int suppress_io = 0;
int use_utf8 = 0;

int nonl = 0;		/* Last thing printed was not an end of line. */
int tape_min = 0;	/* The lowest tape cell moved to. */
int tape_max = 0;	/* The highest tape cell moved to. */
int final_tape_pos = 0;	/* Where the tape pointer finished. */

intmax_t overflows =0;	/* Number of detected overflows. */
intmax_t underflows =0;	/* Number of detected underflows. */
intmax_t neg_clear =0;	/* Number of detected underflows from [-]. */
int phy_limit = 0;	/* Did we hit an interpreter limit */
int hard_max = 0, hard_min = 0;
int do_optimise = 1;

char bf[] = "+-><[].,";
intmax_t profile[256*4];
int program_len;	/* Number of BF command characters */
int found_comment = 0;
int found_comment_loop = 0;
int eof_count = 0;

#ifdef SELFPROFILE
intmax_t optprofile[256];
#define inc_opt(_x) optprofile[_x]++;
#else
#define inc_opt(_x)
#endif

static struct timeval run_start, paused, run_pause;
static double run_time, wait_time;

int
option(char *arg)
{
    if (!strcmp(arg, "-e")) {
	on_eof = -1; return 1;
    } else if (!strcmp(arg, "-z")) {
	on_eof = 0; return 1;
    } else if (!strcmp(arg, "-n")) {
	on_eof = 1; return 1;
    } else if (!strcmp(arg, "-N")) {
	suppress_io = 1; return 1;
    } else if (!strcmp(arg, "-U")) {
	use_utf8 = 1; return 1;
    } else if (!strcmp(arg, "-d")) {
	debug = 1; return 1;
    } else if (!strcmp(arg, "-p")) {
	physical_overflow=1; return 1;
    } else if (!strcmp(arg, "-s")) {
	physical_min = -1; return 1;
    } else if (!strcmp(arg, "-u")) {
	physical_min = 0; return 1;
    } else if (!strcmp(arg, "-q")) {
	quick_summary=1; return 1;
    } else if (!strcmp(arg, "-Q")) {
	quick_summary=1; quick_with_counts=1; return 1;
    } else if (!strcmp(arg, "-a")) {
	all_cells++; return 1;
    } else if (!strcmp(arg, "-Z")) {
	do_optimise=0; return 1;
    } else if (!strcmp(arg, "-sc")) {
	cell_mask = (1<<8)-1;
	physical_min = -1;
	return 1;
    } else if (!strcmp(arg, "-W")) {
	cell_mask = (1<<16)-1;
	physical_min = -1;
	return 1;
    } else if (!strcmp(arg, "-w")) {
	cell_mask = -1;
	physical_min = -1;
	return 1;
    } else if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
	printf("Usage: %s [options] [file]\n", progname);
	puts("    Runs the brainfuck program provided.");
	puts("    Multiple statistics are output once the program completes.");

	puts(   "    Default operation uses 8 bit cells and no change on EOF."
	"\n"    "    The stats will display the number of logical overflows,"
	"\n"    "    these are overflows that alter the flow of control."
	"\n"
	"\n"    "Options:"
	"\n"    "    -e  Return EOF (-1) on end of file."
	"\n"    "    -z  Return zero on end of file."
	"\n"    "    -n  Do not change the current cell on end of file (Default)."
	"\n"    "    -N  Disable ALL I/O from the BF program; just output the stats."
	"\n"    "    -U  Translate output to UTF-8 sequences."
	"\n"    "    -d  Use the '#' command to output the current tape."
	"\n"    "    -p  Count physical overflows not logical ones."
	"\n"    "    -q  Output a quick summary of the run."
	"\n"    "    -Q  Output a quick summary of the run (variant)."
	"\n"    "    -a  Output all calls that have been used."
	"\n"    "    -sc Use 'signed character' (8bit) cells."
	"\n"    "    -s  Use signed cells."
	"\n"    "    -w  Use 32bit cells instead of 8 bit."
	"\n"    "    -7..16 Use unsigned 7..16 bit cells instead of 8 bit."
	);

	exit(1);
    }

    if((arg[1] >= '7' && arg[1] <= '9' && arg[2] == 0) ||
       (arg[1] == '1' && arg[2] >= '0' && arg[2] <= '6' && arg[3] == 0)) {

	cell_mask = (1<<atoi(arg+1))-1;
	return 1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    FILE * ifd;
    int ch, ar;
    int p = -1, n = -1, j = -1, lc = 0, inp = 0;
    char * datafile = 0;
    progname = argv[0];

    for(ar=1; ar<argc; ar++)
    {
	char * op, obuf[4];
	if (argv[ar][0] != '-' || !strcmp(argv[ar], "-")) {
	    if (datafile) {
		fprintf(stderr, "Only one file allowed\n");
		exit(1);
	    }
	    datafile = argv[ar];
	    continue;
	}
	if(option(argv[ar])) continue;

	for(op=argv[ar]+1; *op; op++) {
	    obuf[0] = '-';
	    obuf[1] = *op;
	    obuf[2] = 0;
	    if (!option(obuf)) {
		fprintf(stderr, "Unknown option '%s', try -h\n", argv[ar]);
		exit(1);
	    }
	}
    }

    if (cell_mask <= 0) {
	physical_max = INT_MAX;
	physical_min = INT_MIN;
	cell_smask = INT_MIN;
    } else if (physical_min == 0) {
	physical_max = cell_mask;
	physical_min = 0;
	cell_smask = 0;
    } else {
	physical_max = (cell_mask>>1);
	physical_min = -1 - physical_max;
	cell_smask = (cell_mask ^ (cell_mask>>1));
    }

    ifd = datafile && strcmp(datafile, "-") ? fopen(datafile, "r") : stdin;
    if(!ifd) {
	perror(datafile);
	return 1;
    }

    /*
     *	For each character, if it's a valid BF command add it onto the
     *	end of the program. If the input is stdin use the '!' character
     *	to mark the end of the program and the start of the data, but
     *	only if we have a complete program.  The 'j' variable points
     *	at the list of currently open '[' commands, one is matched off
     *	by each ']'.  A ']' without a matching '[' is not a legal BF
     *	command and so is ignored.  If there are any '[' commands left
     *	over at the end they are not valid BF commands and so are ignored.
     *
     *  A run of any of the commands "<>+-" is converted into a single
     *  entry in the pgm array. This run is limited to 128 items of the
     *  same type so the counts will be correct.
     *
     *  The sequence "[-]" is detected and converted into a single token.
     *  If the debug flag is set the '#' command is also included.
     */

    while((ch = getc(ifd)) != EOF && (ifd != stdin || ch != '!' || !inp || j >= 0)) {
	int r = (ch == '<' || ch == '>' || ch == '+' || ch == '-');
	if (r || (debug && ch == '#') || (ch == ']' && (j >= 0 || lc)) ||
	    ch == '[' || ch == ',' || ch == '.') {
	    if (r && p >= 0 && pgm[p].cmd == ch) {
		if (pgm[p].arg < 128 && (ch == '-' || ch == '+'))
		    { pgm[p].arg += r; continue; }
		if (pgm[p].arg < 65536 && (ch == '<' || ch == '>'))
		    { pgm[p].arg += r; continue; }
	    }
	    if (lc || (ch == '[' && n == -1)) {
		found_comment++;
		if (ch == '[' && !lc) found_comment_loop++;
		lc += (ch=='[') - (ch==']');
		continue;
	    }
	    n++;
	    if (n >= pgmlen-2) pgm = realloc(pgm, (pgmlen = n+99)*sizeof *pgm);
	    if (!pgm) { perror("realloc"); exit(1); }
	    pgm[n].cmd = ch; pgm[n].arg = r; p = n;
	    if (pgm[n].cmd == '[') { pgm[n].cmd = ' '; pgm[n].arg = j; j = n; }
	    else if (pgm[n].cmd == ']') {
		pgm[n].arg = j; j = pgm[r = j].arg; pgm[r].arg = n; pgm[r].cmd = '[';
		if (pgm[n-1].cmd == '-' && pgm[n-1].arg == 1 &&
		    pgm[n-2].cmd == '[') {
		    n -= 2; pgm[p=n].cmd = '='; pgm[n].arg = 0;
		}
	    } else if (pgm[n].cmd == ',') inp = 1;
	}
    }
    if (ifd != stdin) fclose(ifd);
    if (isatty(fileno(stdout))) setbuf(stdout, NULL);
    if (!pgm) {
	fprintf(stderr, "Empty program, everything is zero.\n");
	return 1;
    }

    pgm[n+1].cmd = 0;

    {
	int i;
	for(program_len = i = 0; pgm[i].cmd; i++) {
	    if (pgm[i].cmd == '>' || pgm[i].cmd == '<' ||
		pgm[i].cmd == '+' || pgm[i].cmd == '-' ) {
		program_len += pgm[i].arg;
	    } else if (pgm[i].cmd == '=')
		program_len += 3;
	    else if(pgm[i].cmd != ' ')
		program_len++;
	}
    }

    if (do_optimise) optimise();

    start_runclock();

    run();

    finish_runclock(&run_time, &wait_time);

    fflush(stdout);

    print_summary();
    return 0;
}

int
alloc_ptr(int memoff)
{
    int amt, i, off;
    if (memoff >= 0 && memoff < memsize) return memoff;

    off = 0;
    if (memoff<0) off = -memoff;
    else if(memoff>=memsize) off = memoff-memsize;
    amt = (off / MINALLOC + 1) * MINALLOC;
    mem = realloc(mem, (memsize+amt)*sizeof(*mem));
    if (mem == 0) {
	fprintf(stderr, "memory allocation failure for %d cells\n", memsize+amt);
	exit(1);
    }
    if (memoff<0) {
        memmove(mem+amt, mem, memsize*sizeof(*mem));
        for(i=0; i<amt; i++)
            mem[i] = 0;
        memoff += amt;
	memshift += amt;
    } else {
        for(i=0; i<amt; i++)
            mem[memsize+i] = 0;
    }
    memsize += amt;
    return memoff;
}

void run(void)
{
    int m = 0;
    int n, v = 1;
    m = alloc_ptr(MAXOFF)-MAXOFF;
    for(n = 0;; n++) {
	inc_opt(pgm[n].cmd);
	switch(pgm[n].cmd)
	{
	case 0:
	    final_tape_pos = m-memshift;
	    return;

	case '>':
	    profile[pgm[n].cmd*4] += pgm[n].arg;
	    m += pgm[n].arg;
	    if (m+MAXOFF >= memsize) m = alloc_ptr(m+MAXOFF)-MAXOFF;
	    if (tape_max < m-memshift) tape_max = m-memshift;
	    break;

	case 'R':
	    profile['>'*4] += pgm[n].arg*(intmax_t)(unsigned)v;
	    m += pgm[n].arg;
	    if (m+MAXOFF >= memsize) m = alloc_ptr(m+MAXOFF)-MAXOFF;
	    if (tape_max < m-memshift) tape_max = m-memshift;
	    break;

	case '<':
	    profile[pgm[n].cmd*4] += pgm[n].arg;
	    m -= pgm[n].arg;
	    if (m+MINOFF <= 0) m = alloc_ptr(m+MINOFF)-MINOFF;
	    if (tape_min > m-memshift) {
		tape_min = m-memshift;
		if(tape_min < -1000) {
		    fprintf(stderr, "Tape underflow at pointer %d\n", tape_min);
		    exit(1);
		}
	    }
	    break;

	case 'L':
	    profile['<'*4] += pgm[n].arg*(intmax_t)(unsigned)v;
	    m -= pgm[n].arg;
	    if (m+MINOFF <= 0) m = alloc_ptr(m+MINOFF)-MINOFF;
	    if (tape_min > m-memshift) {
		tape_min = m-memshift;
		if(tape_min < -1000) {
		    fprintf(stderr, "Tape underflow at pointer %d\n", tape_min);
		    exit(1);
		}
	    }
	    break;

	case '+':
	    {
		int ov=0;
		profile[pgm[n].cmd*4] += pgm[n].arg;
		mem[m] = ov_iadd(mem[m], pgm[n].arg, &ov);

		if (ov) {
		    overflows++;
		    phy_limit = 1;
		    profile[pgm[n].cmd*4 + 3]++;
		} else {
		    if (mem[m] > hard_max) {
			if(physical_overflow && mem[m] > physical_max) {
			    overflows++;
			    mem[m] -= (physical_max-physical_min+1);
			}
			hard_max = mem[m];
		    }
		}
	    }
	    break;

	case '-':
	    {
		int ov = 0;
		profile[pgm[n].cmd*4] += pgm[n].arg;
		mem[m] = ov_isub(mem[m], pgm[n].arg, &ov);

		if (ov) {
		    underflows++;
		    phy_limit = 1;
		    profile[pgm[n].cmd*4 + 3]++;
		} else {
		    if (mem[m] < hard_min) {
			if(physical_overflow && mem[m] < physical_min) {
			    underflows++;
			    mem[m] += (physical_max-physical_min+1);
			}
			hard_min = mem[m];
		    }
		}
	    }
	    break;

	case '=':
	    if (mem[m] < 0) neg_clear++;

	    if (mem[m] < 0) {
		profile['='*4 + 3]++;
		profile['='*4] ++;
	    } else if (mem[m] > 0)
		profile['='*4 + 2]++;
	    else
		profile['='*4 + 1]++;

	    mem[m] &= cell_mask;

	    if (mem[m]) {
		profile['['*4 + 2] ++;
		profile['-'*4 + 0] += mem[m];
		profile[']'*4 + 1] ++;
		profile[']'*4 + 2] += mem[m]-1;
	    } else
		profile['['*4 + 1]++;

	    mem[m] = 0;
	    break;

	case '[':
	    if (!physical_overflow) {
		if (mem[m] && (mem[m] & cell_mask) == 0) {
		    /* This condition will be different. */
		    if (mem[m] < 0) underflows++; else overflows++;
		    profile[pgm[n].cmd*4 + 3]++;

		    mem[m] &= cell_mask;
		    if (mem[m] > physical_max)
			mem[m] -= (physical_max-physical_min+1);
		}
	    }

	    profile[pgm[n].cmd*4+1 + !!mem[m]]++;
	    if (mem[m] == 0) n = pgm[n].arg;
	    break;

	case ']':
	    if (!physical_overflow) {
		if (mem[m] && (mem[m] & cell_mask) == 0) {
		    /* This condition will be different. */
		    if (mem[m] < 0) underflows++; else overflows++;
		    profile[pgm[n].cmd*4 + 3]++;

		    mem[m] &= cell_mask;
		    if (mem[m] > physical_max)
			mem[m] -= (physical_max-physical_min+1);
		}
	    }

	    profile[pgm[n].cmd*4+1 + !!mem[m]]++;
	    if (mem[m] != 0) n = pgm[n].arg;
	    break;

	case '.':
	    profile[pgm[n].cmd*4]++;
	    {
		int a = SM(mem[m]);
		if (!suppress_io) {
		    if (use_utf8) {
			if (cell_mask == 0xFFFF) a = UM(a); /* Assume UTF16 */
			oututf8(a);
		    } else putchar(a);
		}
		if (a != 13) {
		    if (use_utf8)
			nonl = (a != '\n');
		    else
			nonl = ((a&0xFF) != '\n');
		}
	    }
	    break;

	case ',':
	    profile[pgm[n].cmd*4+1 + !!mem[m]]++;

	    pause_runclock();
	    { int a = suppress_io?EOF:getchar();
	      if(a != EOF) mem[m] = a;
	      else {
		eof_count++;
		if (eof_count > 100) return;
		if (on_eof != 1) mem[m] = on_eof;
	      }
	    }

	    unpause_runclock();
	    break;

	case '#':
	    if (all_cells && cell_mask == 0xFF) {
		int a;
		fprintf(stderr, "Debug dump ->\n");
		hex_bracket = m;
		for(a = 0; a <= tape_max-tape_min; a++)
		    hex_output(stderr, ranged_mask(mem[a]), 0);
		hex_output(stderr, 0, 1);
		fprintf(stderr, "\n");
	    } else {
		int a;
		for (a=0; a<10 && a <= tape_max-tape_min; a++)
		    fprintf(stderr, "%c%-5d", a==m?'>':' ', mem[a]);
		fprintf(stderr, "\n");
	    }
	    break;

	case 'B':
	    if (!physical_overflow) {
		if (mem[m] && (mem[m] & cell_mask) == 0) {
		    /* This condition will be different. */
		    if (mem[m] < 0) underflows++; else overflows++;
		    profile['['*4 + 3]++;

		    mem[m] &= cell_mask;
		    if (mem[m] > physical_max)
			mem[m] -= (physical_max-physical_min+1);
		}
	    }

	    profile['['*4+1 + !!mem[m]]++;
	    if (mem[m] == 0){
		n = pgm[n].arg;
		v = 1;
	    } else
		v = (mem[m] & cell_mask);
	    break;

	case 'E':
	    profile[']'*4 + 1] ++;
	    profile[']'*4 + 2] += v-1;
	    v = 1;
	    break;

	case 'M':
	    {
		int ov = 0;
		profile['+'*4] += pgm[n].arg*(intmax_t)(unsigned)v;
		mem[m] = ov_iadd(mem[m], ov_imul(pgm[n].arg, v, &ov), &ov);

		if (ov) {
		    overflows++;
		    phy_limit = 1;
		    profile['+'*4 + 3]++;
		} else
		    if(physical_overflow) {
			while(mem[m] > physical_max) {
			    overflows++;
			    mem[m] -= (physical_max-physical_min+1);
			}
		    }
		if (mem[m] > hard_max) hard_max = mem[m];
	    }
	    break;

	case 'N':
	    {
		int ov = 0;
		profile['-'*4] += pgm[n].arg*(intmax_t)(unsigned)v;
		mem[m] = ov_isub(mem[m], ov_imul(pgm[n].arg, v, &ov), &ov);

		if (ov) {
		    underflows++;
		    phy_limit = 1;
		    profile['-'*4 + 3]++;
		} else
		    if(physical_overflow) {
			while (mem[m] < physical_min) {
			    underflows++;
			    mem[m] += (physical_max-physical_min+1);
			}
		    }
		if (mem[m] > hard_max) hard_max = mem[m];
	    }
	    break;

	case '{':
	    {
		int m0;

		if (!physical_overflow) {
		    if ((mem[m] & cell_mask) == 0 && mem[m]) {
			/* This condition will be different. */
			if (mem[m] < 0) underflows++; else overflows++;
			profile['['*4 + 3]++;

			mem[m] &= cell_mask;
			if (mem[m] > physical_max)
			    mem[m] -= (physical_max-physical_min+1);
		    }
		}

		profile['['*4+1 + !!mem[m]]++;
		if (mem[m] == 0) { n = pgm[n].arg; break; }

		n++;
		m0 = m;
		do
		{
		    if (pgm[n].cmd == '>' ) {
			m += pgm[n].arg;
			if (m+MAXOFF >= memsize) m = alloc_ptr(m+MAXOFF)-MAXOFF;
			if(tape_max < m-memshift) tape_max = m-memshift;
		    } else {
			m -= pgm[n].arg;
			if (m+MINOFF <= 0) m = alloc_ptr(m+MINOFF)-MINOFF;
			if(tape_min > m-memshift) {
			    tape_min = m-memshift;
			    if(tape_min < -1000) {
				fprintf(stderr, "Tape underflow at pointer %d\n", tape_min);
				exit(1);
			    }
			}
		    }

		    if (!physical_overflow) {
			if (mem[m] && (mem[m] & cell_mask) == 0) {
			    /* This condition will be different. */
			    if (mem[m] < 0) underflows++; else overflows++;
			    profile[']'*4 + 3]++;

			    mem[m] &= cell_mask;
			    if (mem[m] > physical_max)
				mem[m] -= (physical_max-physical_min+1);
			}
		    }

		} while(mem[m] != 0);

		m0 = abs(m0-m);
		profile[pgm[n].cmd*4] += m0;
		profile[']'*4+1 ]++;
		profile[']'*4+1 + 1] += (m0/pgm[n].arg -1);
		n++;

	    }
	    break;

	}
    }
}

void optimise(void)
{
    // +++++ [  -  >  +++  >  ++++  <<  ]
    // 5+    [ 1- 1>  3+  1>  4+    2<  ]
    // 5+    B    1>  3M  1>  4M    2<  Z
    // 5i    1    1*B 3*B 1*B 4*B   2*B 2*B

    int n, lastopen = 0, balance = 0, decfound = 0;

    for(n = 0; pgm[n].cmd; n++) {
	// > < + - = [ ] . , #
	if (pgm[n].cmd == '[') {
	    lastopen = n;
	    decfound = balance = 0;
	} else if (pgm[n].cmd == '>') {
	    balance += pgm[n].arg;
	} else if (pgm[n].cmd == '<') {
	    balance -= pgm[n].arg;
	} else if (pgm[n].cmd == '+' || pgm[n].cmd == '-') {
	    if (balance == 0) {
		if (pgm[n].cmd == '-' && pgm[n].arg == 1) {
			if (decfound) lastopen = 0;
			else decfound = 1;
		} else
		    lastopen = 0;
	    }
	} else if (pgm[n].cmd == ']' && lastopen && balance == 0 && decfound) {
	    /* We have a balanced loop with simple contents */
	    int i;
	    for(i=lastopen; i<n; i++) {
		if (pgm[i].cmd == '[') pgm[i].cmd = 'B';
		else if (pgm[i].cmd == '+') pgm[i].cmd = 'M';
		else if (pgm[i].cmd == '-') pgm[i].cmd = 'N';
		else if (pgm[i].cmd == '>') pgm[i].cmd = 'R';
		else if (pgm[i].cmd == '<') pgm[i].cmd = 'L';
	    }
	    pgm[n].cmd = 'E';
	} else
	    lastopen = 0;

	if (pgm[n].cmd == ']' && (pgm[n-1].cmd == '>' || pgm[n-1].cmd == '<')
	    && pgm[n-2].cmd == '[') {
	    pgm[n-2].cmd = '{';
	    pgm[n].cmd = '}';
	}
    }
}

void
print_pgm()
{
    int n;
    for(n = 0; pgm[n].cmd; n++) {
	if (pgm[n].cmd == '>' || pgm[n].cmd == '<' ||
	    pgm[n].cmd == '+' || pgm[n].cmd == '-' ) {

	    int i;
	    for(i = 0; i < pgm[n].arg; i++)
		fprintf(stderr, "%c", pgm[n].cmd);
	} else if (pgm[n].cmd == '=')
	    fprintf(stderr, "[-]");
	else if (pgm[n].cmd == 'M' || pgm[n].cmd == 'N' ||
	         pgm[n].cmd == 'R' || pgm[n].cmd == 'L') {
	    int i;
	    for(i = 0; i < pgm[n].arg; i++)
		fprintf(stderr, "%c",
			pgm[n].cmd=='M'?'+':
			pgm[n].cmd=='N'?'-':
			pgm[n].cmd=='R'?'>':
			'<');
	}
	else if (pgm[n].cmd == 'B' || pgm[n].cmd == '{') fprintf(stderr, "[");
	else if (pgm[n].cmd == 'E' || pgm[n].cmd == '}') fprintf(stderr, "]");
	else if (pgm[n].cmd != ' ') fprintf(stderr, "%c", pgm[n].cmd);
    }
}

void
print_summary()
{
    intmax_t total_count = 0;

    {
	int i;
	for (i = 0; i < (int)sizeof(bf); i++) {
	    total_count += profile[bf[i]*4]
			+  profile[bf[i]*4+1]
			+  profile[bf[i]*4+2];
	}
    }

    if (!quick_summary)
    {
	int ch, n;

	if (nonl && !suppress_io)
	    fprintf(stderr, "\n\\ no newline at end of output.\n");
	else if (nonl)
	    fprintf(stderr, "No newline at end of output.\n");

	if (program_len <= 60) {
	    fprintf(stderr, "Program size %d : ", program_len);
	    print_pgm();
	    fprintf(stderr, "\n");
	} else if (found_comment_loop == 1)
	    fprintf(stderr, "Program size %d (+%d initial comment)\n",
			    program_len, found_comment);
	else if (found_comment)
	    fprintf(stderr, "Program size %d (+%d/%d initial comment)\n",
			    program_len, found_comment, found_comment_loop);
	else
	    fprintf(stderr, "Program size %d\n", program_len);
	fprintf(stderr, "Final tape contents:\n");

	{
	    int cc = 0, pc = 0, hex_ok = 0;

	    if (all_cells && cell_mask != 0xFF && tape_min == 0) {
		hex_ok = 1;
		for(ch = 0; ch <= tape_max-tape_min; ch++) {
		    int v = ranged_mask(mem[ch+memshift]);
		    if (v < -9 || v > 255) {
			hex_ok = 0;
			break;
		    }
		}
	    }

	    if (all_cells && (cell_mask == 0xFF || hex_ok) && tape_min == 0) {
		fprintf(stderr, "Pointer at: %d\n", final_tape_pos);
		hex_bracket = final_tape_pos;
		for(ch = 0; ch <= tape_max-tape_min; ch++)
		    hex_output(stderr, ranged_mask(mem[ch+memshift]), 0);
		hex_output(stderr, 0, 1);
	    } else {
		char buf[sizeof(int)*3+4];
		int i, pw = 2;

		for(ch = 0; (all_cells || ch < 16) && ch <= tape_max-tape_min; ch++) {
		    i = sprintf(buf, "%d", ranged_mask(mem[ch+memshift+tape_min]));
		    if (i > pw) pw = i;
		}

		if (tape_min < 0) cc += fprintf(stderr, " !");

		for(ch = 0; (all_cells || ch < 16) && ch <= tape_max-tape_min; ch++) {
		    if (((ch+tape_min)&15) == 0) {
			if (ch+tape_min != 0) {
			    if (pc) fprintf(stderr, "\n%*s\n", pc, "^");
			    else fprintf(stderr, "\n");
			    cc = pc = 0;
			}
			cc += fprintf(stderr, " :");
		    }
		    cc += fprintf(stderr, " %*d",
				  pw, ranged_mask(mem[ch+memshift+tape_min]));
		    if (final_tape_pos == ch+tape_min)
			pc = cc;
		}
		if (!all_cells && tape_max-tape_min >= 16) fprintf(stderr, " ...");
		if (pc) fprintf(stderr, "\n%*s\n", pc, "^");
		else fprintf(stderr, "\nPointer at: %d\n", final_tape_pos);
	    }
	}

	if (tape_min < 0)
	    fprintf(stderr, "WARNING: Tape pointer minimum %d, segfault.\n", tape_min);
	fprintf(stderr, "Tape pointer maximum %d", tape_max);
	if (!physical_overflow && !overflows && !underflows && !neg_clear)
	    fprintf(stderr, ", cell value range %d..%d", hard_min, hard_max);
	fprintf(stderr, "\n");

	if (overflows || underflows || neg_clear) {
	    fprintf(stderr, "Range error: ");
	    if (physical_overflow)
		fprintf(stderr, "range %d..%d", physical_min, physical_max);
	    else if(phy_limit)
		fprintf(stderr, "physical overflow");
	    else
		fprintf(stderr, "value check");
	    if (overflows)
		fprintf(stderr, ", overflows: %"PRIdMAX"", overflows);
	    if (underflows || neg_clear)
		fprintf(stderr, ", underflows: %"PRIdMAX"",
		    underflows + neg_clear);
	    fprintf(stderr, "\n");
	}

	if (!physical_overflow) {
	    if (profile['+'*4+3]+profile['-'*4+3])
		fprintf(stderr, "Physical overflows '+' %"PRIdMAX", '-' %"PRIdMAX"\n",
		    profile['+'*4+3], profile['-'*4+3]);
	}

	if (profile['['*4+3]+profile[']'*4+3])
	    fprintf(stderr, "Program logic effects '['->%"PRIdMAX", ']'->%"PRIdMAX"\n",
		profile['['*4+3], profile[']'*4+3]);

	if (profile['='*4 + 3])
	    fprintf(stderr, "Sequence '[-]' on negative cell: %"PRIdMAX"\n",
		    profile['='*4 + 3]);

	if (profile['['*4+1])
	    fprintf(stderr, "Skipped loops (zero on '['): %"PRIdMAX"\n",
		profile['['*4+1]);

	if (eof_count>100)
	    fprintf(stderr, "ERROR: Program aborted because too many "
			    "EOFs ignored. Try using -z option?\n");
	else if (eof_count>1)
	    fprintf(stderr, "Input command read EOF %d times\n", eof_count);
	else if (eof_count == 1)
	    fprintf(stderr, "Input command read EOF\n");

	for(n = 0; n < (int)sizeof(bf)-1; n++) {
	    ch = bf[n];

	    if (n==0 || n==4)
		fprintf(stderr, "Counts:    ");

	    fprintf(stderr, " %c: %-12"PRIdMAX"", ch,
		    profile[ch*4]+ profile[ch*4+1]+ profile[ch*4+2]);

	    if (n == 3)
		fprintf(stderr, "\n");
	}
	fprintf(stderr, "\nTotal:         %-12"PRIdMAX"", total_count);

	if (run_time != 0.0 ) {
	    fprintf(stderr, " run: %0.3fs", run_time);
	    if (wait_time != 0.0)
		fprintf(stderr, ", wait: %0.3fs", wait_time);
	}
	fprintf(stderr, "\n");
    }
    else
    {
	if (tape_min < -16) {
	    fprintf(stderr, "ERROR ");
	    print_pgm();
	    fprintf(stderr, "\n");
	} else {
	    int ch, nonwrap =
		(overflows == 0 && underflows == 0 && neg_clear == 0 &&
		 (mem[final_tape_pos+memshift] & ~cell_mask) == 0);

	    fprintf(stderr, "%d ", (mem[final_tape_pos+memshift] & cell_mask) );
	    fprintf(stderr, "%d ", program_len-tape_min);
	    fprintf(stderr, "%d ", tape_max-tape_min+1);
	    fprintf(stderr, "%"PRIdMAX" ", total_count);
	    for(ch = tape_min; ch < 0; ch++)
		fprintf(stderr, ">");
	    print_pgm();

	    if (quick_with_counts)
		fprintf(stderr, " (%d, %d, %"PRIdMAX") %swrapping%s\n",
			program_len-tape_min, tape_max-tape_min+1,
			total_count-tape_min,
			nonwrap ? "non-" : "",
			nonwrap && (overflows||underflows) ? " (soft)" : "");
	    else
		fprintf(stderr, " (%d, %d) %swrapping%s\n",
			program_len-tape_min, tape_max-tape_min+1,
			nonwrap ? "non-" : "",
			nonwrap && (overflows||underflows) ? " (soft)" : "");
	}
    }

#ifdef SELFPROFILE
    for(int i=0; i<256; i++) {
	if(optprofile[i])
	    if(i>' ' && i<='~')
		printf("cmd('%c') = %"PRIdMAX"\n", i, optprofile[i]);
	    else
		printf("cmd('\\%03o') = %"PRIdMAX"\n", i, optprofile[i]);
    }
#endif
}

void
hex_output(FILE * ofd, int ch, int eof)
{
    static char lastbuf[80];
    static char linebuf[80];
    static char buf[20];
    static int pos = 0, addr = 0, lastmode = 0;

    if( eof ) {
	if(pos)
	    fprintf(ofd, "%06x: %.67s\n", addr, linebuf);
	pos = 0;
	addr = 0;
	*lastbuf = 0;
	lastmode = 0;
	hex_bracket = -1;
    } else {
	if(!pos)
	    memset(linebuf, ' ', sizeof(linebuf));
	if (ch < 0 && ch >= -9)
	    sprintf(buf, "%2d", ch);
	else
	    sprintf(buf, "%02x", ch & 0xFF);
	memcpy(linebuf+pos*3+(pos > 7)+1, buf, 2);
	if (addr+pos == hex_bracket) {
	    linebuf[pos*3+(pos > 7)] = '(';
	    linebuf[pos*3+(pos > 7)+3] = ')';
	    hex_bracket = 0;
	}

	if( ch > ' ' && ch <= '~' )
	    linebuf[51+pos] = ch;
	else linebuf[51+pos] = '.';
	pos = ((pos+1) & 0xF);
	if( pos == 0 ) {
	    linebuf[67] = 0;
	    if (*lastbuf && strcmp(linebuf, lastbuf) == 0) {
		if (!lastmode)
		    fprintf(ofd, "*\n");
		lastmode = 1;
	    } else {
		lastmode = 0;
		fprintf(ofd, "%06x: %.67s\n", addr, linebuf);
		strcpy(lastbuf, linebuf);
	    }
	    addr += 16;
	}
    }
}

int
ranged_mask(int v)
{
    if (physical_overflow) return SM(v);
    if (cell_mask <= 0) return v;
    if (v>= -cell_mask/2 && v<=cell_mask)
	return v;
    return SM(v);
}

void
start_runclock(void)
{
    gettimeofday(&run_start, 0);
    paused.tv_sec = 0;
    paused.tv_usec = 0;
}

void
finish_runclock(double * prun_time, double *pwait_time)
{
    struct timeval run_end;
    gettimeofday(&run_end, 0);

    run_end.tv_sec -= run_start.tv_sec;
    run_end.tv_usec -= run_start.tv_usec;
    if (run_end.tv_usec < 0)
        { run_end.tv_usec += 1000000; run_end.tv_sec -= 1; }

    run_end.tv_sec -= paused.tv_sec;
    run_end.tv_usec -= paused.tv_usec;
    if (run_end.tv_usec < 0)
        { run_end.tv_usec += 1000000; run_end.tv_sec -= 1; }

    if (prun_time)
        *prun_time = (run_end.tv_sec + run_end.tv_usec/1000000.0);
    if (pwait_time)
        *pwait_time = (paused.tv_sec + paused.tv_usec/1000000.0);
}

void
pause_runclock(void)
{
    gettimeofday(&run_pause, 0);
}

void
unpause_runclock(void)
{
    struct timeval run_end;

    gettimeofday(&run_end, 0);
    paused.tv_sec += run_end.tv_sec - run_pause.tv_sec;
    paused.tv_usec += run_end.tv_usec - run_pause.tv_usec;

    if (paused.tv_usec < 0)
        { paused.tv_usec += 1000000; paused.tv_sec -= 1; }
    if (paused.tv_usec >= 1000000)
        { paused.tv_usec -= 1000000; paused.tv_sec += 1; }
}

/* Output a UTF-8 codepoint. */
void
oututf8(int input_chr)
{
static int pending_hi = 0; /* UTF-16 conversion for 16-bit */
    if ( (input_chr & ~0x3FF) == 0xD800) {
	if (pending_hi) oututf8(0xFFFD);
	pending_hi = input_chr;
	return;
    }
    if ( (input_chr & ~0x3FF) == 0xDC00) {
	if (pending_hi)
	    input_chr = 0x10000 + ((pending_hi&0x3FF)<<10) + (input_chr&0x3FF);
	pending_hi = 0;
    }
    if (input_chr >= 0xD800 && input_chr <= 0xDFFF)
	input_chr = 0xFFFD;

    if (input_chr < 0x80) {            /* one-byte character */
        putchar(input_chr);
    } else if (input_chr < 0x800) {    /* two-byte character */
        putchar(0xC0 | (0x1F & (input_chr >>  6)));
        putchar(0x80 | (0x3F & (input_chr      )));
    } else if (input_chr < 0x10000) {  /* three-byte character */
        putchar(0xE0 | (0x0F & (input_chr >> 12)));
        putchar(0x80 | (0x3F & (input_chr >>  6)));
        putchar(0x80 | (0x3F & (input_chr      )));
    } else if (input_chr < 0x200000) { /* four-byte character */
        putchar(0xF0 | (0x07 & (input_chr >> 18)));
        putchar(0x80 | (0x3F & (input_chr >> 12)));
        putchar(0x80 | (0x3F & (input_chr >>  6)));
        putchar(0x80 | (0x3F & (input_chr      )));
    } else if (input_chr < 0x4000000) {/* five-byte character */
        putchar(0xF8 | (0x03 & (input_chr >> 24)));
        putchar(0x80 | (0x3F & (input_chr >> 18)));
        putchar(0x80 | (0x3F & (input_chr >> 12)));
        putchar(0x80 | (0x3F & (input_chr >>  6)));
        putchar(0x80 | (0x3F & (input_chr      )));
    } else {                           /* six-byte character */
        putchar(0xFC | (0x01 & (input_chr >> 30)));
        putchar(0x80 | (0x3F & (input_chr >> 24)));
        putchar(0x80 | (0x3F & (input_chr >> 18)));
        putchar(0x80 | (0x3F & (input_chr >> 12)));
        putchar(0x80 | (0x3F & (input_chr >>  6)));
        putchar(0x80 | (0x3F & (input_chr      )));
    }
}
