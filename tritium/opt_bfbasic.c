#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "bfi.tree.h"

void
bf_basic_hack()
{
    struct bfi *n = bfprog;
    int indent = 0, i;

    while(n)
    {
	/* This is something that's always true for BF Basic generated
	 * code. The 'while-switch' loop takes the first few cells and
	 * next seven are temps that will always be left at zero. This
	 * fragment explicitly zeros those cells so the optimiser can
	 * easily see this is true.
	 *
	 * QGT0123456
	 * @_Q  0 = quit (end), 1 = otherwise
	 * @_G  0 = performing a jump (goto), 1 = otherwise
	 */

	switch(n->type)
	{
	case T_END:
	case T_ENDIF:
	    /* if (indent == 2 && n->offset == 2)
		; * These loops are always T_IF loops. */

	    indent--;
	    break;

	case T_MULT: case T_CMULT:
	case T_IF:
	case T_WHL:
	    indent++;

	    /* The outer loop is on "G", the second is on "T" */
	    if (indent != 2 || n->offset != 2)
		break;

	    /* These explicitly set the value that should already
	     * exist at this location */

	    n = add_node_after(n);
	    n->type = T_SET;
	    n->count = 1;
	    n->offset = 2;

	    for(i=3; i<10; i++) {
		n = add_node_after(n);
		n->type = T_SET;
		n->count = 0;
		n->offset = i;
	    }
	    break;
	}
	n=n->next;
    }
}
