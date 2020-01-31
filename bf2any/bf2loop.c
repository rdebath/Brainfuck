#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "bf2any_ex.h"

/* Brainfuck loop optimiser
 *
 */

#define asize(ar) ((int)(sizeof(ar)/sizeof(*(ar))))

static int qcmd[128];
static int qrep[128];
static int qcnt = 0;

static void process_loop(void);
static int try_constant_loop(int loopz, int inc, int mov, int has_zmode);
static int try_constant_slider(int mov);

static int madd_offset[32];
static int madd_inc[32];
static int madd_zmode[32];
static int madd_count = 0;

void
outtxn(int ch, int repcnt)
{
    /* Full normal BF optimisations, hunting for MAAD loops. */
    int i;

    /* Sweep everything from the buffer into outopt ? */
    if ((ch != '>' && ch != '<' && ch != '+' && ch != '-' &&
	 ch != '=' && ch != '[' && ch != ']') ||
	(qcnt == 0 && ch != '[') ||
	(qcnt >= asize(qcmd)-16)) {

	/* If it's not a new loop add the current one too */
	if(ch != '[' && ch != 0) {
	    qcmd[qcnt] = ch;
	    qrep[qcnt] = repcnt;
	    qcnt++;
	    ch = 0;
	}
	/* For the queue ... */
	for(i=0; i<qcnt; i++) {
	    if (qcmd[i] > 0) {
		/* Make sure counts are positive and post onward. */
		int cmd = qcmd[i];
		int rep = qrep[i];
		if (rep < 0) switch(cmd) {
		    case '>': cmd = '<'; rep = -rep; break;
		    case '<': cmd = '>'; rep = -rep; break;
		    case '+': cmd = '-'; rep = -rep; break;
		    case '-': cmd = '+'; rep = -rep; break;
		}
		outopt(cmd, rep);
	    }
	}
	qcnt = 0;
	if(ch != '[') return;
	/* We didn't add the '[' -- add it now */
    }

    /* The current loop has another one embedded.
       Send the old loop start out */
    if (ch == '[' && qcnt != 0) outtxn(0, 0);

    /* New token */
    qcmd[qcnt] = ch;
    qrep[qcnt] = repcnt;
    qcnt++;

    if (ch == '[')
    {
	if (bytecell) tape->v %= 256; /* Paranoia! */
	if (tape->is_set && tape->v == 0)
	    outtxn(0, 0);
    }

    /* Now to process a 'simple' loop */
    if (ch == ']')
	process_loop();
}

