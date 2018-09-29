/*
 * This file contains three interpretation engines, they are slow,
 * but have large cells with few limitations.
 *
 * The first "maxtree" uses the largest builtin C type it can find for
 * the basic cell with a bitmask so that it can use any bitsize upto
 * that size.
 *
 * The second "supertree" uses the openssl BIGNUM implementation to
 * support any bitsize upto physical memory limits.
 *
 * The third is an alternate "supertree" which doesn't use any library.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

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
#ifdef _UINT128_T
#define C __uint128_t
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
	run_supertree();
	return;
    }
#if !defined(DISABLE_BN)
    if (iostyle == 3) {
	run_supertree();
	return;
    }
#endif
    if (cell_length < (int)(sizeof(C))*CHAR_BIT) {
	mask = ~( ((C)-1) << cell_length);
    }

    if (verbose)
	fprintf(stderr, "Maxtree variant: %d byte int/cell\n",
			(int)sizeof(C));

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

/******************************************************************************/
/*
 * Another simple tree based interpreter, but with unlimited cells.
 */

#if !defined(DISABLE_BN)

#include <openssl/bn.h>
#include <openssl/crypto.h>

static BIGNUM ** mem = 0;
static int dyn_memsize = 0;
#define MINALLOC 16

static
BIGNUM **
alloc_ptr(BIGNUM **p)
{
    int amt, memoff, i, off;
    BIGNUM ** nmem;
    if (p >= mem && p < mem+dyn_memsize) return p;

    memoff = p-mem; off = 0;
    if (memoff<0) off = -memoff;
    else if(memoff>=dyn_memsize) off = memoff-dyn_memsize;
    amt = off / MINALLOC;
    amt = (amt+1) * MINALLOC;
    nmem = realloc(mem, (dyn_memsize+amt)*sizeof(*mem));
    if (!nmem) {
	if(mem) free(mem);
	fprintf(stderr, "Memory overflow when expanding tape.\n");
	exit(99);
    } else mem=nmem;
    if (memoff<0) {
        memmove(mem+amt, mem, dyn_memsize*sizeof(*mem));
        for(i=0; i<amt; i++)
            mem[i] = BN_new();
        memoff += amt;
    } else {
        for(i=0; i<amt; i++)
            mem[dyn_memsize+i] = BN_new();
    }
    dyn_memsize += amt;
    return mem+memoff;
}

