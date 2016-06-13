/*
 *  Convert tree to code for dc(1)
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "bfi.tree.h"
#include "bfi.run.h"
#include "bfi.dc.h"

static FILE * ofd;
static int no_v7 = 0;

/*
 * DC Registers:
 * p	The bf pointer
 * a	The bf array or tape
 * F	The bf program (pushed before start)
 *
 * b	A stack of currently open while loops
 * i	Input a chracter to the TOS
 * o	Output the TOS as one character (calls reg 'm' if needed)
 * m	Replaces TOS with (TOS & cell_mask)
 * M	Used for when % gives a negative in 'm'.
 *
 * C	May be defined as an array of character printers for lox
 *
 * A B Z
 *	Used as temps, restored.
 *
 *
 * Note: traditional dc has no way of entering characters from stdin, a filter
 * must be used that converts the characters into numbers. End of file is
 * properly detected though.
 */

static void
fetch_cell(int offset)
{
    if (offset>0)
	fprintf(ofd, "lp%d+;a", offset);
    else if (offset==0)
	fprintf(ofd, "lp;a");
    else
	fprintf(ofd, "lp_%d+;a", -offset);
}

static void
save_cell(int offset)
{
    if (offset>0)
	fprintf(ofd, "lp%d+:a\n", offset);
    else if (offset==0)
	fprintf(ofd, "lp:a\n");
    else
	fprintf(ofd, "lp_%d+:a\n", -offset);
}

static void
prt_value(const char * prefix, int count, const char * suffix)
{
    if (count>=0)
	fprintf(ofd, "%s%d%s", prefix, count, suffix);
    else {
	unsigned int u = -count;
	fprintf(ofd, "%s_%u%s", prefix, u, suffix);
    }
}

int
checkarg_dc(char * opt, char * arg UNUSED)
{
    if (!strcmp(opt, "-nov7")) {
	no_v7 = 1;
	return 1;
    }
    return 0;
}

/* This is true for characters that are safe in a [ ] string */
/* '\' if for bsd dc, '|' and '~' are for dc.sed */
#define okay_for_printf(xc) \
		    ( (xc) == '\n' || ( (xc)>= ' ' && (xc) <= '~' && \
		      (xc) != '\\' && (xc) != '|' && (xc) != '~' && \
		      (xc) != '[' && (xc) != ']' ) \
		    )
