#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

#include "bfi.tree.h"
#include "bfi.run.h"
#include "opt_runner.h"

/* Trial run. */
static int update_opt_runner(struct bfi * n, int * mem, int offset);
static void run_tree_noinp(void);

static long long profile_min_cell, profile_max_cell;
static struct bfi *opt_run_start = 0, *opt_run_end = 0;

#define UCSMAX 0x10FFFF

/*
 * This function contains the code for the '-Orun' option.
 * The option simply runs the optimised tree until it finds a loop containing
 * an input instruction. Then everything it's run gets converted onto a print
 * string and some code to setup the tape.
 *
 * This is a rather silly optimiser and for a good benchmark program
 * this should do nothing, but frequently it'll change the compiled
 * program into a 'Hello World'.
 *
 * TODO: If there are NO T_INP instructions we could use a fast runner and
 *      just intercept the output.
 *
 *      I could continue running until we actually hit the T_INP instruction,
 *      I would need to save the current tape state so that it can be rolled
 *      back to the beginning of the loop.
 *
 *      This method could also be used in the middle of the program, just stop
 *      when you find a calculation based on an unknown.
 *
 *      A simple counter would stop this hanging on an infinite loop.
 */
void
try_opt_runner(void)
{
    struct bfi *v = bfprog, *n = 0;
    int lp = 0;

    if (cell_size <= 0) return; /* Oops! */

    while(v && v->type != T_INP && v->type != T_INPI && v->type != T_PRTI) {
	if (v->orgtype == T_END) lp--;
	if(!lp && v->orgtype != T_WHL) n=v;
	if (v->orgtype == T_WHL) lp++;
	v=v->next;
    }

    if (n == 0) return;

    if (verbose>5) {
	fprintf(stderr, "Inserting T_SUSP node after: ");
	printtreecell(stderr, 0, n);
	fprintf(stderr, "\nSearch stopped by : ");
	printtreecell(stderr, 0, v);
	fprintf(stderr, "\n");
    }
    v = add_node_after(n);
    v->type = T_SUSP;

    opt_run_start = opt_run_end = tcalloc(1, sizeof*bfprog);
    opt_run_start->inum = 0;
    opt_run_start->type = T_NOP;
    if (verbose>3)
	fprintf(stderr, "Running trial run optimise\n");
    run_tree_noinp();
}

static int
update_opt_runner(struct bfi * n, int * mem, int offset)
{
    struct bfi *v = bfprog;
    int i, ntype;

    if (n && n->orgtype == T_WHL && UM(mem[offset + n->offset]) == 0) {
	/* Move the T_SUSP */
	int lp = 1;
	if (verbose>3)
	    fprintf(stderr, "Trial run optimise triggered on dead loop.\n");

	v = n->jmp;
	n = 0;

	while(v && v->type != T_INP && v->type != T_INPI && v->type != T_STOP) {
	    if (v->orgtype == T_END) lp--;
	    if(!lp && v->orgtype != T_WHL) n=v;
	    if (v->orgtype == T_WHL) lp++;
	    v=v->next;
	}

	if (verbose>5) {
	    fprintf(stderr, "Inserting T_SUSP node after: ");
	    printtreecell(stderr, 0, n);
	    fprintf(stderr, "\nSearch stopped by : ");
	    printtreecell(stderr, 0, v);
	    fprintf(stderr, "\n");
	}

	v = add_node_after(n);
	v->type = T_SUSP;
	return 1;
    }

    ntype = n?n->type:T_SUSP;

    if (verbose>3)
	fprintf(stderr, "Trial run optimise triggered on %s.\n",
	    ntype==T_STOP? "T_STOP node": "end");

    if (ntype == T_STOP) {
	struct bfi *n2;
	n2 = add_node_after(opt_run_end);
	n2->type = T_STOP;
	n2->line = n->line;
	n2->col = n->col;
	opt_run_end = n2;
    }

    while(ntype == T_STOP ? v!=0: v!=n) {
	bfprog = v->next;
	if (v->type == T_STR)
	    free(v->str);
	v->type = T_NOP;
	free(v);
	v = bfprog;
    }

    if (bfprog) {
	for(i=profile_min_cell; i<=profile_max_cell; i++) {
	    if (UM(mem[i]) != 0) {
		v = add_node_after(opt_run_end);
		v->type = T_SET;
		v->offset = i;
		if (cell_size == 8)
		    v->count = UM(mem[i]);
		else
		    v->count = SM(mem[i]);
		opt_run_end = v;
	    }
	}

	if (offset) {
	    v = add_node_after(opt_run_end);
	    v->type = T_MOV;
	    v->count = offset;
	    opt_run_end = v;
	}
    }

    if (opt_run_start && opt_run_start->type == T_NOP) {
	v = opt_run_start->next;
	if (v) v->prev = 0;
	v = opt_run_start;
	opt_run_start = opt_run_start->next;
	free(v);
    }

    if (opt_run_start) {
	if (opt_no_litprt && bfprog) {
	    struct bfi *v2 = add_node_after(opt_run_end);
	    v2->type = T_SET;
	    v2->count = 0;
	    opt_run_end = v2;
	}
	opt_run_end->next = bfprog;
	bfprog = opt_run_start;
	if (opt_run_end->next)
	    opt_run_end->next->prev = opt_run_end;
    }
    return 0;
}

/*
 * This is a simple tree based interpreter for -Orun
 */
