#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "bf2any.h"
#include "bf2loop.h"
#include "bf2const.h"
#include "ov_int.h"

/* Brainfuck loop optimiser
 *
 */
static int qcmd[64];
static int qrep[64];
static int qcnt = 0;

void process_loop(void);
int try_constant_loop(int loopz, int inc, int mov);

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
	(qcnt >= sizeof(qcmd)/sizeof(*qcmd)-1)) {

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
	if (disable_be_optim)
	{
	    if (tape->is_set)
	    {
		if (bytecell) tape->v %= 256; /* -255 .. +255 */
		/* The unoptimised backends are usually eight bit, even if I
		 * don't know it yet. So if this code looks like it might be
		 * testing for 8-bit don't optimise too hard. */
		if (tape->v%256 != 0)
		    return; /* OK for dumb BE */
	    }
	    outtxn(0, 0);
	} else if (tape->is_set && tape->v == 0)
	    outtxn(0, 0);
    }

    /* Now to process a 'simple' loop */
    if (ch == ']')
	process_loop();
}

void process_loop()
{
    int i;
    int mov = 0;	/* Total movement of <> in the loop */
    int inc = 0;	/* Total added to loop index */
    int loopz = 0;	/* Was the loop index zeroed with [-] */

    madd_count = 0;
    for(i=1; i<qcnt-1; i++) {
	int j;
	if (qcmd[i] == '>') { mov += qrep[i]; continue; }
	if (qcmd[i] == '<') { mov -= qrep[i]; continue; }
	if (qcmd[i] == '+' && mov == 0) { inc += qrep[i]; continue; }
	if (qcmd[i] == '-' && mov == 0) { inc -= qrep[i]; continue; }
	if (qcmd[i] == '=' && mov == 0) { loopz = 1; inc = qrep[i]; continue; }
	for (j=0; j<madd_count; j++) {
	    if (mov == madd_offset[j]) break;
	}
	if (j == madd_count) {
	    if (j >= sizeof(madd_offset)/sizeof(*madd_offset)) {
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
	    continue;
	}
	if (qcmd[i] == '+') { madd_inc[j] += qrep[i]; continue; }
	if (qcmd[i] == '-') { madd_inc[j] -= qrep[i]; continue; }

	/* WTF! Some unknown command has made it here!? */
	outtxn(0,0);
	return;
    }

    if (mov) { outtxn(0, 0); return; }  /* Unbalanced loop -- Nope. */

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


    if (tape->is_set) {
	/* We might be able to do the calculation. */
	if (try_constant_loop(loopz, inc, mov))
	    return;
    }

    /* Sigh, must have been an overflow */
    if (disable_be_optim) { outtxn(0, 0); return; }

    /* Last check for not simple. */
    if (loopz == 0 && inc != -1 && inc != 1) { outtxn(0, 0); return; }

    /* Delete old loop */
    qcnt = 0;

    /* Start new */
    qcmd[qcnt] = 'B'; qrep[qcnt] = 1; qcnt ++;
    if (loopz) {
	inc = -1;
	qcmd[qcnt] = 'Q'; qrep[qcnt] = 1; qcnt ++;
	qcmd[qcnt] = 'B'; qrep[qcnt] = 1; qcnt ++;
    }
    qcmd[qcnt] = '='; qrep[qcnt] = 0; qcnt ++;

    for (i=0; i<madd_count; i++) {
	if (!madd_zmode[i] && madd_inc[i] == 0) continue; /* NOP */
	if (mov != madd_offset[i]) {
	    qcmd[qcnt] = '>'; qrep[qcnt] = madd_offset[i] - mov; qcnt ++;
	    mov = madd_offset[i];
	}
	if (madd_zmode[i]) {
	    qcmd[qcnt] = 'Q'; qrep[qcnt] = madd_inc[i]; qcnt ++;
	} else {
	    qcmd[qcnt] = 'M'; qrep[qcnt] = madd_inc[i] * -inc;
	    if (qrep[qcnt] == 1) qcmd[qcnt] = 'S';
	    if (qrep[qcnt] < 0) {
		qcmd[qcnt] = 'N';
		qrep[qcnt] = -qrep[qcnt];
	    }

	    /*
	     * Note the 'BOFF' define; this is an optimisation
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

	    if (mov < -BOFF) qcmd[qcnt] = qcmd[qcnt] - 'A' + 'a';
	    qcnt++;
	}
    }

    if (mov != 0) {
	qcmd[qcnt] = '>'; qrep[qcnt] = 0 - mov; qcnt ++;
    }

    /* And forward the converted loop */
    outtxn(0, 0);
}

int
try_constant_loop(int loopz, int inc, int mov)
{
    int i;
    int v = tape->v, ninc = inc;

    if (!tape->is_set) return 0;

    if (loopz) {
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
	if (!cells_are_ints)
	    for(i=0; i<madd_count; i++)
	    {
		int res;
		if (madd_zmode[i]) continue;
		res = ov_imul(madd_inc[i], v, &ov);
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