void
print_dc(void)
{
    struct bfi * n = bfprog;
    int stackdepth = 0;
    int hello_world;
    int used_lix = 0, used_lox = 0;
    int use_lmx = 0;
    ofd = stdout;

    hello_world = (total_nodes == node_type_counts[T_CHR] && !noheader);

    if (hello_world) {
	struct bfi * v = n;
	while(v && v->type == T_CHR && okay_for_printf(v->count))
	    v=v->next;
	if (v) hello_world = 0;
    }

    if (hello_world) {
	printf("[");
	for(; n; n=n->next) putchar(n->count);
	printf("]Pq\n");
	return;
    }

    if (no_v7) {
	fprintf(ofd, "#!/usr/bin/dc\n");
	/* fprintf(ofd, "# [This program requires a modern dc.]pq\n\n");
	 * Note: the above comment blocks v7 dc from running *
	 * But V7 dc gets stuck on the #! first
	 */
	fprintf(ofd, "# Code generated from %s\n\n", bfname);
    } else
	fprintf(ofd, "[ Code generated from %s ]SF\n\n", bfname);

    if (most_neg_maad_loop < 0 && node_type_counts[T_MOV] != 0)
	fprintf(ofd, "[%dsp\n", -most_neg_maad_loop);
    else
	fprintf(ofd, "[0sp\n");

    use_lmx = (cell_length>0);

    while(n)
    {
	switch(n->type)
	{
	case T_MOV:
	    prt_value("lp", n->count, "+sp\n");
	    break;

	case T_ADD:
	    fetch_cell(n->offset);
	    prt_value("", n->count, "+");
	    save_cell(n->offset);
	    break;

	case T_SET:
	    prt_value("", n->count, "");
	    save_cell(n->offset);
	    break;

	case T_CALC:
	    /* p[offset] = count + count2 * p[offset2] + count3 * p[offset3] */
	    if (n->offset == n->offset2 && n->count2 == 1) {
		fetch_cell(n->offset2);
		if (n->count)
		    prt_value("", n->count, "+");
	    } else if (n->count2 != 0) {
		fetch_cell(n->offset2);
		if (n->count2 != 1)
		    prt_value("", n->count2, "*");
		if (n->count)
		    prt_value("", n->count, "+");
	    } else
		prt_value("", n->count, " ");

	    if (n->count3 != 0) {
		fetch_cell(n->offset3);

		if (n->count3 != 1)
		    prt_value("", n->count3, "*+");
		else
		    fprintf(ofd, "+");
	    }

	    save_cell(n->offset);
	    break;

	case T_IF: case T_MULT: case T_CMULT:
	case T_WHL:
	    stackdepth++;
	    fprintf(ofd, "[\n");
	    break;

	case T_END:
	    stackdepth--;
	    fetch_cell(n->offset);
	    if (use_lmx) fprintf(ofd, "lmx ");

	    fprintf(ofd, "0!=b]Sb ");

	    fetch_cell(n->offset);
	    if (use_lmx) fprintf(ofd, "lmx ");

	    fprintf(ofd, "0!=bLbc\n");
	    break;

	case T_ENDIF:
	    stackdepth--;
	    fprintf(ofd, "]Sb ");
	    fetch_cell(n->offset);
	    if (use_lmx) fprintf(ofd, "lmx ");

	    fprintf(ofd, "0!=bLbc\n");
	    break;

	case T_PRT:
	    fetch_cell(n->offset);
	    if (use_lmx && cell_length<8) fprintf(ofd, "lmx ");
	    if (no_v7)
		fprintf(ofd, "aP\n");
	    else {
		fprintf(ofd, "lox\n");
		used_lox = 1;
	    }
	    break;

	case T_CHR:
	    if (!okay_for_printf(n->count)) {
		if (no_v7)
		    prt_value("", n->count, "aP\n");
		else {
		    prt_value("", n->count, "lox\n");
		    used_lox = 1;
		}
	    } else {
		unsigned i = 0, j;
		struct bfi * v = n;
		char *s, *p;
		while(v->next && v->next->type == T_CHR &&
			    okay_for_printf(v->next->count)) {
		    v = v->next;
		    i++;
		}
		p = s = malloc(i+2);

		for(j=0; j<i; j++) {
		    *p++ = (char) /*GCC -Wconversion*/ n->count;
		    n = n->next;
		}
		*p++ = (char) /*GCC -Wconversion*/ n->count;
		*p++ = 0;

		fprintf(ofd, "[%s]P\n", s);
		free(s);
	    }
	    break;

	case T_INP:
	    fprintf(ofd, "lix");
	    used_lix = 1;

	    fprintf(ofd, "[");
	    save_cell(n->offset);
	    fprintf(ofd, "]SA d _1!=A ");
	    fprintf(ofd, "0sALAc");
	    break;

	case T_STOP:
	    fprintf(ofd, "[STOP command executed\n]P\n");
	    if (stackdepth>1)
		fprintf(ofd, "%dQ\n", stackdepth);
	    else
		fprintf(ofd, "q\n");
	    break;

	case T_NOP:
	case T_DUMP:
	    fprintf(stderr, "Warning on code generation: "
		    "%s node: ptr+%d, cnt=%d, @(%d,%d).\n",
		    tokennames[n->type],
		    n->offset, n->count, n->line, n->col);
	    break;

	default:
	    fprintf(stderr, "Code gen error: "
		    "%s\t"
		    "%d,%d,%d,%d,%d,%d\n",
		    tokennames[n->type],
		    n->offset, n->count,
		    n->offset2, n->count2,
		    n->offset3, n->count3);
	    exit(1);
	}
	n=n->next;
    }

    fprintf(ofd, "q]SF\n");

    if (used_lox) {
	int i;
	/* Setup the Z register with a script to create the C array */
	fprintf(ofd, "[");
	for (i = 1; i < 127; i++) {
	    if (okay_for_printf(i)) {
		fprintf(ofd, "[[%c]P]%d:C", i, i);
	    } else if (i < 100) { /* Only works for 1..99 in V7 dc */
		fprintf(ofd, "[%dP]%d:C", i, i);
	    }
	    if (i % 8 == 7)
		fprintf(ofd, "\n");
	}
	fprintf(ofd, "]SZ\n");

	/* Use 'Z' command to detect v7 dc. It detects dc.sed as not v7. */
	/* So if Z says ok we see if the '#' comment character works too. */

	fprintf(ofd, "[[aP]so\n");
	fprintf(ofd, "1#1-\n");
	fprintf(ofd, "]SB\n");
	fprintf(ofd, "[lZx [256%%256+256%%;Cx]so ]SA\n");
	fprintf(ofd, "0 0[o]Z1=B0=A\n");
	/* Restore A, B and Z */
	fprintf(ofd, "0sALAsBLBsZLZd!=.\n");
    }

    if (use_lmx) {
	if (cell_mask > 0)
	    fprintf(ofd, "[%u+]sM [%u %% d0>M]sm\n",
		    (unsigned)cell_mask+1, (unsigned)cell_mask+1);
	else if (cell_size == 32) {
	    static char ttt[] = "4294967296";
	    fprintf(ofd, "[%s+]sM [%s %% d0>M]sm\n", ttt, ttt);
	} else {
	    fprintf(ofd, "[2 %d^+]sM [2 %d^%% d0>M]sm\n",
		    cell_length, cell_length);
	}
    }

    if (used_lix) {
	if (input_string) {
	    char * p = input_string;
	    int c = 0;
	    for(;*p; p++)
		fprintf(ofd, "%d %d:I\n", (unsigned char)*p, ++c);
	    fprintf(ofd, "%d 0:I\n", c);
	    fprintf(ofd, "0sn\n");
	    fprintf(ofd, "[[d>.ln1+dsn;I]SN_1ln0;I>N0sNLNd>.]si\n");
	} else {

	    /* New for GNU dc, character input ... soon */
	    fprintf(ofd, "0dd:Isn\n");
	    fprintf(ofd, "[[0$I0sn]SNln0;I!>N\n");
	    fprintf(ofd, "[d>.ln1+dsn;I]sN_1ln0;I>N0sNLNd>.]si\n");

	    /* New for GNU dc, character I/O ... soon
	    fprintf(ofd, "[1G [sB_1]SA [bAla]SB 0=A Bx 0sALAaBLB+ ]si\n");

	    */
	    /* Input integers, -1 on EOF
	    // fprintf(ofd, "[? z [_1]SA 0=A 0sALA+ ]si\n");
	    */
	}
    }

    fprintf(ofd, "LFx\n");
}