static void
run_tree_noinp(void)
{
    int *p, *oldp;
    struct bfi * n = bfprog;

    oldp = p = map_hugeram();

    while(n){
	{
	    int off = (p+n->offset) - oldp;
	    if (n->type != T_MOV) {
		if (off < profile_min_cell) profile_min_cell = off;
		if (off > profile_max_cell) profile_max_cell = off;
	    }
	}

	switch(n->type)
	{
	    case T_MOV: p += n->count; break;
	    case T_ADD: p[n->offset] += n->count; break;
	    case T_SET: p[n->offset] = n->count; break;
	    case T_CALC:
		p[n->offset] = n->count
			    + n->count2 * p[n->offset2]
			    + n->count3 * p[n->offset3];
		if (n->count2) {
		    int off = (p+n->offset2) - oldp;
		    if (off < profile_min_cell) profile_min_cell = off;
		    if (off > profile_max_cell) profile_max_cell = off;
		}
		if (n->count3) {
		    int off = (p+n->offset3) - oldp;
		    if (off < profile_min_cell) profile_min_cell = off;
		    if (off > profile_max_cell) profile_max_cell = off;
		}
		break;

	    case T_CALCMULT:
		p[n->offset] = n->count
		    + (n->count2 * p[n->offset2] * n->count3 * p[n->offset3]);
		if (n->count2) {
		    int off = (p+n->offset2) - oldp;
		    if (off < profile_min_cell) profile_min_cell = off;
		    if (off > profile_max_cell) profile_max_cell = off;
		}
		if (n->count3) {
		    int off = (p+n->offset3) - oldp;
		    if (off < profile_min_cell) profile_min_cell = off;
		    if (off > profile_max_cell) profile_max_cell = off;
		}
		break;

	    case T_LT:
		p[n->offset] += (((unsigned) UM(p[n->offset2]))
			       < ((unsigned) UM(p[n->offset3])));

		if (n->count2) {
		    int off = (p+n->offset2) - oldp;
		    if (off < profile_min_cell) profile_min_cell = off;
		    if (off > profile_max_cell) profile_max_cell = off;
		}
		if (n->count3) {
		    int off = (p+n->offset3) - oldp;
		    if (off < profile_min_cell) profile_min_cell = off;
		    if (off > profile_max_cell) profile_max_cell = off;
		}
		break;

	    case T_DIV:
		p[n->offset] = UM(p[n->offset]);
		p[n->offset+1] = UM(p[n->offset+1]);
		if (p[n->offset+1] != 0) {
		    p[n->offset+2] = (unsigned)p[n->offset] % p[n->offset+1];
		    p[n->offset+3] = (unsigned)p[n->offset] / p[n->offset+1];
		} else {
		    p[n->offset+2] = p[n->offset];
		    p[n->offset+3] = 0;
		}
		break;

	    case T_IF: case T_MULT: case T_CMULT:

	    case T_WHL: if(UM(p[n->offset]) == 0) n=n->jmp;
		break;

	    case T_END: if(UM(p[n->offset]) != 0) n=n->jmp;
		break;

	    case T_ENDIF:
		break;

	    case T_PRT:
	    case T_CHR:
		{
		    int ch = (n->type == T_PRT?UM(p[n->offset]):n->count);
		    if ( opt_run_end->type == T_STR &&
			(ch < 128 || ch > UCSMAX || iostyle != 1))
		    {
			struct bfi *v = opt_run_end;
			ch = (signed char) ch;

			if (v->str->length >= v->str->maxlen) {
			    struct string *nstr;
			    nstr = realloc(v->str, v->str->maxlen*2+sizeof*nstr);
			    if (!nstr) {
				perror("Extending string");
				exit(1);
			    }
			    nstr->maxlen *= 2;
			    v->str = nstr;
			}
			v->str->buf[v->str->length++] = ch;
			v->count = v->str->length;
			break;
		    }
		}
		/*FALLTHROUGH*/
	    case T_STR:
		{
		    struct bfi *v = add_node_after(opt_run_end);
		    v->type = T_CHR;
		    v->line = n->line;
		    v->col = n->col;
		    v->count = (n->type == T_PRT?UM(p[n->offset]):n->count);
		    opt_run_end = v;
		    if (n->type == T_STR) {
			v->type = T_STR;
			v->str = tcalloc(1, n->str->maxlen+sizeof*(n->str));
			memcpy(v->str, n->str, n->str->maxlen+sizeof*(n->str));
		    } else if (opt_no_litprt) {
			v->type = T_SET;
			v = add_node_after(opt_run_end);
			v->type = T_PRT;
			v->line = n->line;
			v->col = n->col;
			opt_run_end = v;
		    } else
			if (v->count < 128 || v->count > UCSMAX || iostyle != 1)
			    v->count = (signed char) v->count;
		}
		break;

	    case T_NOP:
	    case T_DUMP:
		break;

	    case T_SUSP:
		n=n->next;
		/*FALLTHRU*/
	    case T_STOP:
		if( update_opt_runner(n, oldp, p-oldp) )
		    continue;
		else
		    goto break_break;

	    default:
		printtreecell(stderr, 0, n);
		fprintf(stderr, "\nTrial run execution error:\n"
		       "Bad node: type %d: ptr+%d, cnt=%d.\n",
			n->type, n->offset, n->count);
		exit(1);
	}
	n = n->next;
    }

break_break:;
}
