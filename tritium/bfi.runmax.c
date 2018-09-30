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
#include "ov_int.h"

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
	    case T_CALCMULT:
		p[n->offset] = n->count
			    + n->count2 * p[n->offset2]
			    * n->count3 * p[n->offset3];
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
		       "Bad node: type %s: ptr+%d, cnt=%d.\n",
			tokennames[n->type], n->offset, n->count);
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
#include <openssl/err.h>

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
    BN_CTX * BNtmp = BN_CTX_new();

    if (verbose)
	fprintf(stderr, "Maxtree variant: using OpenSSL Bignums.\n");

    only_uses_putch = 1;
    start_runclock();

    while(n) {
#if 0
	fprintf(stderr, "P(%d,%d)=", n->line, n->col);
	printtreecell(stderr, -1, n);
	fprintf(stderr, "\n");
#endif
	switch(n->type)
	{
	    case T_MOV: m = move_ptr(m, n->count); break;
	    case T_ADD:
		// p[n->offset] += n->count;
		if (n->count >= 0) {
		    if (!BN_add_word(m[n->offset], n->count))
			goto BN_op_failed;
		} else {
		    if (!BN_sub_word(m[n->offset], -n->count))
			goto BN_op_failed;
		}
		break;
	    case T_SET:
		// p[n->offset] = n->count;
		if (n->count >= 0) {
		    if (!BN_set_word(m[n->offset], n->count))
			goto BN_op_failed;
		} else {
		    if (!BN_zero(m[n->offset]))
			goto BN_op_failed;
		    if (!BN_sub_word(m[n->offset], -n->count))
			goto BN_op_failed;
		}
		break;
	    case T_CALC:
		// p[n->offset] = n->count
		//	    + n->count2 * p[n->offset2]
		//	    + n->count3 * p[n->offset3];

		/* Quick copy or add */
		if (n->count == 0 && n->count2 == 1) {
		    if (n->count3 == 0) {
			if (!BN_copy(m[n->offset], m[n->offset2]))
			    goto BN_op_failed;
			break;
		    }
		    if (n->count3 == 1) {
			if (!BN_add(m[n->offset], m[n->offset2], m[n->offset3]))
			    goto BN_op_failed;
			break;
		    }
		}

		if (n->count >= 0) {
		    if (!BN_set_word(t1, n->count))
			goto BN_op_failed;
		} else {
		    if (!BN_zero(t1))
			goto BN_op_failed;
		    if (!BN_sub_word(t1, -n->count))
			goto BN_op_failed;
		}

		if (n->count2 == 1) {
		    if (!BN_add(t1, t1, m[n->offset2]))
			goto BN_op_failed;
		} else if (n->count2 > 0) {
		    if (!BN_copy(t2, m[n->offset2]))
			goto BN_op_failed;
		    if (!BN_mul_word(t2, n->count2))
			goto BN_op_failed;
		    if (!BN_add(t1, t1, t2))
			goto BN_op_failed;
		} else if (n->count2 < 0) {
		    if (!BN_copy(t2, m[n->offset2]))
			goto BN_op_failed;
		    if (!BN_mul_word(t2, -n->count2))
			goto BN_op_failed;
		    if (!BN_sub(t1, t1, t2))
			goto BN_op_failed;
		}

		if (n->count3 == 1) {
		    if (!BN_add(t1, t1, m[n->offset3]))
			goto BN_op_failed;
		} else if (n->count3 > 0) {
		    if (!BN_copy(t3, m[n->offset3]))
			goto BN_op_failed;
		    if (!BN_mul_word(t3, n->count3))
			goto BN_op_failed;
		    if (!BN_add(t1, t1, t3))
			goto BN_op_failed;
		} else if (n->count3 < 0) {
		    if (!BN_copy(t3, m[n->offset3]))
			goto BN_op_failed;
		    if (!BN_mul_word(t3, -n->count3))
			goto BN_op_failed;
		    if (!BN_sub(t1, t1, t3))
			goto BN_op_failed;
		}

		if(do_mask)
		    BN_mask_bits(t1, cell_length);
		if (!BN_copy(m[n->offset], t1))
		    goto BN_op_failed;
		break;

	    case T_CALCMULT:
		// p[n->offset] = n->count
		//	    + n->count2 * p[n->offset2]
		//	    * n->count3 * p[n->offset3];

		if (n->count == 0 && n->count2 == 1 && n->count3 == 1) {

		    if (!BN_mul(m[n->offset], m[n->offset2],
			m[n->offset3], BNtmp))
			goto BN_op_failed;
		    if(do_mask)
			BN_mask_bits(m[n->offset], cell_length);
		    break;
		}

		if (n->count2 == 1) {
		    if (!BN_copy(t1, m[n->offset2]))
			goto BN_op_failed;
		} else if (n->count2 > 0) {
		    if (!BN_copy(t1, m[n->offset2]))
			goto BN_op_failed;
		    if (!BN_mul_word(t1, n->count2))
			goto BN_op_failed;
		} else if (n->count2 < 0) {
		    if (!BN_copy(t2, m[n->offset2]))
			goto BN_op_failed;
		    if (n->count2 != -1)
			if (!BN_mul_word(t2, -n->count2))
			    goto BN_op_failed;
		    if (!BN_zero(t1))
			goto BN_op_failed;
		    if (!BN_sub(t1, t1, t2))
			goto BN_op_failed;
		}

		if (n->count3 == 1) {
		    if (!BN_mul(t1, t1, m[n->offset3], BNtmp))
			goto BN_op_failed;
		} else if (n->count3 > 0) {
		    if (!BN_copy(t3, m[n->offset3]))
			goto BN_op_failed;
		    if (!BN_mul_word(t3, n->count3))
			goto BN_op_failed;
		    if (!BN_mul(t1, t1, t3, BNtmp))
			goto BN_op_failed;
		} else if (n->count3 < 0) {
		    if (!BN_copy(t3, m[n->offset3]))
			goto BN_op_failed;
		    if (n->count3 != -1)
			if (!BN_mul_word(t3, -n->count3))
			    goto BN_op_failed;
		    if (!BN_zero(t2))
			goto BN_op_failed;
		    if (!BN_sub(t2, t2, t3))
			goto BN_op_failed;
		    if (!BN_mul(t1, t1, t2, BNtmp))
			goto BN_op_failed;
		}

		if (n->count > 0) {
		    if (!BN_add_word(t1, n->count))
			goto BN_op_failed;
		} else if (n->count < 0) {
		    if (!BN_sub_word(t1, -n->count))
			goto BN_op_failed;
		}

		if(do_mask)
		    BN_mask_bits(t1, cell_length);
		if (!BN_copy(m[n->offset], t1))
		    goto BN_op_failed;
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
		       "Bad node: type %s: ptr+%d, cnt=%d.\n",
			tokennames[n->type], n->offset, n->count);
		exit(1);
	}
	n = n->next;
    }

    finish_runclock(&run_time, &io_time);

    /* TODO: free the tape */

    if (0) {
BN_op_failed:
	fprintf(stderr, "OpenSSL-BN: %s\n",
	    ERR_error_string(ERR_get_error(), 0));
	exit(255);
    }
}

