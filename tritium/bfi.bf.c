
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "bfi.tree.h"
#include "bfi.bf.h"

static int BFBasic = 0;
static int bfrle = 0;
static int col = 0;

int
checkarg_bf(char * opt, char * arg UNUSED)
{
    if (!strcmp(opt, "-fbfbasic")) { BFBasic = 1; return 1; }
    if (!strcmp(opt, "-fbftif")) { BFBasic = 2; return 1; }
    if (!strcmp(opt, "-bfrle")) { bfrle = 1; return 1; }
    return 0;
}

static void
pc(int ch) { if (col>=72) { putchar('\n'); col=0; } col++; putchar(ch); }

static void
ps(const char * p) {
    if (!p || col+strlen(p) >= 72) {putchar('\n'); col = 0;}
    while(p&&*p) pc(*p++);
}

static void
pr(char ch, int r) {
    char wbuf[sizeof(int)*3+8];
    if (bfrle && r>1) {
	sprintf(wbuf, "%d%c", r, ch);
	ps(wbuf);
    } else
	while(r-->0) pc(ch);
}

/*
 * Regenerate some BF code.
 *
 * Most optimised tokens cannot be generated because the code would need
 * temp cells on the tape, but the information as to which temps were used
 * has been discarded. However, we could use any cell who's value is known.
 *
 * You may have to reduce the optimisation to -O1
 */
void
print_bf(void)
{
    struct bfi *n = bfprog;
    int i, last_offset = 0;

    if (!noheader)
	printf("[ BF%s regenerated from %s ]\n", bfrle?"-RLE":"", bfname);

    while(n)
    {
	if (n->type == T_MOV) {
	    last_offset -= n->count;
	    n = n->next;
	    continue;
	}

	if(n->offset>last_offset)
	    pr('>', n->offset-last_offset);
	if(n->offset<last_offset)
	    pr('<', last_offset-n->offset);
	last_offset = n->offset;

	switch(n->type)
	{
	case T_SET:
	    if (bfrle) pc('='); else ps("[-]");
	    i = n->count;
	    if(i>0) pr('+', i);
	    if(i<0) pr('-', -i);
	    break;

	case T_ADD:
	    i = n->count;
	    if(i>0) pr('+', i);
	    if(i<0) pr('-', -i);
	    break;

	case T_PRT:
	    pc('.');
	    break;

	case T_INP:
	    pc(',');
	    break;

	case T_MULT: case T_CMULT:
	case T_IF:
	case T_WHL:
	    pc('[');
	    /* This is something that's always true for BF Basic generated
	     * code. The 'while-switch' loop takes the first few cells and
	     * next seven are temps that will always be left at zero. This
	     * fragment explicitly zeros those cells so the optimiser can
	     * easily see this is true. */
	    if (BFBasic)
		if (n->offset == 2) {
		    if (BFBasic == 1)
			ps("[-]+> [-]>[-]>[-]>[-]>[-]>[-]>[-]><<<<<<< <");
		    else
			ps("[-]+> []>[]>[]>[]>[]>[]>[]><<<<<<< <");
		}
	    break;

	case T_END:
	    if (BFBasic > 1)
		if (n->offset == 2)
		    ps("[]");
	    pc(']');
	    break;

	case T_STOP:
	    ps("[-]+[]");
	    break;

	case T_DUMP:
	    pc('#');
	    break;

	case T_NOP:
	    break;

	default:
	    fprintf(stderr, "Error on code generation:\n"
	           "Bad node: %s ptr+%d, cnt=%d.\n",
		    tokennames[n->type], n->offset, n->count);
	    fprintf(stderr, "Optimisation level probably too high for BF output\n");
	    exit(1);
	}
	n=n->next;
    }
    ps(0);
}

