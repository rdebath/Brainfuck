#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

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

struct mem { struct mem *p, *n; int is_set; int v; int cleaned, cleaned_val;};
static int first_run = 0;

static struct mem *tape = 0, *tapezero = 0;
static int curroff = 0;
static int reg_known = 0, reg_val;

static int enable_prt = 0;
static char * sav_str_str = 0;
static unsigned int sav_str_maxlen = 0, sav_str_len = 0;

static int deadloop = 0;

static void
new_n(struct mem *p) {
    p->n = calloc(1, sizeof*p);
    p->n->p = p;
    p->n->is_set = first_run;
    p->n->cleaned = first_run;
}

static void
new_p(struct mem *p) {
    p->p = calloc(1, sizeof*p);
    p->p->n = p;
    p->p->is_set = first_run;
    p->p->cleaned = first_run;
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
    if (sav_str_len) return sav_str_str;
    else return 0;
}

static void
flush_tape(int no_output)
{
    int outoff = 0, tapeoff = 0;
    int direction = 1;
    int flipcount = 2;
    struct mem *p = tapezero;

    if (sav_str_len>0) {
	add_string(0);
	outcmd('"', 0);
	sav_str_len = 0;
    }
    if(tapezero) {
	if (curroff > 0) direction = -1;

	for(;;)
	{
	    if (bytecell) p->v %= 256; /* Note: preserves sign but limits range. */
	    if (p->v || p->is_set) {
		if (no_output) {
		    outoff=tapeoff;
		    p->is_set = p->v = 0;
		}

		if (p->cleaned && p->cleaned_val == p->v && p->is_set) {
		    p->is_set = p->v = 0;
		    p->cleaned = p->cleaned_val = 0;
		} else {

		    if (tapeoff > outoff) { outcmd('>', tapeoff-outoff); outoff=tapeoff; }
		    if (tapeoff < outoff) { outcmd('<', outoff-tapeoff); outoff=tapeoff; }
		    if (p->cleaned) {
			if (p->v > p->cleaned_val)
			    outcmd('+', p->v-p->cleaned_val);
			if (p->v < p->cleaned_val)
			    outcmd('-', p->cleaned_val-p->v);
		    } else if (p->is_set) {
			outcmd('=', p->v);
		    } else {
			if (p->v > 0) { outcmd('+', p->v); p->v=0; }
			if (p->v < 0) { outcmd('-', -p->v); p->v=0; }
		    }

		    p->v = p->is_set = 0;
		    p->cleaned = p->cleaned_val = 0;
		}
	    } else
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
	tapeoff = curroff;
	if (no_output) outoff=tapeoff;
	if (tapeoff > outoff) { outcmd('>', tapeoff-outoff); outoff=tapeoff; }
	if (tapeoff < outoff) { outcmd('<', outoff-tapeoff); outoff=tapeoff; }
    }
    if (!tapezero) tapezero = calloc(1, sizeof*tape);
    tape = tapezero;
    curroff = 0;
    reg_known = 0;
    reg_val = 0;
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
	    enable_prt = check_arg("-savestring");
	    flush_tape(1);
	    tape->cleaned = tape->is_set = first_run = 1;
	} else if (ch == '~')
	    flush_tape(1);
	else
	    flush_tape(0);
	if (ch) outcmd(ch, count);

	/* Loops end with zero */
	if (ch == ']') {
	    tape->is_set = 1;
	    tape->v = 0;
	    tape->cleaned = 1;
	    tape->cleaned_val = tape->v;
	}
	return;

    case '.':
	if (enable_prt && tape->is_set && tape->v > 0 && tape->v < 128) {
	    add_string(tape->v);

	    if (sav_str_len >= 128*1024) /* Limit the buffer size. */
	    {
		add_string(0);
		outcmd('"', 0);
		sav_str_len = 0;
	    }
	    break;
	}
	flush_tape(0);
	outcmd(ch, count);
	return;

    case '>': while(count-->0) { if (tape->n == 0) new_n(tape); tape=tape->n; curroff++; } break;
    case '<': while(count-->0) { if (tape->p == 0) new_p(tape); tape=tape->p; curroff--; } break;
    case '+': tape->v += count; break;
    case '-': tape->v -= count; break;

    case '=': tape->v = count; tape->is_set = 1; break;

    case 'B':
	if (!tape->is_set) {
	    flush_tape(0);
	    outcmd(ch, count);
	    return;
	}
	if (bytecell) tape->v %= 256; /* Note: preserves sign but limits range. */
	reg_known = 1;
	reg_val = tape->v;
	break;

    case 'M':
    case 'N':
    case 'S':
    case 'Q':
    case 'm':
    case 'n':
    case 's':
	if (!reg_known) {
	    flush_tape(0);
	    outcmd(ch, count);
	    return;
	}
	switch(ch) {
	case 'B':
	    reg_known = 1;
	    reg_val = tape->v;
	    break;
	case 'm':
	case 'M':
	    tape->v += reg_val * count;
	    break;
	case 'n':
	case 'N':
	    tape->v -= reg_val * count;
	    break;
	case 's':
	case 'S':
	    tape->v += reg_val;
	    break;

	case 'Q':
	    if (reg_val != 0) {
		tape->v = count;
		tape->is_set = 1;
	    }
	    break;
	}
	if (bytecell) tape->v %= 256; /* Note: preserves sign but limits range. */
    }
}