#endif

/******************************************************************************/

#ifdef DISABLE_BN

#ifdef TESTSOFTMULT
typedef C uint_cell;
#define NO_BIG_INT
#elif defined(__SIZEOF_INT128__) && defined(UINT64_MAX)
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

typedef struct { char f; int v; uint_cell * b[1]; } mem_cell;
static mem_cell * mem = 0;
static int dyn_memsize = 0;
static int ints_per_cell = 0;
static unsigned byte_per_cell = 0;

#define MINALLOC 16

static
mem_cell *
alloc_ptr(mem_cell *p)
{
    int amt, memoff, off;
    mem_cell * nmem;
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
        memset(mem, 0, amt*sizeof(*mem));
        memoff += amt;
    } else {
        memset(mem+dyn_memsize, 0, amt*sizeof(*mem));
    }
    dyn_memsize += amt;
    return mem+memoff;
}

static inline mem_cell *
move_ptr(mem_cell *p, int off) {
    p += off;
    if (off>=0 && p+max_pointer >= mem+dyn_memsize)
        p = alloc_ptr(p+max_pointer)-max_pointer;
    if (off<=0 && p+min_pointer <= mem)
        p = alloc_ptr(p+min_pointer)-min_pointer;
    return p;
}

static inline void BI_free(uint_cell ** m)
{
    if (!*m) return;
    free(*m);
    *m = 0;
}

static inline void BI_zero(uint_cell ** m)
{
    if (!*m) return;
    memset(*m, 0, byte_per_cell);
}

static inline void BI_init(uint_cell ** m)
{
    uint_cell *t;
    if (*m) return;
    t = malloc(sizeof(uint_cell) * ints_per_cell);
    if (!t) {
	fprintf(stderr, "Memory overflow when allocating a cell.\n");
	exit(99);
    }
    *m = t;
}

static inline uint_cell * BI_zinit(uint_cell ** m)
{
    if (!*m) {
	BI_init(m);
	memset(*m, 0, byte_per_cell);
    }
    return *m;
}

