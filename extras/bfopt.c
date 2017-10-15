#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Brainfuck to brainfuck optimiser.
 *
 * This program loads BF code with a simple optimiser. It then puts it through
 * an interpreter without loops. The partial tapes created are then converted
 * back to optimised BF and chained with the loop commands.
 *
 * The peephole optimisations it does are:
 *
 * RLE of '+', '-', '>', '<'.
 * Remove loop comments; ie: ][ comment ]
 * Change '[-]' and '[+]' to single instructions.
 * Strings like '[-]+++++++' are changed to a set token.
 *
 * With just use of the commands "<>+-" the optimal pattern to create is
 * pretty simple. The "[-]" sequence doesn't alter this significantly.
 * But when you add the "." and "," commands calculation of the best route
 * becomes significantly harder. The "delay_clean" option highlights the
 * main choice to be made here, in addition there's the possibility of
 * cleaning only the cells you go past.
 *
 * NOTE: This assumes that the cells are 32 bit or less binary cells.
 *       However, you're unlikely to trigger the limitation.
 *
 * This program has been extracted from the bf2any brainfuck interpreter.
 */

const char * current_file;
int enable_debug = 0;
int enabled_rle = 0;
int enable_initopt = 1;
int enable_opt = 1;
int delay_clean = 0;
int maxcol=72;

void outrun(int ch, int count);
void outopt(int ch, int count);
void outcmd(int ch, int count);

/*
 *  Decode arguments.
 */
int enable_rle = 0;

int
check_argv(const char * arg)
{
    if (strcmp(arg, "-#") == 0) {
	enable_debug++;
    } else if (strcmp(arg, "-R") == 0) {
	enable_rle++;
    } else if (strcmp(arg, "-O0") == 0 || strcmp(arg, "-m") == 0) {
	enable_opt = enable_initopt = 0;
    } else if (strcmp(arg, "-O1") == 0 || strcmp(arg, "-p") == 0) {
	enable_initopt=0;
    } else if (strcmp(arg, "-O2") == 0) {
	enable_opt = enable_initopt = 1;
    } else if (strcmp(arg, "-d") == 0) {
	delay_clean=1;
    } else if (strcmp(arg, "-w") == 0) {
	maxcol=0;
    } else if (strncmp(arg, "-w", 2) == 0 && arg[2] >= '0' && arg[2] <= '9') {
        maxcol = atol(arg+2);

    } else if (strcmp(arg, "-h") == 0) {
	return 0;
    }
    else return 0;
    return 1;
}

/*
 * This main function is quite high density, it's functions are:
 *  *) Open and read the BF file.
 *  *) Decode the BF into calls of the outrun() function.
 *  *) Run length encode calls to the outrun() function.
 *  *) Ensure that '[', ']' commands balance.
 *  *) Allow the '#' command only if enabled.
 *  *) If enabled; decode input RLE (a number prefix on the "+-<>" commands)
 *  *) If enabled; decode input quoted strings as BF print sequences.
 *  *) If enabled; convert the '=' command into '[-]'.
 */
