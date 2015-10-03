/*
 *
 * Robert de Bath (c) 2014,2015 GPL v2 or later.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <limits.h>
#if _POSIX_VERSION >= 199506L || defined(LLONG_MAX)
# include <inttypes.h>
#else
# define intmax_t long
# define PRIdMAX  "ld"
#endif

#define CELL int
#define MINOFF (0)
#define MAXOFF (10)
#define MINALLOC 1024
#define SAFE_CELL_MAX	((1<<30) -1)

CELL * mem = 0;
int memsize = 0;
int memshift = 0;

void run(void);
void print_summary(void);
void hex_output(FILE * ofd, int ch);

struct bfi { int cmd; int arg; } *pgm = 0;
int pgmlen = 0, on_eof = 1, debug = 0;

int physical_overflow = 0;
int physical_min = 0;
int physical_max = 255;
int quick_summary = 0;
int quick_with_counts = 0;
int cell_mask = 255;
int all_cells = 0;
int suppress_io = 0;

int nonl = 0;		/* Last thing printed was not an end of line. */
int tape_min = 0;	/* The lowest tape cell moved to. */
int tape_max = 0;	/* The highest tape cell moved to. */
int final_tape_pos = 0;	/* Where the tape pointer finished. */

intmax_t overflows =0;	/* Number of detected overflows. */
intmax_t underflows =0;	/* Number of detected underflows. */
int hard_wrap = 0;	/* Have cell values gone outside 0..255 ? */

char bf[] = "+-><[].,";
intmax_t profile[256*4];

