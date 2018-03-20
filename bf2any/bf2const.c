#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "bf2any.h"
#include "bf2const.h"
#include "ov_int.h"

/*
 *  Simple constant folding.
 *
 *  This routine takes the stream of tokens from the BF peephole optimisation
 *  and re-sorts the tokens between loops merging both the standard tokens and
 *  the tokens derived from flattening simple loops.
 *
 *  This allows it to build up larger ADD and SET tokens generated from
 *  multiple chained simple loops.
 *
 *  In addition now that the args to the PRT command ('.') are often constant
 *  this can buffer up the characters and send then to the backend in one go.
 *  (if the backend agrees.)
 *
 *  Note; I'm limiting the range when bytecell is set by using %= 256,
 *  this uses the range -255..255 preserving the sign. This is larger
 *  than the range actually needed but has the nice effect that it won't
 *  convert '-' commands into '+' commands as a matter of routine.
 */

static int first_run = 0;

struct mem *tape = 0;
static struct mem *tapezero = 0, *freelist = 0;
static int curroff = 0;

static char * sav_str_str = 0;
static unsigned int sav_str_maxlen = 0, sav_str_len = 0;

static int deadloop = 0;

static void be_codegen_failure(void);

static void
new_n(struct mem *p) {
    if (freelist) {
	p->n = freelist;
	freelist = freelist->n;
    } else
	p->n = calloc(1, sizeof*p);
    if (!p->n) { perror("bf2any: new_n()"); exit(1); }
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
    if (!p->p) { perror("bf2any: new_p()"); exit(1); }
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

static void add_string(int ch)
{
    while (sav_str_len+2 > sav_str_maxlen) {
	sav_str_str = realloc(sav_str_str, sav_str_maxlen += 512);
	if(!sav_str_str) { perror("bf2any: string"); exit(1); }
    }
    sav_str_str[sav_str_len++] = ch;
}

char *
get_string(void)
{
    if (sav_str_len) {
	sav_str_len = 0;
	return sav_str_str;
    } else return 0;
}

static void
flush_string(void)
{
    if (sav_str_len<=0) return;
    add_string(0);
    outcmd('"', 0);
    if (sav_str_len<=0) return;

    {
	int outoff = 0, tapeoff = 0;
	int direction = 1;
	int flipcount = 1;
	struct mem* p = tapezero;

	for(;;)
	{
	    if (p->is_set)
		break;

	    if (direction > 0 && p->n) {
		p=p->n; tapeoff ++;
	    } else
	    if (direction < 0 && p->p) {
		p=p->p; tapeoff --;
	    } else
	    if (flipcount) {
		flipcount--;
		direction = -direction;
	    } else {
		fprintf(stderr, "FAILED\n");
		exit(1);
	    }
	}

	if (tapeoff > outoff) { outcmd('>', tapeoff-outoff); outoff=tapeoff; }
	if (tapeoff < outoff) { outcmd('<', outoff-tapeoff); outoff=tapeoff; }
	tapezero = p;
	curroff -= tapeoff;
    }

    {
	char c=0, *p = sav_str_str;
	while(*p) {
	    outcmd('=', (c = *p++));
	    outcmd('.', 0);
	}
	tapezero->cleaned = 1;
	tapezero->cleaned_val = c;
	sav_str_len = 0;
    }
}

static void
flush_tape(int no_output, int keep_knowns)
{
    int outoff = 0, tapeoff = 0;
    int direction = 1;
    int flipcount = 2;
    struct mem *p;

    if (sav_str_len>0)
	flush_string();

    p = tapezero;

    if(tapezero) {
	if (curroff > 0) direction = -1;

	for(;;)
	{
	    if (no_output) clear_cell(p);

	    if (bytecell) p->v %= 256; /* Note: preserves sign but limits range. */

	    if ((p->v || p->is_set) &&
		    (curroff != 0 || (tapeoff != 0 || flipcount == 0)) &&
		    (!keep_knowns || p == tape)) {
		if (p->cleaned && p->cleaned_val == p->v && p->is_set) {
		    if (!keep_knowns) clear_cell(p);
		} else {

		    if (tapeoff > outoff) { outcmd('>', tapeoff-outoff); outoff=tapeoff; }
		    if (tapeoff < outoff) { outcmd('<', outoff-tapeoff); outoff=tapeoff; }
		    if (p->is_set) {
			int c = p->v;
			if (bytecell) c &= 0xFF;
			outcmd('=', c);
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
    case '[': case ']': case '!': case '~': case 'X': case '#':
    case 'I': case 'E':
	if (ch == '!') {
	    flush_tape(1,0);
	    tape->cleaned = tape->is_set = first_run = !disable_init_optim;
	} else if (ch == '~' && enable_optim && !disable_init_optim)
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
	if (tape->is_set) {
	    int c = tape->v;
	    if (bytecell) c &= 0xFF;
	    if (c > 0 && c < 128) {
		add_string(c);

		/* Limit the buffer size. */
		if (sav_str_len >= 128*1024 - (tape->v=='\n')*1024)
		    flush_string();
		break;
	    }
	}
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
    case '+':
	if (be_interface.cells_are_ints || bytecell) {
	    tape->v += count;
	    if (bytecell) tape->v %= 256; /* -255..255 */
	} else {
	    int ov=0, res;
	    res = ov_iadd(tape->v, count, &ov);
	    if (!ov)
		tape->v = res;
	    else {
		flush_tape(0,1);
		clear_cell(tape);
		outcmd(ch, count);
	    }
	}
	break;
    case '-':
	if (be_interface.cells_are_ints || bytecell) {
	    tape->v -= count;
	    if (bytecell) tape->v %= 256; /* -255..255 */
	} else {
	    int ov=0, res;
	    res = ov_isub(tape->v, count, &ov);
	    if (!ov)
		tape->v = res;
	    else {
		flush_tape(0,1);
		clear_cell(tape);
		outcmd(ch, count);
	    }
	}
	break;

    case '=': tape->v = count; tape->is_set = 1; break;

    case 'B':
	flush_tape(0,1);
	if (be_interface.disable_be_optim) be_codegen_failure();
	outcmd(ch, count);
	return;

    case 'M':
    case 'N':
    case 'S':
    case 'Q':
    case 'm':
    case 'n':
    case 's':
	flush_tape(0,1);
	clear_cell(tape);
	if (be_interface.disable_be_optim) be_codegen_failure();
	outcmd(ch, count);
	return;

    default:
	if (be_interface.disable_be_optim) be_codegen_failure();
	if (ch>=0 && ch<256)
	    fprintf(stderr, "Unknown token in bf2const.c (%d)\n", ch);
	flush_tape(0,0);
	outcmd(ch, count);
	return;
    }
}

static void
be_codegen_failure(void)
{
    fprintf(stderr, "Limited backend code generator failure\n");
    exit(1);
}