int
main(int argc, char ** argv)
{
    char * pgm = argv[0];
    int ch, lastch=']', c=0, m, b=0, lc=0, ar, inp=0;
    FILE * ifd;
    int digits = 0, number = 0, multi = 1;
    int qstring = 0;
    char ** filelist = 0;
    int filecount = 0;

    filelist = calloc(argc, sizeof*filelist);

    for(ar=1;ar<argc;ar++) {
	if (argv[ar][0] != '-' || argv[ar][1] == '\0') {
	    filelist[filecount++] = argv[ar];

	} else if (strcmp(argv[ar], "-h") == 0) {

	    fprintf(stderr, "%s: [options] [File]\n", pgm);
	    fprintf(stderr, "%s\n",
	    "\t"    "-h      This message"
	    "\n\t"  "-#      Consider '#' a BF instruction."
	    "\n\t"  "-wNN    Width of output lines (0/missing for unlimited)"
	    "\n\t"  "-R      Decode rle on '+-<>', quoted strings and '='."
	    "\n\t"  "-m -O0  Turn off optimisation"
	    "\n\t"  "-p -O1  This is a part of a BF program"
	    "\n\t"  "-O2     Normal optimisation"
	    ""
	    );

	    exit(0);
	} else if (check_argv(argv[ar])) {
	    ;
	} else if (strcmp(argv[ar], "--") == 0) {
	    ;
	    break;
	} else if (argv[ar][0] == '-') {
	    char * ap = argv[ar]+1;
	    static char buf[4] = "-X";
	    while(*ap) {
		buf[1] = *ap;
		if (!check_argv(buf))
		    break;
		ap++;
	    }
	    if (*ap) {
		fprintf(stderr, "Unknown option '%s'; try -h for option list\n",
			argv[ar]);
		exit(1);
	    }
	} else
	    filelist[filecount++] = argv[ar];
    }

    if (filecount == 0)
	filelist[filecount++] = "-";

    /* Now we do it ... */
    outrun('!', 0);
    for(ar=0; ar<filecount; ar++) {

	if (strcmp(filelist[ar], "-") == 0) {
	    ifd = stdin;
	    current_file = "stdin";
	} else if((ifd = fopen(filelist[ar],"r")) == 0) {
	    perror(filelist[ar]); exit(1);
	} else
	    current_file = filelist[ar];

	while((ch = getc(ifd)) != EOF && (ifd!=stdin || ch != '!' ||
		b || !inp || lc || qstring)) {
	    /* Quoted strings are printed. (And set current cell) */
	    if (qstring) {
		if (ch == '"') {
		    qstring++;
		    if (qstring == 2) continue;
		    if (qstring == 3) qstring = 1;
		}
		if (qstring == 2) {
		    qstring = 0;
		} else {
		    outrun('[', 1); outrun('-', 1); outrun(']', 1);
		    outrun('+', ch); outrun('.', 1);
		    lastch = '.'; c = 0;
		    continue;
		}
	    }
	    /* Source RLE decoding */
	    if (ch >= '0' && ch <= '9') {
		digits = 1;
		number = number*10 + ch-'0';
		continue;
	    }
	    if (ch == ' ' || ch == '\t') continue;
	    if (!digits) number = 1; digits=0;
	    multi = enable_rle?number:1; number = 0;
	    /* These chars are RLE */
	    m = (ch == '>' || ch == '<' || ch == '+' || ch == '-');
	    /* These ones are not */
	    if(!m && ch != '[' && ch != ']' && ch != '.' && ch != ',' &&
		(ch != '#' || !enable_debug) &&
		((ch != '"' && ch != '=') || !enable_rle)) continue;
	    /* Check for loop comments; ie: ][ comment ] */
	    if (lc || (ch=='[' && lastch==']' && enable_opt)) {
		lc += (ch=='[') - (ch==']'); continue;
	    }
	    if (lc) continue;
	    /* Do the RLE */
	    if (m && ch == lastch) { c+=multi; continue; }
	    /* Post the RLE token onward */
	    if (c) { outrun(lastch, c); c=0; }
	    if (!m) {
		/* Non RLE tokens here */
		if (ch == '"') { qstring++; continue; }
		if (ch == ',') inp = 1;
		if (ch == '=') {
		    outrun('[', 1); outrun('-', 1); outrun(']', 1);
		    lastch = ']';
		    continue;
		}
		if (!b && ch == ']') {
		    fprintf(stderr, "Warning: skipping unbalanced ']' command.\n");
		    continue; /* Ignore too many ']' */
		}
		b += (ch=='[') - (ch==']');
		if (lastch == '[' && ch == ']') outrun('X', 1);
		outrun(ch, 1);
	    } else
		c = multi;
	    lastch = ch;
	}
	if (ifd != stdin) fclose(ifd);
    }
    if(c) outrun(lastch, c);
    if(b>0) {
	fprintf(stderr, "Warning: closing unbalanced '[' command.\n");
	outrun('[', 1); outrun('-', 1); outrun(']', 1);
	while(b>0){ outrun(']', 1); b--;} /* Not enough ']', add some. */
    }
    outrun('~', 0);
    return 0;
}

/****************************************************************************/

void
outrun(int ch, int count)
{
static int zstate = 0;
    if (!enable_opt) { outcmd(ch, count); return; }

    switch(zstate)
    {
    case 1:
	if (count == 1 && ch == '-') { zstate=2; return; }
	if (count == 1 && ch == '+') { zstate=3; return; }
	outopt('[', 1);
	break;
    case 2:
	if (count == 1 && ch == ']') { zstate=4; return; }
	outopt('[', 1);
	outopt('-', 1);
	break;
    case 3:
	if (count == 1 && ch == ']') { zstate=4; return; }
	outopt('[', 1);
	outopt('+', 1);
	break;
    case 4:
	if (ch == '+') { outopt('=', count); zstate=0; return; }
	if (ch == '-') { outopt('=', -count); zstate=0; return; }
	outopt('=', 0);
	break;
    }
    zstate=0;
    if (count == 1 && ch == '[') { zstate++; return; }
    outopt(ch, count);
}

/****************************************************************************/

struct mem { struct mem *p, *n; int is_set; int v; int cleaned, cleaned_val;};
static int first_run = 0;

static struct mem *tape = 0, *tapezero = 0, *freelist = 0;
static int curroff = 0;

static int deadloop = 0;

static void
new_n(struct mem *p) {
    if (freelist) {
	p->n = freelist;
	freelist = freelist->n;
    } else
	p->n = calloc(1, sizeof*p);
    p->n->p = p;
    p->n->n = 0;
    p->n->is_set = first_run;
    p->n->cleaned = first_run;
}

static void
new_p(struct mem *p) {
    if (freelist) {
	p->p = freelist;
	freelist = freelist->n;
    } else
	p->p = calloc(1, sizeof*p);
    p->p->n = p;
    p->p->p = 0;
    p->p->is_set = first_run;
    p->p->cleaned = first_run;
}

static void clear_cell(struct mem *p)
{
    p->is_set = p->v = 0;
    p->cleaned = p->cleaned_val = 0;
}