int main(int argc, char **argv)
{
    FILE * ifd;
    int ch;
    int p = -1, n = -1, j = -1;

    for (;;) {
	if (argc < 2 || argv[1][0] != '-' || argv[1][1] == '\0') {
	    break;
	} else if (!strcmp(argv[1], "-e")) {
	    on_eof = -1; argc--; argv++;
	} else if (!strcmp(argv[1], "-z")) {
	    on_eof = 0; argc--; argv++;
	} else if (!strcmp(argv[1], "-n")) {
	    on_eof = 1; argc--; argv++;
	} else if (!strcmp(argv[1], "-N")) {
	    suppress_io = 1; argc--; argv++;
	} else if (!strcmp(argv[1], "-d")) {
	    debug = 1; argc--; argv++;
	} else if (!strcmp(argv[1], "-p")) {
	    physical_overflow=1; argc--; argv++;
	} else if (!strcmp(argv[1], "-q")) {
	    quick_summary=1; argc--; argv++;
	} else if (!strcmp(argv[1], "-Q")) {
	    quick_summary=1; quick_with_counts=1; argc--; argv++;
	} else if (!strcmp(argv[1], "-a")) {
	    all_cells++; argc--; argv++;
	} else if (!strcmp(argv[1], "-w")) {
	    cell_mask = (1<<16)-1;
	    physical_min = 0;
	    physical_max = cell_mask;
	    argc--; argv++;
	} else if (!strcmp(argv[1], "-sc")) {
	    cell_mask = (1<<8)-1;
	    physical_max = (cell_mask>>1);
	    physical_min = -1 - physical_max;
	    argc--; argv++;
	} else if (!strcmp(argv[1], "-12")) {
	    cell_mask = (1<<12)-1;
	    physical_min = 0;
	    physical_max = cell_mask;
	    argc--; argv++;
	} else if (argv[1][0] == '-') {
	    fprintf(stderr, "Unknown option '%s'\n", argv[1]);
	    exit(1);
	} else break;
    }

    ifd = argc > 1 && strcmp(argv[1], "-") ? fopen(argv[1], "r") : stdin;
    if(!ifd) {
	perror(argv[1]);
	return 1;
    }

    while((ch = getc(ifd)) != EOF && (ifd != stdin || ch != '!' || j >= 0)) {
	int r = (ch == '<' || ch == '>' || ch == '+' || ch == '-');
	if (r || (debug && ch == '#') || (ch == ']' && j >= 0) ||
	    ch == '[' || ch == ',' || ch == '.') {
	    if (r && p >= 0 && pgm[p].cmd == ch && pgm[p].arg < 128)
		{ pgm[p].arg += r; continue; }
	    n++;
	    if (n >= pgmlen-2) pgm = realloc(pgm, (pgmlen = n+99)*sizeof *pgm);
	    if (!pgm) { perror("realloc"); exit(1); }
	    pgm[n].cmd = ch; pgm[n].arg = r; p = n;
	    if (pgm[n].cmd == '[') { pgm[n].cmd = 0; pgm[n].arg = j; j = n; }
	    else if (pgm[n].cmd == ']') {
		pgm[n].arg = j; j = pgm[r = j].arg; pgm[r].arg = n; pgm[r].cmd = '[';
		if (pgm[n-1].cmd == '-' && pgm[n-1].arg == 1 &&
		    pgm[n-2].cmd == '[') {
		    n -= 2; pgm[p=n].cmd = '='; pgm[n].arg = 0;
		}
	    }
	}
    }
    if (ifd != stdin) fclose(ifd);
    setbuf(stdout, NULL);
    if (!pgm) {
	fprintf(stderr, "Empty program, everything is zero.\n");
	return 1;
    }

    pgm[n+1].cmd = 0;
    run();

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
    int n;
    m = alloc_ptr(MAXOFF)-MAXOFF;
    for(n = 0;; n++) {
	switch(pgm[n].cmd)
	{
	case 0:
	    final_tape_pos = m-memshift;
	    return;

	case '>':
	    profile[pgm[n].cmd*4] += pgm[n].arg;
	    m += pgm[n].arg;
	    if (m+MAXOFF >= memsize) m = alloc_ptr(m+MAXOFF)-MAXOFF;
	    if(tape_max < m-memshift) tape_max = m-memshift;
	    break;
	case '<':
	    profile[pgm[n].cmd*4] += pgm[n].arg;
	    m -= pgm[n].arg;
	    if (m+MINOFF <= 0) m = alloc_ptr(m+MINOFF)-MINOFF;
	    if(tape_min > m-memshift) {
		tape_min = m-memshift;
		if(tape_min < -1000) {
		    fprintf(stderr, "Tape underflow at pointer %d\n", tape_min);
		    exit(1);
		}
	    }
	    break;

	case '+':
	    profile[pgm[n].cmd*4] += pgm[n].arg;
	    mem[m] += pgm[n].arg;

	    if(physical_overflow) {
		if (mem[m] > physical_max) {
		    overflows++;
		    mem[m] -= (physical_max-physical_min+1);
		}
	    } else {
		if (mem[m] > physical_max) hard_wrap = 1;
		if (mem[m] > SAFE_CELL_MAX) {
		    /* Even if we're checking on '[' it's possible for our "int" cell to overflow; trap that here. */
		    overflows++;
		    mem[m] -= (SAFE_CELL_MAX+1);
		    profile[pgm[n].cmd*4 + 3]++;
		}
	    }
	    break;
	case '-':
	    profile[pgm[n].cmd*4] += pgm[n].arg;
	    mem[m] -= pgm[n].arg;

	    if(physical_overflow) {
		if (mem[m] < physical_min) {
		    underflows++;
		    mem[m] += (physical_max-physical_min+1);
		}
	    } else {
		if (mem[m] < physical_min) hard_wrap = 1;
		if (mem[m] < -SAFE_CELL_MAX) {
		    /* Even if we're checking on '[' it's possible for our "int" cell to overflow; trap that here. */
		    underflows++;
		    mem[m] += (SAFE_CELL_MAX+1);
		    profile[pgm[n].cmd*4 + 3]++;
		}
	    }
	    break;
	case '=':

	    if(physical_overflow) {
		if (mem[m] < 0) underflows++;
	    } else {
		if (mem[m] < 0) {
		    underflows++;
		    // profile[']'*4 + 3]++;
		}
	    }

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
		if ((mem[m] & cell_mask) == 0 && mem[m]) {
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
		if ((mem[m] & cell_mask) == 0 && mem[m]) {
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
	    { int a = (mem[m] & 0xFF);
	      if (!suppress_io) putchar(a);
	      if (a != 13) nonl = (a != '\n'); }
	    break;
	case ',':
	    profile[pgm[n].cmd*4+1 + !!mem[m]]++;

	    { int a = suppress_io?EOF:getchar();
	      if(a != EOF) mem[m] = a;
	      else if (on_eof != 1) mem[m] = on_eof; }
	    break;
	case '#':
	{
	    fprintf(stderr, "%2d %2d %2d %2d %2d %2d %2d %2d %2d %2d\n",
		    mem[0], mem[1], mem[2], mem[3], mem[4], mem[5],
		    mem[6], mem[7], mem[8], mem[9]);

	    if (m >= 0 && m < 10) fprintf(stderr, "%*s\n", 3*m+2, "^");
	}
	break;
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
	else
	    fprintf(stderr, "%c", pgm[n].cmd);
    }
}

void
print_summary()
{
    int program_len;			/* Number of BF command characters */
    intmax_t total_count = 0;

    {
	int i;
	for (i = 0; i < sizeof(bf); i++) {
	    total_count += profile[bf[i]*4]
			+  profile[bf[i]*4+1]
			+  profile[bf[i]*4+2];
	}
    }

    {
	int n;
	for(program_len = n = 0; pgm[n].cmd; n++) {
	    if (pgm[n].cmd == '>' || pgm[n].cmd == '<' ||
		pgm[n].cmd == '+' || pgm[n].cmd == '-' ) {
		program_len += pgm[n].arg;
	    } else if (pgm[n].cmd == '=')
		program_len += 3;
	    else
		program_len++;
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
	} else
	    fprintf(stderr, "Program size %d\n", program_len);
	fprintf(stderr, "Final tape contents:\n");

	{
	    int pw = 0, cc = 0, pc = 0;
	    {
		char buf[64];
		int i;
		i = sprintf(buf, "%d", physical_min);
		if (i > pw) pw = i;
		i = sprintf(buf, "%d", physical_max);
		if (i > pw) pw = i;
	    }

	    if (all_cells && cell_mask == 0xFF && tape_min == 0) {
		fprintf(stderr, "Pointer at: %d\n", final_tape_pos);
		for(ch = 0; ch <= tape_max-tape_min; ch++) {
		    hex_output(stderr, mem[ch+memshift] & cell_mask);
		}
		hex_output(stderr, EOF);
	    } else {
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
				  pw, mem[ch+memshift+tape_min] & cell_mask);
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
	fprintf(stderr, "Tape pointer maximum %d\n", tape_max);

	if (overflows || underflows) {
	    fprintf(stderr, "Range error: ");
	    if (physical_overflow)
		fprintf(stderr, "range %d..%d", physical_min, physical_max);
	    else
		fprintf(stderr, "value check");
	    if (overflows)
		fprintf(stderr, ", overflows: %"PRIdMAX"", overflows);
	    if (underflows)
		fprintf(stderr, ", underflows: %"PRIdMAX"", underflows);
	    fprintf(stderr, "\n");
	} else if (!physical_overflow && hard_wrap) {
	    fprintf(stderr, "Hard wrapping would occur for %s cells.\n",
		physical_min<0?"signed":"unsigned");
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

	for(n = 0; n < sizeof(bf)-1; n++) {
	    ch = bf[n];

	    if (n==0 || n==4)
		fprintf(stderr, "Counts:    ");

	    fprintf(stderr, " %c: %-12"PRIdMAX"", ch,
		    profile[ch*4]+ profile[ch*4+1]+ profile[ch*4+2]);

	    if (n == 3)
		fprintf(stderr, "\n");
	}
	fprintf(stderr, "\nTotal:         %-12"PRIdMAX"\n", total_count);
    }
    else
    {
	if (tape_min < -16) {
	    fprintf(stderr, "ERROR ");
	    print_pgm();
	    fprintf(stderr, "\n");
	} else {
	    int ch, nonwrap =
		(overflows == 0 && underflows == 0 &&
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
			nonwrap && hard_wrap ? " (soft)" : "");
	    else
		fprintf(stderr, " (%d, %d) %swrapping%s\n",
			program_len-tape_min, tape_max-tape_min+1,
			nonwrap ? "non-" : "",
			nonwrap && hard_wrap ? " (soft)" : "");
	}
    }
}

void
hex_output(FILE * ofd, int ch)
{
    static char lastbuf[80];
    static char linebuf[80];
    static char buf[20];
    static int pos = 0, addr = 0, lastmode = 0;

    if( ch == EOF ) {
	if(pos)
	    fprintf(ofd, "%06x:  %.66s\n", addr, linebuf);
	pos = 0;
	addr = 0;
	*lastbuf = 0;
	lastmode = 0;
    } else {
	if(!pos)
	    memset(linebuf, ' ', sizeof(linebuf));
	sprintf(buf, "%02x", ch&0xFF);
	memcpy(linebuf+pos*3+(pos > 7), buf, 2);

	if( ch > ' ' && ch <= '~' )
	    linebuf[50+pos] = ch;
	else linebuf[50+pos] = '.';
	pos = ((pos+1) & 0xF);
	if( pos == 0 ) {
	    linebuf[66] = 0;
	    if (*lastbuf && strcmp(linebuf, lastbuf) == 0) {
		if (!lastmode)
		    fprintf(ofd, "*\n");
		lastmode = 1;
	    } else {
		fprintf(ofd, "%06x:  %.66s\n", addr, linebuf);
		strcpy(lastbuf, linebuf);
	    }
	    addr += 16;
	}
    }
}
