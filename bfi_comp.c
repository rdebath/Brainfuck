/*
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "bfi.tree.h"
#include "bfi.run.h"

static int acc_loaded = 0;
static int acc_offset = 0;
static int acc_dirty = 0;
static int acc_hi_dirty = 0;

static void
clean_acc(void)
{
    if (acc_loaded && acc_dirty) {
	XXX Save ACC to acc_offset

	acc_dirty = 0;
    }
}

static void
set_acc_offset(int offset)
{
    if (acc_loaded && acc_dirty && acc_offset != offset)
	clean_acc();

    acc_offset = offset;
    acc_loaded = 1;
    acc_dirty = 1;
    acc_hi_dirty = 1;
}

static void
load_acc_offset(int offset)
{
    if (acc_loaded) {
	if (acc_offset == offset) return;
	if (acc_dirty)
	    clean_acc();
    }

    acc_offset = offset;

    XXX Load acc from offset;

    acc_loaded = 1;
    acc_dirty = 0;
    acc_hi_dirty = (tape_step*8 != cell_size);
}

static void puts_without_nl(char * s) { fputs(s, stdout); }

static void failout(void) { fprintf(stderr, "STOP Command executed.\n"); exit(1); }

static void
save_ptr_for_free(void * memp)
{
    /* TODO */
}

void 
compile_to_code(void)
{
    struct bfi * n = bfprog;
    int maxstack = 0, stackptr = 0;
    int * loopstack = 0;

    calculate_stats();

    prolog();

    while(n)
    {
	switch(n->type)
	{
	case T_MOV:
	    clean_acc();
	    acc_loaded = 0;

	    XXX PTR += n->count
	    break;

	case T_ADD:
	    load_acc_offset(n->offset);
	    set_acc_offset(n->offset);

	    XXX ACC += n->count;
	    break;

	case T_SET:
	    set_acc_offset(n->offset);
	    XXX ACC = n->count;
	    break;

	case T_CALC:
	    if (n->offset == n->offset2 && n->count2 == 1) {
		load_acc_offset(n->offset);
		set_acc_offset(n->offset);
		if (n->count)
		    jit_addi(REG_ACC, REG_ACC, n->count);
	    } else if (n->count2 != 0) {
		load_acc_offset(n->offset2);
		set_acc_offset(n->offset);
		if (n->count2 == -1)
		    jit_negr(REG_ACC, REG_ACC);
		else if (n->count2 != 1)
		    jit_muli(REG_ACC, REG_ACC, n->count2);
		if (n->count)
		    jit_addi(REG_ACC, REG_ACC, n->count);
	    } else {
		clean_acc();
		set_acc_offset(n->offset);

		jit_movi(REG_ACC, n->count);
		if (n->count2 != 0) {
		    if (tape_step > 1)
			jit_ldxi_i(REG_A1, REG_P, n->offset2 * tape_step); 
		    else
			jit_ldxi_uc(REG_A1, REG_P, n->offset2); 
		    if (n->count2 == -1)
			jit_negr(REG_A1, REG_A1);
		    else if (n->count2 != 1)
			jit_muli(REG_A1, REG_A1, n->count2);
		    jit_addr(REG_ACC, REG_ACC, REG_A1);
		}
	    }

	    if (n->count3 != 0) {
		if (tape_step > 1)
		    jit_ldxi_i(REG_A1, REG_P, n->offset3 * tape_step); 
		else
		    jit_ldxi_uc(REG_A1, REG_P, n->offset3); 
		if (n->count3 == -1)
		    jit_negr(REG_A1, REG_A1);
		else if (n->count3 != 1)
		    jit_muli(REG_A1, REG_A1, n->count3);
		jit_addr(REG_ACC, REG_ACC, REG_A1);
	    }
	    break;

	case T_IF: case T_MULT: case T_CMULT:
	case T_MFIND: case T_ZFIND: case T_ADDWZ: case T_FOR:
	case T_WHL:
	    load_acc_offset(n->offset);
	    clean_acc();

	    if (stackptr >= maxstack) {
		loopstack = realloc(loopstack,
			    ((maxstack+=32)+2)*sizeof(*loopstack));
		if (loopstack == 0) {
		    perror("loop stack realloc failure");
		    exit(1);
		}
	    }

	    if (cell_mask > 0 && acc_hi_dirty) {
		XXX ACC &= cell_mask
	    }

	    XXX JMPZ 0x00000000

	    loopstack[stackptr] = ADDR offset
	    stackptr ++;
	    break;

	case T_END:
	    load_acc_offset(n->offset);
	    clean_acc();

	    stackptr --;
	    if (stackptr < 0) {
		fprintf(stderr, "Code gen failure: Stack pointer negative.\n");
		exit(1);
	    }

	    if (cell_mask > 0 && acc_hi_dirty) {
		XXX ACC &= cell_mask
	    }

	    XXX JMPZ 0x00000000
	    XXX PATCH loopstack[stackptr] 

	    break;

	case T_ENDIF:
	    clean_acc();
	    acc_loaded = 0;

	    stackptr --;
	    if (stackptr < 0) {
		fprintf(stderr, "Code gen failure: Stack pointer negative.\n");
		exit(1);
	    }
	    XXX PATCH loopstack[stackptr] 
	    break;

	case T_PRT:
	    clean_acc();
	    if (n->count == -1) {
		load_acc_offset(n->offset);
		acc_loaded = 0;

		if (cell_mask > 0 && acc_hi_dirty) {
		    XXX ACC &= cell_mask
		}

		XXX putchar(ACC)
		break;
	    }
	    acc_loaded = 0;

	    if (n->count <= 0 || (n->count >= 127 && iostyle == 1) || 
		    !n->next || n->next->type != T_PRT) {
		XXX ACC =  n->count
		XXX putchar(ACC)
	    } else {
		int i = 0, j;
		struct bfi * v = n;
		char *s, *p;
		while(v->next && v->next->type == T_PRT &&
			v->next->count > 0 &&
			    (v->next->count < 127 || iostyle != 1)) {
		    v = v->next;
		    i++;
		}
		p = s = malloc(i+2);

		for(j=0; j<i; j++) {
		    *p++ = n->count;
		    n = n->next;
		}
		*p++ = n->count;
		*p++ = 0;

		XXX Print string (s)
		
		free(s);
		// OR save_ptr_for_free(s);
	    }
	    break;

	case T_INP:
	    load_acc_offset(n->offset);
	    set_acc_offset(n->offset);

	    XXX ACC = getchar()
	    break;

	case T_STOP:
	    XXX EXIT @ INFINITY
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
	    break;
	}
	n=n->next;
    }

    XXX END OF CODE
    XXX RUN CODE

    XXX Free memory
}
