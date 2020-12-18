
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "bfi.tree.h"
#include "bfi.run.h"
#include "bfi.runarray.h"
#include "clock.h"
#include "big_int.h"

#define FNAME run_intprog
#define icell   unsigned int
#include "bfi.execloop.h"
#undef FNAME
#undef icell

#define FNAME run_charprog
#define icell   unsigned char
#include "bfi.execloop.h"
#undef FNAME
#undef icell

#define FNAME run_maskprog
#define icell   unsigned int
#define DYNAMIC_MASK
#include "bfi.execloop.h"
#undef FNAME
#undef icell
#undef DYNAMIC_MASK

#define FNAME run_supermask
#define icell   uintbig_t
#define EXTENDED_MASK
#define COOLFUNC
#include "bfi.execloop.h"
#undef FNAME
#undef icell
#undef EXTENDED_MASK

void
run_tree_as_array(void)
{
    int * progarray = convert_tree_to_runarray(1);

    delete_tree();
    start_runclock();
    if (cell_size == CHAR_BIT) {
	run_charprog(progarray, map_hugeram());
    } else if (cell_size == sizeof(int)*CHAR_BIT) {
	run_intprog(progarray, map_hugeram());
    } else if (cell_length <= sizeof(int)*CHAR_BIT) {
	run_maskprog(progarray, map_hugeram());
    } else {
	run_supermask(progarray, map_hugeram());
    }
    finish_runclock(&run_time, &io_time);
    free(progarray);
}