static inline BIGNUM **
move_ptr(BIGNUM **p, int off) {
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
    register BIGNUM ** m = move_ptr(alloc_ptr(mem),0);
    BIGNUM *t1 = BN_new(), *t2 = BN_new(), *t3 = BN_new();
    int do_mask = (cell_length>0 && cell_length<INT_MAX);

    if (verbose)
	fprintf(stderr, "Maxtree variant: using OpenSSL Bignums\n");

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

		if(do_mask)
		    BN_mask_bits(t1, cell_length);
		BN_copy(m[n->offset], t1);
		break;

	    case T_IF: case T_MULT: case T_CMULT:

	    case T_WHL:
		if(do_mask)
		    BN_mask_bits(m[n->offset], cell_length);
		if( BN_is_zero(m[n->offset]) ) n=n->jmp;
		break;

	    case T_END:
		if(do_mask)
		    BN_mask_bits(m[n->offset], cell_length);
		if(!BN_is_zero(m[n->offset]) ) n=n->jmp;
		break;

	    case T_ENDIF:
		break;

	    case T_CHR:
		putch(n->count);
		break;
	    case T_PRT:
		if (iostyle == 3) {
		    char * dec_num;
		    if(do_mask)
			BN_mask_bits(m[n->offset], cell_length);
		    dec_num = BN_bn2dec(m[n->offset]);
		    printf("%s\n", dec_num);
		    OPENSSL_free(dec_num);
		} else {
		    int input_chr;
		    BN_zero(t2);
		    input_chr = BN_get_word(m[n->offset]);
		    if(BN_cmp(m[n->offset],t2) < 0) input_chr = -input_chr;
		    putch(input_chr);
		}
		break;
	    case T_INP:
		if (iostyle == 3) {
		    char * bntxtptr = 0;
		    int rv;
		    rv = scanf("%ms", &bntxtptr);
		    if (rv == EOF || rv == 0) {
			int c = 1;
			switch(eofcell)
			{
			case 2: c = -1; break;
			case 3: c = 0; break;
			case 4: c = EOF; break;
			}
			if (c<=0)
			    BN_zero(m[n->offset]);
			if (c<0)
			    BN_sub_word(m[n->offset], -c);
		    } else {
			BN_dec2bn(&m[n->offset], bntxtptr);
			if(do_mask)
			    BN_mask_bits(m[n->offset], cell_length);
			free(bntxtptr);
		    }
		} else {   /* Cell is too large for an int */
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

/******************************************************************************/

#ifdef DISABLE_BN

#if defined(__SIZEOF_INT128__) && defined(UINT64_MAX)
typedef unsigned __int128 uint_long;
typedef uint64_t uint_cell;
#elif defined(_UINT128_T) && defined(UINT64_MAX)
typedef __uint128_t uint_long;
typedef uint64_t uint_cell;
#elif defined(UINT64_MAX) && defined(UINT32_MAX) && (UINT32_MAX >= UINT_MAX)
typedef uint64_t uint_long;
typedef uint32_t uint_cell;
#elif defined(WORD_BIT) && defined(LONG_BIT) && (WORD_BIT*2 <= LONG_BIT)
typedef unsigned long uint_long;
typedef unsigned int uint_cell;
#else
typedef C uint_cell;
#define NO_BIG_INT
#endif

/*
 * Another simple tree based interpreter, this one is definitly slow.
 */

static uint_cell * mem = 0;
static int dyn_memsize = 0;
static int ints_per_cell = 0;
static unsigned byte_per_cell = 0;
static int max_int_offset, min_int_offset;

#define MINALLOC 16

static
uint_cell *
alloc_ptr(uint_cell *p)
{
    int amt, memoff, i, off;
    uint_cell * nmem;
    if (p >= mem && p < mem+dyn_memsize) return p;

    memoff = p-mem; off = 0;
    if (memoff<0) off = -memoff;
    else if(memoff>=dyn_memsize) off = memoff-dyn_memsize;
    amt = off / MINALLOC;
    amt = (amt+1) * MINALLOC;
    nmem = realloc(mem, (dyn_memsize+amt)*sizeof(*mem));
    if (!nmem) {
	if(mem) free(mem);
	fprintf(stderr, "Memory overflow when expanding tape.\n");
	exit(99);
    } else mem=nmem;
    if (memoff<0) {
        memmove(mem+amt, mem, dyn_memsize*sizeof(*mem));
        for(i=0; i<amt; i++)
            mem[i] = 0;
        memoff += amt;
    } else {
        for(i=0; i<amt; i++)
            mem[dyn_memsize+i] = 0;
    }
    dyn_memsize += amt;
    return mem+memoff;
}

static inline uint_cell *
move_ptr(uint_cell *p, int off) {
    p += off;
    if (off>=0 && p+max_int_offset >= mem+dyn_memsize)
        p = alloc_ptr(p+max_int_offset)-max_int_offset;
    if (off<=0 && p+min_int_offset <= mem)
        p = alloc_ptr(p+min_int_offset)-min_int_offset;
    return p;
}

static inline void BI_zero(uint_cell * m)
{
    memset(m, 0, byte_per_cell);
}

static inline void BI_set_int(uint_cell * m, int v)
{
    *m = (uint_cell)v;
    if (v < 0)
	memset(m+1, -1, byte_per_cell-sizeof(uint_cell));
    else
	memset(m+1, 0, byte_per_cell-sizeof(uint_cell));
}

static inline void BI_copy(uint_cell * a, uint_cell * b)
{
    memcpy(a, b, byte_per_cell);
}

static inline void BI_add_cell(uint_cell * a, uint_cell v)
{
    uint_cell b = v;
    int cy = 0, i;
    for(i=0; i<ints_per_cell; i++) {
	uint_cell p = a[i];
	a[i] = cy + a[i] + b;
	if (a[i] > p) cy = 0;
	if (a[i] < p) cy = 1;
	b = 0;
    }
}

static inline void BI_sub_cell(uint_cell * a, uint_cell v)
{
    uint_cell b = v;
    int cy = 1, i;
    for(i=0; i<ints_per_cell; i++) {
	uint_cell p = a[i];
	a[i] = cy + a[i] + ~b;
	if (a[i] > p) cy = 0;
	if (a[i] < p) cy = 1;
	b = 0;
    }
}

static inline void BI_add(uint_cell * a, uint_cell * b)
{
    int cy = 0, i;
    for(i=0; i<ints_per_cell; i++) {
	uint_cell p = a[i];
	a[i] = cy + a[i] + b[i];
	if (a[i] > p) cy = 0;
	if (a[i] < p) cy = 1;
    }
}

static inline void BI_sub(uint_cell * a, uint_cell * b)
{
    int cy = 1, i;
    for(i=0; i<ints_per_cell; i++) {
	uint_cell p = a[i];
	a[i] = cy + a[i] + ~b[i];
	if (a[i] > p) cy = 0;
	if (a[i] < p) cy = 1;
    }
}

static inline void BI_neg(uint_cell * a)
{
    int cy = 1, i;
    for(i=0; i<ints_per_cell; i++) {
	a[i] = cy + ~a[i];
	cy = (a[i] == 0);
    }
}

#ifndef NO_BIG_INT
static inline void BI_mul_uint(uint_cell * a, unsigned int b)
{
    uint_cell h, cy = 0;
    uint_long r;
    int i;

    for(i=0; i<ints_per_cell; i++) {
	r = a[i];
	r *= b;
	r += cy;
	h = (r >> (sizeof(uint_cell) * CHAR_BIT));
	a[i] = r;
	cy = h;
    }
}
#endif

#ifdef NO_BIG_INT
#define HALF_BITS	(sizeof(uint_cell)*CHAR_BIT/2)
#define HALF_MASK	((((uint_cell)1)<<HALF_BITS)-1)

static inline void
long_mult(uint_cell * rh, uint_cell * rl, uint_cell a, uint_cell b)
{
    uint_cell sum0, sum1, sum2, sum3;
    uint_cell al, bl, ah, bh;

    /* Four halfwords */
    al = (a & HALF_MASK);
    ah = (a >> HALF_BITS);
    bl = (b & HALF_MASK);
    bh = (b >> HALF_BITS);

    /* Multiply the four permutations */
    sum0 = al * bl;	/* Offset = 0 */
    sum1 = al * bh;	/* Offset = HALF_BITS */
    sum2 = ah * bl;	/* Offset = HALF_BITS */
    sum3 = ah * bh;	/* Offset = FULL_BITS */

    /* sum1 will be no more than MAX-2*(MAX>>HALF_BITS) */
    sum1 += (sum0 >> HALF_BITS);

    /* A normal N->N+1 bits addition, so check for overflow */
    sum1 += sum2;
    if (sum1 < sum2)
	sum3 += HALF_MASK+1;

    /* No overflow here either. */
    *rh = sum3 + (sum1 >> HALF_BITS);

    /* Two independent parts to join, not add */
    *rl = (sum1 << HALF_BITS) + (sum0 & HALF_MASK);
}

static inline void
BI_mul_uint(uint_cell * a, unsigned int b)
{
    uint_cell cy = 0;
    uint_cell rl, rh;
    int i;

    for(i=0; i<ints_per_cell; i++) {
	long_mult(&rh, &rl, a[i], b);
	rl += cy;
	if (rl < cy)
	    rh++;
	a[i] = rl;
	cy = rh;
    }
}

#endif

static inline int BI_is_zero(uint_cell * a)
{
    uint_cell b = 0;
    int i;
    for(i=0; i<ints_per_cell; i++) {
	b |= a[i];
    }
    return b == 0;
}

static void
run_supertree(void)
{
    struct bfi * n = bfprog;
    uint_cell * m;
    uint_cell *t1, *t2, *t3;
    uint_cell mask_value = -1;
    int mask_offset = 0;

    ints_per_cell = ((unsigned)cell_length + sizeof(uint_cell)*CHAR_BIT -1)
		    / (sizeof(uint_cell)*CHAR_BIT);

    byte_per_cell = sizeof(uint_cell)*ints_per_cell;

    if (byte_per_cell*CHAR_BIT != cell_length) {
	mask_offset = ints_per_cell-1;
	mask_value >>= (byte_per_cell*CHAR_BIT - cell_length);
    }

    max_int_offset = max_pointer*ints_per_cell + ints_per_cell - 1;
    min_int_offset = max_pointer*ints_per_cell;

#ifndef NO_BIG_INT
    if (verbose)
	fprintf(stderr, "Maxtree variant: %dx%d byte int/cell (hardmult)\n",
		ints_per_cell, (int)sizeof(uint_cell));
#else
    if (verbose)
	fprintf(stderr, "Maxtree variant: %dx%d byte int/cell (softmult)\n",
		ints_per_cell, (int)sizeof(uint_cell));
#endif

    t1 = tcalloc(sizeof(uint_cell), ints_per_cell);
    t2 = tcalloc(sizeof(uint_cell), ints_per_cell);
    t3 = tcalloc(sizeof(uint_cell), ints_per_cell);

    m = move_ptr(alloc_ptr(mem),0);

    only_uses_putch = 1;
    start_runclock();

    while(n) {
	switch(n->type)
	{
	    case T_MOV: m = move_ptr(m, n->count * ints_per_cell); break;
	    case T_ADD:
		// p[n->offset] += n->count;
		if (n->count >= 0) {
		    BI_add_cell(m + n->offset*ints_per_cell, n->count);
		} else {
		    BI_sub_cell(m + n->offset*ints_per_cell, -n->count);
		}
		break;

	    case T_SET:
		// p[n->offset] = n->count;
		BI_set_int(m + n->offset*ints_per_cell, n->count);
		break;

	    case T_CALC:
		// p[n->offset] = n->count
		//	    + n->count2 * p[n->offset2]
		//	    + n->count3 * p[n->offset3];

		BI_set_int(t1, n->count);

		if (n->count2 == 1) {
		    BI_add(t1, m + n->offset2*ints_per_cell);
		} else if (n->count2 > 0) {
		    BI_copy(t2, m + n->offset2*ints_per_cell);
		    BI_mul_uint(t2, n->count2);
		    BI_add(t1, t2);
		} else if (n->count2 < 0) {
		    BI_copy(t2, m + n->offset2*ints_per_cell);
		    BI_mul_uint(t2, -n->count2);
		    BI_sub(t1, t2);
		}

		if (n->count3 == 1) {
		    BI_add(t1, m + n->offset3*ints_per_cell);
		} else if (n->count3 > 0) {
		    BI_copy(t3, m + n->offset3*ints_per_cell);
		    BI_mul_uint(t3, n->count3);
		    BI_add(t1, t3);
		} else if (n->count3 < 0) {
		    BI_copy(t3, m + n->offset3*ints_per_cell);
		    BI_mul_uint(t3, -n->count3);
		    BI_sub(t1, t3);
		}

		BI_copy(m + n->offset*ints_per_cell, t1);
		break;

	    case T_IF: case T_MULT: case T_CMULT:

	    case T_WHL:
		m[n->offset*ints_per_cell+mask_offset] &= mask_value;
		if( BI_is_zero(m + n->offset*ints_per_cell) ) n=n->jmp;
		break;

	    case T_END:
		m[n->offset*ints_per_cell+mask_offset] &= mask_value;
		if(!BI_is_zero(m + n->offset*ints_per_cell) ) n=n->jmp;
		break;

	    case T_ENDIF:
		break;

	    case T_CHR:
		putch(n->count);
		break;
	    case T_PRT:
		putch(m[n->offset*ints_per_cell]);
		break;
	    case T_INP:
		{   /* Cell is too large for an int */
		    int ch = -256;
		    ch = getch(ch);
		    if (ch != -256) {
			BI_set_int(m + n->offset*ints_per_cell, ch);
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

    free(mem);
    free(t1);
    free(t2);
    free(t3);
}

#endif
