/*
 * This file contains two interpretation engines, both are slow, but both
 * have large cells with few limitations.
 *
 * The first "maxtree" uses the largest builtin C type it can find for
 * the basic cell with a bitmask so that it can use any bitsize upto
 * that size.
 *
 * The second "supertree" uses the openssl BIGNUM implementation to
 * support any bitsize upto physical memory limits.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "bfi.tree.h"
#include "bfi.run.h"
#include "bfi.runmax.h"
#include "clock.h"

#ifdef __STDC__
#include <limits.h>
/* LLONG_MAX came in after inttypes.h, limits.h is very old. */
#if _POSIX_VERSION >= 199506L || defined(LLONG_MAX)
#include <inttypes.h>
#endif
#endif

#ifndef C
#ifdef __SIZEOF_INT128__
#define C unsigned __int128
#else
#if defined(ULLONG_MAX) || defined(__LONG_LONG_MAX__)
#define C unsigned long long
#else
#if defined(UINTMAX_MAX)
#define C uintmax_t
#else
#define C unsigned long
#endif
#endif
#endif
#endif

static void run_supertree(void);

/*
 * This is a simple tree based interpreter with big cells.
 */
void
run_maxtree(void)
{
    C *p, mask = 0;
    struct bfi * n = bfprog;

    mask = ~mask;
    if (cell_length > (int)(sizeof(C))*CHAR_BIT) {
#ifndef DISABLE_BN
	run_supertree();
	return;
#else
	fprintf(stderr,
		"ERROR: Cell size of %dbits requires big number type\n",
		cell_length);
	exit(1);
#endif
    }
    if (cell_length < (int)(sizeof(C))*CHAR_BIT) {
	mask = ~( ((C)1) << cell_length);
    }

    only_uses_putch = 1;
    p = map_hugeram();
    start_runclock();

    while(n) {
	switch(n->type)
	{
	    case T_MOV: p += n->count; break;
	    case T_ADD: p[n->offset] += n->count; break;
	    case T_SET: p[n->offset] = n->count; break;
	    case T_CALC:
		p[n->offset] = n->count
			    + n->count2 * p[n->offset2]
			    + n->count3 * p[n->offset3];
		break;

	    case T_IF: case T_MULT: case T_CMULT:

	    case T_WHL: if((p[n->offset] & mask) == 0) n=n->jmp;
		break;

	    case T_END: if((p[n->offset] & mask) != 0) n=n->jmp;
		break;

	    case T_ENDIF:
		break;

	    case T_CHR:
		putch(n->count);
		break;
	    case T_PRT:
		putch(p[n->offset]);
		break;
	    case T_INP:
		{   /* Cell may be too large for an int */
		    int ch = -256;
		    ch = getch(ch);
		    if (ch != -256) p[n->offset] = ch;
		}
		break;

	    case T_STOP:
		fprintf(stderr, "STOP Command executed.\n");
		exit(1);

	    case T_NOP:
	    case T_DUMP:
		break;

	    default:
		fprintf(stderr, "Execution error:\n"
		       "Bad node: type %d: ptr+%d, cnt=%d.\n",
			n->type, n->offset, n->count);
		exit(1);
	}
	n = n->next;
    }

    finish_runclock(&run_time, &io_time);
}

/*
 * Another simple tree based interpreter, but with unlimited cells.
 */

#ifndef DISABLE_BN

#include <openssl/bn.h>

typedef BIGNUM BIGNUM_V[1];

static BIGNUM_V * mem = 0;
static int dyn_memsize = 0;
#define MINALLOC 16

static
BIGNUM_V *
alloc_ptr(BIGNUM_V *p)
{
    int amt, memoff, i, off;
    if (p >= mem && p < mem+dyn_memsize) return p;

    memoff = p-mem; off = 0;
    if (memoff<0) off = -memoff;
    else if(memoff>=dyn_memsize) off = memoff-dyn_memsize;
    amt = off / MINALLOC;
    amt = (amt+1) * MINALLOC;
    mem = realloc(mem, (dyn_memsize+amt)*sizeof(*mem));
    if (memoff<0) {
        memmove(mem+amt, mem, dyn_memsize*sizeof(*mem));
        for(i=0; i<amt; i++)
            BN_init(mem[i]);
        memoff += amt;
    } else {
        for(i=0; i<amt; i++)
            BN_init(mem[dyn_memsize+i]);
    }
    dyn_memsize += amt;
    return mem+memoff;
}