static void process_loop()
{
    int i;
    int mov = 0;	/* Total movement of <> in the loop */
    int inc = 0;	/* Total added to loop index */
    int loopz = 0;	/* Was the loop index zeroed with [-] */

    int use_v_var = 0;	/* Generate mult-v instructions */
    int doneif = 0;	/* Added if command ? */
    int donezap = 0;	/* Added loopcnt=0 ? */
    int minmov = 0;	/* Lowest address found */
    int has_zmode = 0;	/* Anything else zeroed with [-] ? */

    madd_count = 0;
    for(i=1; i<qcnt-1; i++) {
	int j;
	if (qcmd[i] == '>') { mov += qrep[i]; continue; }
	if (qcmd[i] == '<') {
	    mov -= qrep[i];
	    if (mov<minmov) minmov = mov;
	    continue;
	}
	if (qcmd[i] == '+' && mov == 0) { inc += qrep[i]; continue; }
	if (qcmd[i] == '-' && mov == 0) { inc -= qrep[i]; continue; }
	if (qcmd[i] == '=' && mov == 0) { loopz = 1; inc = qrep[i]; continue; }
	for (j=0; j<madd_count; j++) {
	    if (mov == madd_offset[j]) break;
	}
	if (j == madd_count) {
	    if (j >= asize(madd_offset)) {
		/* Loop too big */
		outtxn(0,0);
		return;
	    }
	    madd_offset[madd_count] = mov;
	    madd_inc[madd_count] = 0;
	    madd_zmode[madd_count] = 0;
	    madd_count ++;
	}
	if (qcmd[i] == '=') {
	    madd_zmode[j] = 1;
	    madd_inc[j] = qrep[i];
	    has_zmode ++;
	    continue;
	}
	if (qcmd[i] == '+') { madd_inc[j] += qrep[i]; continue; }
	if (qcmd[i] == '-') { madd_inc[j] -= qrep[i]; continue; }

	/* WTF! Some unknown command has made it here!? */
	outtxn(0,0);
	return;
    }

    if (mov) {
	if (qcnt == 3 && (qcmd[1] == '<' || qcmd[1] == '>'))
	    if (try_constant_slider(mov))
		return;
	outtxn(0, 0);  /* Unbalanced loop -- Nope. */
	return;
    }

    if (   (loopz != 0 && inc != 0)	/* Um, infinite loop? */
	|| (loopz == 0 && inc == 0)	/* This too */
	)
    {
	qcmd[qcnt] = qcmd[qcnt-1];
	qrep[qcnt] = qrep[qcnt-1];
	qcmd[qcnt-1] = 'X';
	qcnt++;
	outtxn(0, 0);
	return;
    }


    if (tape && tape->is_set) {
	/* We might be able to do the calculation. */
	if (try_constant_loop(loopz, inc, mov, has_zmode))
	    return;
    }

    /* Sigh, must have been an overflow */
    if (be_interface.disable_be_optim) { outtxn(0, 0); return; }

    /* Last check for not simple. */
    if (loopz == 0 && inc != -1 && inc != 1) { outtxn(0, 0); return; }

    /* Delete old loop */
    qcnt = 0;

    /* Start new */
    if ((minmov < -tapeinit || has_zmode) && !loopz) {
	/*
	 * Note the 'tapeinit' value; this is an optimisation
	 * tweak that allows loops, that may be skipped over, to
	 * be converted into move/add instructions even if the
	 * references inside the loop may be converted so they
	 * 'Add zero' to cells before the start of the tape. The
	 * start of the tape will be shifted by this count so the
	 * 'add zero' doesn't cause a segfault.
	 *
	 * Any reference that is further back than this range
	 * cannot be proven to be after the physical start of
	 * the tape so it must keep the 'if non-zero' condition
	 * to avert the possible segfault.
	 */

	qcmd[qcnt] = 'I'; qrep[qcnt] = 0; qcnt ++;
	if (madd_count > has_zmode) {
	    qcmd[qcnt] = 'B'; qrep[qcnt] = 1; qcnt ++;
	}
	doneif = 1;
    } else if (loopz) {
	inc = -1;
	qcmd[qcnt] = 'I'; qrep[qcnt] = 0; qcnt ++;
	qcmd[qcnt] = '='; qrep[qcnt] = 0; qcnt ++;
	donezap = 1;
	doneif = 1;
    } else {
	qcmd[qcnt] = 'B'; qrep[qcnt] = 1; qcnt ++;
    }

    for (use_v_var=0; use_v_var<2; use_v_var++)
    {
	for (i=0; i<madd_count; i++) {
	    if (!madd_zmode[i] && madd_inc[i] == 0) continue; /* NOP */

	    if (!use_v_var) {
		if (	(!madd_zmode[i] || madd_count<2) &&
			!loopz &&
			(!(madd_offset[i] < -tapeinit) || madd_count<2) )
		    continue;
	    }

	    if (use_v_var == doneif) {
		if (mov != 0) {
		    qcmd[qcnt] = '>'; qrep[qcnt] = 0 - mov; qcnt ++;
		    mov = 0;
		}
		if (!use_v_var && !doneif) {
		    qcmd[qcnt] = 'I'; qrep[qcnt] = 0; qcnt ++;
		    doneif = 1;
		}
	    }

	    if (mov != madd_offset[i]) {
		qcmd[qcnt] = '>'; qrep[qcnt] = madd_offset[i] - mov; qcnt ++;
		mov = madd_offset[i];
	    }

	    if (doneif && (loopz || madd_zmode[i])) {
		if (madd_zmode[i]) {
		    qcmd[qcnt] = '='; qrep[qcnt] = madd_inc[i]; qcnt ++;
		} else {
		    qcmd[qcnt] = '+'; qrep[qcnt] = madd_inc[i] * -inc;
		    qcnt++;
		}
	    } else if (madd_zmode[i]) {
		fprintf(stderr, "Zmode failure.\n");
		exit(1);
	    } else {
		qcmd[qcnt] = 'M'; qrep[qcnt] = madd_inc[i] * -inc;

		if (mov < -tapeinit && !doneif) {
		    fprintf(stderr, "Unexpected TAPE underflow\n");
		    exit(1);
		}
		qcnt++;
	    }
	    madd_zmode[i] = madd_inc[i] = 0; /* Done this one */
	}
    }

    if (mov != 0) {
	qcmd[qcnt] = '>'; qrep[qcnt] = 0 - mov; qcnt ++;
    }

    if (doneif) {
	qcmd[qcnt] = 'E'; qrep[qcnt] = 0; qcnt ++;
	doneif = 0;
    }

    if (!donezap) {
	qcmd[qcnt] = '='; qrep[qcnt] = 0; qcnt ++;
	donezap = 1;
    }

    /* And forward the converted loop */
    outtxn(0, 0);
}

