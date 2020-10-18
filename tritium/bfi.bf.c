
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "bfi.tree.h"
#include "bfi.bf.h"

static int col = 0;

static void
pc(int ch) { if (col>=72) { putchar('\n'); col=0; } col++; putchar(ch); }

static void
ps(const char * p) {
    if (!p || col+strlen(p) >= 72) {putchar('\n'); col = 0;}
    while(p&&*p) pc(*p++);
}

static void
pr(char ch, int r) {
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
	printf("[ BF regenerated from %s ]\n", bfname);

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
	    ps("[-]");
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
	    break;

	case T_END:
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