static inline void BI_set_int(uint_cell ** m, int v)
{
    BI_init(m);
    **m = (uint_cell)v;
    if (v < 0)
	memset(*m+1, -1, byte_per_cell-sizeof(uint_cell));
    else
	memset(*m+1, 0, byte_per_cell-sizeof(uint_cell));
}

static inline void BI_copy(uint_cell ** a, uint_cell ** b)
{
    if (!*b) BI_zero(a);
    else {
	BI_init(a);
	memcpy(*a, *b, byte_per_cell);
    }
}

static inline void BI_add_cell(uint_cell ** pa, uint_cell v)
{
    uint_cell b = v;
    uint_cell * a;
    int cy = 0, i;
    a = BI_zinit(pa);
    for(i=0; i<ints_per_cell; i++) {
	uint_cell p = a[i];
	a[i] = cy + a[i] + b;
	if (a[i] > p) cy = 0;
	if (a[i] < p) cy = 1;
	b = 0;
    }
}

static inline void BI_sub_cell(uint_cell ** pa, uint_cell v)
{
    uint_cell b = v;
    uint_cell * a;
    int cy = 1, i;
    a = BI_zinit(pa);
    for(i=0; i<ints_per_cell; i++) {
	uint_cell p = a[i];
	a[i] = cy + a[i] + ~b;
	if (a[i] > p) cy = 0;
	if (a[i] < p) cy = 1;
	b = 0;
    }
}

static inline void BI_add(uint_cell ** pa, uint_cell ** pb)
{
    int cy = 0, i;
    uint_cell * a, * b;
    if (!*pb) return;
    a = BI_zinit(pa);
    b = *pb;
    for(i=0; i<ints_per_cell; i++) {
	uint_cell p = a[i];
	a[i] = cy + a[i] + b[i];
	if (a[i] > p) cy = 0;
	if (a[i] < p) cy = 1;
    }
}

static inline void BI_sub(uint_cell ** pa, uint_cell ** pb)
{
    int cy = 1, i;
    uint_cell * a, * b;
    if (!*pb) return;
    a = BI_zinit(pa);
    b = *pb;
    for(i=0; i<ints_per_cell; i++) {
	uint_cell p = a[i];
	a[i] = cy + a[i] + ~b[i];
	if (a[i] > p) cy = 0;
	if (a[i] < p) cy = 1;
    }
}

static inline void BI_neg(uint_cell ** pa)
{
    int cy = 1, i;
    uint_cell * a;
    if (!*pa) return;
    a = *pa;
    for(i=0; i<ints_per_cell; i++) {
	a[i] = cy + ~a[i];
	cy = (a[i] == 0);
    }
}

#ifndef NO_BIG_INT
static inline void BI_mul_uint(uint_cell ** pa, unsigned int b)
{
    uint_cell h, cy = 0;
    uint_long r;
    int i;
    uint_cell * a;
    if (!*pa) return;
    a = *pa;

    for(i=0; i<ints_per_cell; i++) {
	r = a[i];
	r *= b;
	r += cy;
	h = (r >> (sizeof(uint_cell) * CHAR_BIT));
	a[i] = r;
	cy = h;
    }
}