int *
convert_tree_to_runarray(int merge_mov)
{
    struct bfi * n = bfprog;
    size_t arraylen = 0;
    int * progarray = 0;
    int * p;
    int last_offset = 0;

    only_uses_putch = 1;

    while(n)
    {
	switch(n->type)
	{
	case T_MOV:
	    if (!merge_mov) arraylen += 2;
	    break;

	case T_CALC:
	    arraylen += 7;
	    break;

	case T_CALCMULT: case T_LT:
	    arraylen += 4;
	    break;

	case T_DIV:
	    arraylen += 2;
	    break;

	case T_STR:
	    arraylen += 3+3*n->str->length;
	    break;

	default:
	    arraylen += 3;
	    break;
	}
	n = n->next;
    }

    p = progarray = calloc(arraylen+2, sizeof*progarray);
    if (!progarray) { perror("calloc"); exit(1); }
    n = bfprog;

    last_offset = 0;
    while(n)
    {
#ifdef ARRAY_DUMP
	int * pp = p;
#endif

	if (merge_mov) {
	    if (n->type != T_MOV) {
		*p++ = (n->offset - last_offset);
		last_offset = n->offset;
	    }
	    else {
		last_offset -= n->count;
		n = n->next;
		continue;
	    }
	}

	*p++ = n->type;
	switch(n->type)
	{
	case T_INP: case T_INPI: case T_PRT: case T_PRTI:
	    if (!merge_mov) *p++ = n->offset;
	    break;

	case T_CHR:
	    *p++ = n->count;
	    break;

	case T_STR:
	    {
		int i;
		p[-1] = T_CHR;
		*p++ = n->str->buf[0];
		for (i=1; i<n->str->length; i++) {
		    if (merge_mov) *p++ = 0;
		    *p++ = T_CHR;
		    *p++ = n->str->buf[i];
		}
	    }
	    break;

	case T_ADD: case T_SET:
	    if (!merge_mov) *p++ = n->offset;
	    *p++ = n->count;
	    break;

	case T_MOV:
	    *p++ = n->count;
	    break;

	case T_MULT: case T_CMULT:
	    /* Note: for these I could generate multiply code for then enclosed
	     * T_ADD instructions, but that will only happen if the optimiser
	     * section is turned off.
	     */
	    /*FALLTHROUGH*/

	case T_WHL:
	    if (opt_level>0) {
		struct bfi *n1, *n2=0, *n3=0, *n4=0;
		n1 = n->next;
		if (n1) n2 = n1->next;
		if (n2) n3 = n2->next;
		if (n3) n4 = n3->next;

		if (n1->type == T_MOV && n1->count != 0){
		    /* Look for [<<<] */

		    if (n2->type == T_END) {
			p[-1] = T_ZFIND;
			if (!merge_mov) *p++ = n->offset;
			*p++ = n1->count;
			n = n->jmp;
			break;
		    }
		}

		if (n1->type == T_ADD && n2->type == T_MOV && n3->type == T_END) {
		    p[-1] = T_ADDWZ;
		    if (!merge_mov) *p++ = n->offset;
		    *p++ = n1->offset - last_offset;
		    *p++ = n1->count;
		    *p++ = n2->count;
		    n = n->jmp;
		    break;
		}

		if (n1->type == T_ADD && n2->type == T_ADD &&
		    n3->type == T_MOV && n4->type == T_END &&
		    n1->count == -1 && n2->count == 1 &&
		    n->offset == n1->offset &&
		    n->offset == n2->offset - n3->count
		) {
		    p[-1] = T_MFIND;
		    if (!merge_mov) *p++ = n->offset;
		    *p++ = n3->count;
		    n = n->jmp;
		    break;
		}
	    }

	    /* Storing the location of the instruction in the T_END's count
	     * field; it's not normally used */
	    if (!merge_mov) *p++ = n->offset;
	    n->jmp->count = p-progarray;
	    *p++ = 0;
	    break;

	case T_IF:
	    p[-1] = T_WHL;
	    if (!merge_mov) *p++ = n->offset;
	    n->jmp->count = p-progarray;
	    *p++ = 0;
	    break;

	case T_ENDIF:
	    if (!merge_mov) p--;
	    else if (p[-2] == 0) p -= 2;
	    progarray[n->count] = (p-progarray) - n->count -1;
	    break;

	case T_END:
	    if (!merge_mov) *p++ = n->offset;
	    progarray[n->count] = (p-progarray) - n->count;
	    *p++ = -progarray[n->count];
	    break;

	case T_CALC:
	    if (n->count3 == 0) {
		if (n->count == 0 && n->count2 == 1) {
		    /*  m[off] = m[off2] */
		    p[-1] = T_CALC4;
		    if (!merge_mov) *p++ = n->offset;
		    *p++ = n->offset2 - last_offset;
		} else {
		    /*  m[off] = m[off2]*count2 + count2 */
		    p[-1] = T_CALC2;
		    if (!merge_mov) *p++ = n->offset;
		    *p++ = n->count;
		    *p++ = n->offset2 - last_offset;
		    *p++ = n->count2;
		}
	    } else if ( n->count == 0 && n->count2 == 1 && n->offset == n->offset2 ) {
		if (n->count3 == 1) {
		    /*  m[off] += m[off3] */
		    p[-1] = T_CALC5;
		    if (!merge_mov) *p++ = n->offset;
		    *p++ = n->offset3 - last_offset;
		} else {
		    /*  m[off] += m[off3]*count3 */
		    p[-1] = T_CALC3;
		    if (!merge_mov) *p++ = n->offset;
		    *p++ = n->offset3 - last_offset;
		    *p++ = n->count3;
		}
	    } else {
		if (!merge_mov) *p++ = n->offset;
		*p++ = n->count;
		*p++ = n->offset2 - last_offset;
		*p++ = n->count2;
		*p++ = n->offset3 - last_offset;
		*p++ = n->count3;
	    }
	    break;

	case T_CALCMULT:
	    if (!merge_mov) *p++ = n->offset;
	    *p++ = n->offset2 - last_offset;
	    *p++ = n->offset3 - last_offset;

	    if (n->count != 0 || n->count2 != 1 || n->count3 != 1)
		fprintf(stderr, "Invalid %s node counts found\n", tokennames[n->type]);
	    break;

	case T_LT:
	    if (!merge_mov) *p++ = n->offset;
	    *p++ = n->offset2 - last_offset;
	    *p++ = n->offset3 - last_offset;

	    if (n->count != 0 || n->count2 != 1 || n->count3 != 1)
		fprintf(stderr, "Invalid %s node counts found\n", tokennames[n->type]);
	    break;

	case T_DIV:
	    if (!merge_mov) *p++ = n->offset;
	    break;

	case T_STOP:
	    break;

	case T_NOP:
	    if (!merge_mov) p--;
	    break;

	default:
	    fprintf(stderr, "Invalid node type found = %s\n", tokennames[n->type]);
	    exit(1);
	}

#ifdef ARRAY_DUMP
	if (verbose>1) {
	    fprintf(stderr, "%4d:", p-progarray);
	    if (merge_mov) {
		if (pp[0])
		    fprintf(stderr, "T_MOV(%d): ", pp[0]);

		if (pp[1] >= 0 && pp[1] < TCOUNT)
		    fprintf(stderr, "%s", tokennames[pp[1]]);
		else
		    fprintf(stderr, "TOKEN(%d)", pp[1]);

		for(pp+=2; pp<p; pp++) fprintf(stderr, " %d", pp[0]);

		if(n->line || n->col)
		    fprintf(stderr, " @(%d,%d)", n->line, n->col);

		fprintf(stderr, "\n");
	    } else {
		if (pp[0] >= 0 && pp[0] < TCOUNT)
		    fprintf(stderr, "%s", tokennames[pp[0]]);
		else
		    fprintf(stderr, "TOKEN(%d)", pp[0]);

		for(pp+=1; pp<p; pp++) fprintf(stderr, " %d", pp[0]);

		if(n->line || n->col)
		    fprintf(stderr, " @(%d,%d)", n->line, n->col);

		fprintf(stderr, "\n");
	    }
	}
#endif

	n = n->next;
    }
    if (merge_mov) *p++ = 0;
    *p++ = T_FINI;

    return progarray;
}