static int
try_constant_loop(int loopz, int inc, int mov, int has_zmode)
{
    int i;
    int v = tape->v, ninc = inc;

    if (!tape->is_set) return 0;

    if (be_interface.disable_be_optim && !bytecell)
	/* The unoptimised backends are usually eight bit, even if I
	 * don't know it yet. So if this code looks like it might be
	 * testing for 8-bit don't optimise too hard.
	 *
	 * Specifically, if the known value, the loop count is not
	 * zero but is a value that an eight bit interpreter would
	 * wrap we don't actually know if this code would be run or
	 * skipped.
	 *
	 * Simple additions or multiplications are okay, but any
	 * 'set to zero' is different for zero.
	 */
	if (loopz || has_zmode) {
	{
	    /* If it's wrapped to 8bit will it be zero ? */
	    if (v != 0 && v%256 == 0)
		return 0;
	}
    }

    if (loopz) {
	if (bytecell) v %= 256; /* Paranoia! */
	ninc = -1;
	v = (v!=0);
    }

    /* Exact division ? */
    if (ninc != -1) {
	if ((ninc>0) == (v<0) &&
	    abs(v) % abs(ninc) == 0) {
	    v = abs(v / ninc);
	    ninc = -1;
	}
    }

    if (bytecell && ninc != -1) {
	/* I could solve the modulo division, but for eight bits
	 * it's a lot simpler just to trial it. */
	unsigned short cnt = 0;
	int sum = tape->v & 0xFF;
	while(sum && cnt < 256) {
	    cnt++;
	    sum = ((sum + (ninc & 0xFF)) & 0xFF);
	}

	if (cnt < 256 && sum == 0) {
	    v = cnt;
	    ninc = -1;
	}
    }

    if (ninc == -1)
    {
	int ov = 0;
	if (!be_interface.cells_are_ints)
	    for(i=0; i<madd_count; i++)
	    {
		if (madd_zmode[i]) continue;
		/* res = */ ov_imul(madd_inc[i], v, &ov);
		if (ov) break;
	    }

	if (!ov)
	{
	    qcnt = 0;
	    qcmd[qcnt] = '='; qrep[qcnt] = 0; qcnt ++;
	    for(i=0; i<madd_count; i++)
	    {
		if (!madd_zmode[i] && madd_inc[i] == 0) continue; /* NOP */
		if (mov != madd_offset[i]) {
		    qcmd[qcnt] = '>';
		    qrep[qcnt] = madd_offset[i] - mov; qcnt ++;
		    mov = madd_offset[i];
		}

		if (madd_zmode[i]) {
		    qcmd[qcnt] = '='; qrep[qcnt] = madd_inc[i]; qcnt ++;
		    if (bytecell) qrep[qcnt] %= 256;
		} else {
		    qcmd[qcnt] = '+'; qrep[qcnt] = madd_inc[i] * v;
		    if (bytecell) qrep[qcnt] %= 256;
		    if (qrep[qcnt] < 0) {
			qcmd[qcnt] = '-';
			qrep[qcnt] = -qrep[qcnt];
		    }
		    qcnt++;
		}
	    }
	    if (mov != 0) {
		qcmd[qcnt] = '>'; qrep[qcnt] = 0 - mov; qcnt ++;
	    }

	    /* And forward the converted loop */
	    outtxn(0, 0);
	    return 1;
	}
    }
    return 0;
}

static int
try_constant_slider(int mov)
{
    struct mem *ctape = tape;
    int i, steps = 0;
    if (qcmd[1] == '<') mov = -mov;
    if (mov < 1) return 0;

    while ((!ctape && tape_is_all_blank) || (ctape && ctape->is_set)) {
	if (ctape == 0 || ctape->v == 0) {
	    qcnt = 1;
	    qcmd[0] = qcmd[1];
	    qrep[0] = steps;
	    outtxn(0, 0);
	    return 1;
	}
	for(i=0; i<mov; i++) {
	    if (qcmd[1] == '>') {
		if (!ctape || ctape->n == 0) {
		    if (!tape_is_all_blank)
			return 0;
		    ctape = 0;
	        } else
		    ctape = ctape->n;
		steps ++;
	    }
	    if (qcmd[1] == '<') {
		if (!ctape || ctape->p == 0) {
		    if (!tape_is_all_blank)
			return 0;
		    ctape = 0;
		} else
		    ctape = ctape->p;
		steps ++;
	    }
	}
    }
    return 0;
}