static void
flush_tape(int no_output, int keep_knowns)
{
    int outoff = 0, tapeoff = 0;
    int direction = 1;
    int flipcount = 2;
    struct mem *p = tapezero;

    if(tapezero) {
	if (curroff > 0) direction = -1;

	for(;;)
	{
	    if (no_output) clear_cell(p);

	    if ((p->v || p->is_set) &&
		    (curroff != 0 || (tapeoff != 0 || flipcount == 0)) &&
		    (!keep_knowns || p == tape || !delay_clean)) {
		if (p->cleaned && p->cleaned_val == p->v && p->is_set) {
		    if (!keep_knowns) clear_cell(p);
		} else {

		    if (tapeoff > outoff) { outcmd('>', tapeoff-outoff); outoff=tapeoff; }
		    if (tapeoff < outoff) { outcmd('<', outoff-tapeoff); outoff=tapeoff; }
		    if (p->is_set) {
			if (p->cleaned && abs(p->v-p->cleaned_val) <= abs(p->v)+3) {
			    if (p->v > p->cleaned_val)
				outcmd('+', p->v-p->cleaned_val);
			    if (p->v < p->cleaned_val)
				outcmd('-', p->cleaned_val-p->v);
			} else {
			    outcmd('=', p->v);
			}
		    } else {
			if (p->v > 0) outcmd('+', p->v);
			if (p->v < 0) outcmd('-', -p->v);
		    }

		    if (keep_knowns && p->is_set) {
			p->cleaned = p->is_set;
			p->cleaned_val = p->v;
		    } else
			clear_cell(p);
		}
	    }

	    if (direction > 0 && p->n) {
		p=p->n; tapeoff ++;
	    } else
	    if (direction < 0 && p->p) {
		p=p->p; tapeoff --;
	    } else
	    if (flipcount) {
		flipcount--;
		direction = -direction;
	    } else
		break;
	}
    }

    if (no_output) outoff = curroff;
    if (curroff > outoff) { outcmd('>', curroff-outoff); outoff=curroff; }
    if (curroff < outoff) { outcmd('<', outoff-curroff); outoff=curroff; }

    if (!tapezero) tape = tapezero = calloc(1, sizeof*tape);

    if (!keep_knowns) {
	while(tape->p) tape = tape->p;
	while(tapezero->n) tapezero = tapezero->n;
	if (tape != tapezero) {
	    tapezero->n = freelist;
	    freelist = tape->n;
	    tape->n = 0;
	}
    }

    tapezero = tape;
    curroff = 0;
    first_run = 0;
}

void outopt(int ch, int count)
{
    if (deadloop) {
	if (ch == '[') deadloop++;
	if (ch == ']') deadloop--;
	return;
    }
    if (ch == '[') {
	if (tape->is_set && tape->v == 0) {
	    deadloop++;
	    return;
	}
    }

    switch(ch)
    {
    default:
	if (ch == '!') {
	    flush_tape(1,0);
	    tape->cleaned = tape->is_set = first_run = enable_initopt;
	} else if (ch == '~' && enable_initopt)
	    flush_tape(1,0);
	else
	    flush_tape(0,0);
	if (ch) outcmd(ch, count);

	/* Loops end with zero */
	if (ch == ']') {
	    tape->is_set = 1;
	    tape->v = 0;
	    tape->cleaned = 1;
	    tape->cleaned_val = tape->v;
	}

	/* I could save the cleaned tape state at the beginning of a loop,
	 * then when we find the matching end loop the two tapes could be
	 * merged to give a tape of known values after the loop ends.
	 * This would not break the pipeline style of this code.
	 *
	 * This would also give states where a cell is known to have one
	 * of two or more different values. */
	return;

    case '.':
	flush_tape(0,1);
	outcmd(ch, count);
	return;

    case ',':
	flush_tape(0,1);
	clear_cell(tape);
	outcmd(ch, count);
	return;

    case '>': while(count-->0) { if (tape->n == 0) new_n(tape); tape=tape->n; curroff++; } break;
    case '<': while(count-->0) { if (tape->p == 0) new_p(tape); tape=tape->p; curroff--; } break;
    case '+': tape->v += count; break;
    case '-': tape->v -= count; break;

    case '=': tape->v = count; tape->is_set = 1; break;

    }
}

/****************************************************************************/

static int col=0;

static void
pc(int ch)
{
    if ((col>=maxcol && maxcol) || ch == '\n') {
        putchar('\n');
        col = 0;
        if (ch == ' ' || ch == '\n') ch = 0;
    }
    if (ch) {
        putchar(ch);
        if ((ch&0xC0) != 0x80) /* Count UTF-8 */
            col++;
    }
}

void
outcmd(int ch, int count)
{
    if (ch == '=') {
	pc('['); pc('-'); pc(']');
	if (count>=0) ch = '+'; else { ch = '-'; count = -count; }
    }
    if (ch == '~' && col) pc('\n');
    if (ch == '!' || ch == 'X' || ch == '~') return;

    while(count-->0)
	pc(ch);
}