static inline void BI_mul(uint_cell ** pr, uint_cell ** pa, uint_cell ** pb)
{
    uint_cell h;
    uint_long lr;
    int i, j, k;
    uint_cell *r, *a, *b;
    if (!*pa || !*pb) { BI_zero(pr); return; }

    BI_set_int(pr, 0);
    r = *pr; a = *pa; b = *pb;

    for(i=0; i<ints_per_cell; i++) {
	if (a[i] == 0) continue;
	for(j=0; j<ints_per_cell-i; j++) {
	    if (b[j] == 0) continue;
	    k = i + j;

	    lr = a[i];
	    lr *= b[j];
	    lr += r[k];
	    r[k] = lr; k++;
	    h = (lr >> (sizeof(uint_cell) * CHAR_BIT));
	    while(h != 0 && k < ints_per_cell)
	    {
		lr = h;
		lr += r[k];
		r[k] = lr; k++;
		h = (lr >> (sizeof(uint_cell) * CHAR_BIT));
	    }
	}
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
BI_mul_uint(uint_cell ** pa, unsigned int b)
{
    uint_cell cy = 0;
    uint_cell rl, rh;
    int i;
    uint_cell * a;
    if (!*pa) return;
    a = *pa;

    for(i=0; i<ints_per_cell; i++) {
	long_mult(&rh, &rl, a[i], b);
	rl += cy;
	if (rl < cy)
	    rh++;
	a[i] = rl;
	cy = rh;
    }
}

static inline void BI_mul(uint_cell ** pr, uint_cell ** pa, uint_cell ** pb)
{
    uint_cell h;
    uint_cell rl;
    int i, j, k;
    uint_cell *r, *a, *b;
    if (!*pa || !*pb) { BI_zero(pr); return; }

    BI_set_int(pr, 0);
    r = *pr; a = *pa; b = *pb;

    for(i=0; i<ints_per_cell; i++) {
	if (a[i] == 0) continue;
	for(j=0; j<ints_per_cell-i; j++) {
	    if (b[j] == 0) continue;
	    k = i + j;

	    long_mult(&h, &rl, a[i], b[j]);
	    rl += r[k];
	    if (rl < r[k])
		h++;

	    r[k] = rl; k++;
	    while(h != 0 && k < ints_per_cell)
	    {
		rl = h; h = 0;
		rl += r[k];
		if (rl < r[k])
		    h++;

		r[k] = rl; k++;
	    }
	}
    }
}

#endif

static inline int BI_is_zero(uint_cell ** pa)
{
    int i;
    uint_cell * a;
    if (!*pa) return 1;
    a = *pa;
    for(i=0; i<ints_per_cell; i++)
	if (a[i]) return 0;
    return 1;
}

static void
run_supertree(void)
{
    struct bfi * n = bfprog;
    mem_cell * m;
    uint_cell *t1[1] = {0}, *t2[1] = {0}, *t3[1] = {0};
    uint_cell mask_value = -1;
    int do_mask = 0, mask_offset = 0;

    ints_per_cell = ((unsigned)cell_length + sizeof(uint_cell)*CHAR_BIT -1)
		    / (sizeof(uint_cell)*CHAR_BIT);

    byte_per_cell = sizeof(uint_cell)*ints_per_cell;

    if (byte_per_cell*CHAR_BIT != cell_length) {
	do_mask = 1;
	mask_offset = ints_per_cell-1;
	mask_value >>= (byte_per_cell*CHAR_BIT - cell_length);
    }

#ifndef NO_BIG_INT
    if (verbose)
	fprintf(stderr, "Maxtree variant: %dx%d byte int/cell (hardmult)\n",
		ints_per_cell, (int)sizeof(uint_cell));
#else
    if (verbose)
	fprintf(stderr, "Maxtree variant: %dx%d byte int/cell (softmult)\n",
		ints_per_cell, (int)sizeof(uint_cell));
#endif

    m = move_ptr(alloc_ptr(mem),0);

    only_uses_putch = 1;
    start_runclock();

    while(n) {
#if 0
	fprintf(stderr, "P(%d,%d)=", n->line, n->col);
	printtreecell(stderr, -1, n);
	fprintf(stderr, "\n");
#endif
	switch(n->type)
	{
	    case T_MOV: m = move_ptr(m, n->count); break;
	    case T_ADD:
		// p[n->offset] += n->count;
		if (!m[n->offset].f) {
		    int ov=0, v;
		    v = ov_iadd(m[n->offset].v, n->count, &ov);
		    if (!ov) {
			m[n->offset].v = v;
			break;
		    }
		    m[n->offset].f = 1;
		    BI_set_int(m[n->offset].b, m[n->offset].v);
		}
		if (n->count >= 0) {
		    BI_add_cell(m[n->offset].b, n->count);
		} else {
		    BI_sub_cell(m[n->offset].b, -n->count);
		}
		break;

	    case T_SET:
		// p[n->offset] = n->count;
		m[n->offset].f = 0;
		m[n->offset].v = n->count;
		break;

	    case T_CALC:
		// p[n->offset] = n->count
		//	    + n->count2 * p[n->offset2]
		//	    + n->count3 * p[n->offset3];
		if ((n->count2 == 0 || !m[n->offset2].f) &&
		    (n->count3 == 0 || !m[n->offset3].f) )
		{
		    int ov=0, v;
		    v = ov_iadd(n->count,
			    ov_iadd(
				ov_imul(n->count2, m[n->offset2].v, &ov),
				ov_imul(n->count3, m[n->offset3].v, &ov),
				&ov), &ov);
		    if (!ov) {
			m[n->offset].f = 0;
			m[n->offset].v = v;
			break;
		    }
		}

		if (n->count2 && !m[n->offset2].f) {
		    m[n->offset2].f = 1;
		    BI_set_int(m[n->offset2].b, m[n->offset2].v);
		}
		if (n->count3 && !m[n->offset3].f) {
		    m[n->offset3].f = 1;
		    BI_set_int(m[n->offset3].b, m[n->offset3].v);
		}

		m[n->offset].f = 1;
		if (n->count == 0 && n->count2 == 1) {
		    if (n->offset == n->offset2) {
			if (n->count3 == 1) {
			    BI_add(m[n->offset].b, m[n->offset3].b);
			    break;
			}
			if (n->count3 == -1) {
			    BI_sub(m[n->offset].b, m[n->offset3].b);
			    break;
			}
		    } else if (n->count3 == 0) {
			BI_copy(m[n->offset].b, m[n->offset2].b);
			break;
		    }
		}

		BI_set_int(t1, n->count);

		if (n->count2 == 1) {
		    BI_add(t1, m[n->offset2].b);
		} else if (n->count2 > 0) {
		    BI_copy(t2, m[n->offset2].b);
		    BI_mul_uint(t2, n->count2);
		    BI_add(t1, t2);
		} else if (n->count2 < 0) {
		    BI_copy(t2, m[n->offset2].b);
		    BI_mul_uint(t2, -n->count2);
		    BI_sub(t1, t2);
		}

		if (n->count3 == 1) {
		    BI_add(t1, m[n->offset3].b);
		} else if (n->count3 > 0) {
		    BI_copy(t3, m[n->offset3].b);
		    BI_mul_uint(t3, n->count3);
		    BI_add(t1, t3);
		} else if (n->count3 < 0) {
		    BI_copy(t3, m[n->offset3].b);
		    BI_mul_uint(t3, -n->count3);
		    BI_sub(t1, t3);
		}

		BI_copy(m[n->offset].b, t1);
		break;

	    case T_CALCMULT:
		if (n->count != 0 || n->count2 != 1 || n->count3 != 1) {
		    fprintf(stderr, "Error on code generation:\n"
			   "Bad T_CALCMULT node with incorrect counts.\n");
		    exit(99);
		}

		if (!m[n->offset2].f && !m[n->offset3].f)
		{
		    int ov=0, v;
		    v = ov_imul(m[n->offset2].v, m[n->offset3].v, &ov);
		    if (!ov) {
			m[n->offset].f = 0;
			m[n->offset].v = v;
			break;
		    }
		}

		m[n->offset].f = 1;
		if (!m[n->offset2].f) {
		    m[n->offset2].f = 1;
		    BI_set_int(m[n->offset2].b, m[n->offset2].v);
		}
		if (!m[n->offset3].f) {
		    m[n->offset3].f = 1;
		    BI_set_int(m[n->offset3].b, m[n->offset3].v);
		}

		BI_mul(m[n->offset].b, m[n->offset2].b, m[n->offset3].b);
		break;

	    case T_IF: case T_MULT: case T_CMULT:

	    case T_WHL:
		if (m[n->offset].f) {
		    if (do_mask && m[n->offset].b[0])
			m[n->offset].b[0][mask_offset] &= mask_value;
		    if( BI_is_zero(m[n->offset].b) ) {
			m[n->offset].f = m[n->offset].v = 0;
		    }
		}

		if(!m[n->offset].f && m[n->offset].v == 0) n=n->jmp;
		break;

	    case T_END:
		if (m[n->offset].f) {
		    if (do_mask && m[n->offset].b[0])
			m[n->offset].b[0][mask_offset] &= mask_value;
		    if( BI_is_zero(m[n->offset].b) ) {
			m[n->offset].f = m[n->offset].v = 0;
		    }
		}

		if(m[n->offset].f || m[n->offset].v != 0) n=n->jmp;
		break;

	    case T_ENDIF:
		break;

	    case T_CHR:
		putch(n->count);
		break;
	    case T_PRT:
		if (!m[n->offset].f)
		    putch(m[n->offset].v);
		else
		    putch(m[n->offset].b[0][0]);
		break;
	    case T_INP:
		{   /* Cell is too large for an int */
		    int ch = -256;
		    ch = getch(ch);
		    if (ch != -256) {
			m[n->offset].f = 0;
			m[n->offset].v = ch;
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
		       "Bad node: type %s: ptr+%d, cnt=%d.\n",
			tokennames[n->type], n->offset, n->count);
		exit(1);
	}
	n = n->next;
    }

    finish_runclock(&run_time, &io_time);

    {
	int i;
	for(i=0; i<dyn_memsize; i++) BI_free(mem[i].b);
	free(mem);
	if (*t1) free(*t1);
	if (*t2) free(*t2);
	if (*t3) free(*t3);
    }
}

#endif