static inline BIGNUM_V *
move_ptr(BIGNUM_V *p, int off) {
    p += off;
    if (off>=0 && p+max_pointer >= mem+dyn_memsize)
        p = alloc_ptr(p+max_pointer)-max_pointer;
    if (off<=0 && p+min_pointer <= mem)
        p = alloc_ptr(p+min_pointer)-min_pointer;
    return p;
}

static void
run_supertree(void)
{
    struct bfi * n = bfprog;
    register BIGNUM_V * m = move_ptr(alloc_ptr(mem),0);
    BIGNUM_V t1, t2, t3;
    BN_init(t1); BN_init(t2); BN_init(t3);

    only_uses_putch = 1;
    start_runclock();

    while(n) {
	switch(n->type)
	{
	    case T_MOV: m = move_ptr(m, n->count); break;
	    case T_ADD:
		// p[n->offset] += n->count;
		if (n->count >= 0) {
		    BN_add_word(m[n->offset], n->count);
		} else {
		    BN_sub_word(m[n->offset], -n->count);
		}
		break;
	    case T_SET:
		// p[n->offset] = n->count;
		if (n->count >= 0) {
		    BN_set_word(m[n->offset], n->count);
		} else {
		    BN_zero(m[n->offset]);
		    BN_sub_word(m[n->offset], -n->count);
		}
		break;
	    case T_CALC:
		// p[n->offset] = n->count
		//	    + n->count2 * p[n->offset2]
		//	    + n->count3 * p[n->offset3];

		if (n->count >= 0) {
		    BN_set_word(t1, n->count);
		} else {
		    BN_zero(t1);
		    BN_sub_word(t1, -n->count);
		}

		if (n->count2 == 1) {
		    BN_add(t1, t1, m[n->offset2]);
		} else if (n->count2 > 0) {
		    BN_copy(t2, m[n->offset2]);
		    BN_mul_word(t2, n->count2);
		    BN_add(t1, t1, t2);
		} else if (n->count2 < 0) {
		    BN_copy(t2, m[n->offset2]);
		    BN_mul_word(t2, -n->count2);
		    BN_sub(t1, t1, t2);
		}

		if (n->count3 == 1) {
		    BN_add(t1, t1, m[n->offset3]);
		} else if (n->count3 > 0) {
		    BN_copy(t3, m[n->offset3]);
		    BN_mul_word(t3, n->count3);
		    BN_add(t1, t1, t3);
		} else if (n->count3 < 0) {
		    BN_copy(t3, m[n->offset3]);
		    BN_mul_word(t3, -n->count3);
		    BN_sub(t1, t1, t3);
		}

		BN_copy(m[n->offset], t1);
		break;

	    case T_IF: case T_MULT: case T_CMULT:

	    case T_WHL:
		BN_mask_bits(m[n->offset], cell_length);
		if( BN_is_zero(m[n->offset]) ) n=n->jmp;
		break;

	    case T_END:
		BN_mask_bits(m[n->offset], cell_length);
		if(!BN_is_zero(m[n->offset]) ) n=n->jmp;
		break;

	    case T_ENDIF:
		break;

	    case T_CHR:
		putch(n->count);
		break;
	    case T_PRT:
		{
		    int input_chr;
		    BN_zero(t2);
		    input_chr = BN_get_word(m[n->offset]);
		    if(BN_cmp(m[n->offset],t2) < 0) input_chr = -input_chr;
		    putch(input_chr);
		}
		break;
	    case T_INP:
		{   /* Cell is too large for an int */
		    int ch = -256;
		    ch = getch(ch);
		    if (ch > 0)
			BN_set_word(m[n->offset], ch);
		    else if (ch != -256) {
			BN_zero(m[n->offset]);
			BN_sub_word(m[n->offset], -ch);
		    }
		}
		break;

	    case T_STOP:
		fprintf(stderr, "STOP Command executed.\n");
		exit(1);

	    case T_NOP:
	    case T_DUMP:
		break;

	    default:
		fprintf(stderr, "Execution error:\n"
		       "Bad node: type %d: ptr+%d, cnt=%d.\n",
			n->type, n->offset, n->count);
		exit(1);
	}
	n = n->next;
    }

    finish_runclock(&run_time, &io_time);

    /* TODO: free the tape */
}

#endif
