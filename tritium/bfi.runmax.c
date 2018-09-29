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
	fprintf(stderr, "Maxtree variant: using OpenSSL Bignums\n");

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

		/* Quick copy or add */
		if (n->count == 0 && n->count2 == 1) {
		    if (n->count3 == 0) {
			BN_copy(m[n->offset], m[n->offset2]);
			break;
		    }
		    if (n->count3 == 1) {
			BN_add(m[n->offset], m[n->offset2], m[n->offset3]);
			break;
		    }
		}

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

	    case T_CALCMULT:
		// p[n->offset] = n->count
		//	    + n->count2 * p[n->offset2]
		//	    * n->count3 * p[n->offset3];

		if (n->count == 0 && n->count2 == 1 && n->count3 == 1) {

		    BN_mul(m[n->offset], m[n->offset2], m[n->offset3], BNtmp);
		    if(do_mask)
			BN_mask_bits(m[n->offset], cell_length);
		    break;
		}

		if (n->count2 == 1) {
		    BN_copy(t1, m[n->offset2]);
		} else if (n->count2 > 0) {
		    BN_copy(t1, m[n->offset2]);
		    BN_mul_word(t1, n->count2);
		} else if (n->count2 < 0) {
		    BN_copy(t2, m[n->offset2]);
		    if (n->count2 != -1)
			BN_mul_word(t2, -n->count2);
		    BN_zero(t1);
		    BN_sub(t1, t1, t2);
		}

		if (n->count3 == 1) {
		    BN_mul(t1, t1, m[n->offset3], BNtmp);
		} else if (n->count3 > 0) {
		    BN_copy(t3, m[n->offset3]);
		    BN_mul_word(t3, n->count3);
		    BN_mul(t1, t1, t3, BNtmp);
		} else if (n->count3 < 0) {
		    BN_copy(t3, m[n->offset3]);
		    if (n->count3 != -1)
			BN_mul_word(t3, -n->count3);
		    BN_zero(t2);
		    BN_sub(t2, t2, t3);
		    BN_mul(t1, t1, t2, BNtmp);
		}

		if (n->count > 0) {
		    BN_add_word(t1, n->count);
		} else if (n->count < 0) {
		    BN_sub_word(t1, -n->count);
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
		       "Bad node: type %s: ptr+%d, cnt=%d.\n",
			tokennames[n->type], n->offset, n->count);
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

static uint_cell ** mem = 0;
static int dyn_memsize = 0;
static int ints_per_cell = 0;
static unsigned byte_per_cell = 0;

#define MINALLOC 16

static
uint_cell **
alloc_ptr(uint_cell **p)
{
    int amt, memoff, i, off;
    uint_cell ** nmem;
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

static inline uint_cell **
move_ptr(uint_cell **p, int off) {
    p += off;
    if (off>=0 && p+max_pointer >= mem+dyn_memsize)
        p = alloc_ptr(p+max_pointer)-max_pointer;
    if (off<=0 && p+min_pointer <= mem)
        p = alloc_ptr(p+min_pointer)-min_pointer;
    return p;
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
	fprintf(stderr, "Memory overflow when expanding tape.\n");
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
    uint_cell b = 0;
    int i;
    uint_cell * a;
    if (!*pa) return 1;
    a = *pa;
    for(i=0; i<ints_per_cell; i++) {
	b |= a[i];
    }
    return b == 0;
}

static void
run_supertree(void)
{
    struct bfi * n = bfprog;
    uint_cell ** m;
    uint_cell *t1 = 0, *t2 = 0, *t3 = 0;
    uint_cell mask_value = -1;
    int mask_offset = 0;

    ints_per_cell = ((unsigned)cell_length + sizeof(uint_cell)*CHAR_BIT -1)
		    / (sizeof(uint_cell)*CHAR_BIT);

    byte_per_cell = sizeof(uint_cell)*ints_per_cell;

    if (byte_per_cell*CHAR_BIT != cell_length) {
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
		if (n->count >= 0) {
		    BI_add_cell(m + n->offset, n->count);
		} else {
		    BI_sub_cell(m + n->offset, -n->count);
		}
		break;

	    case T_SET:
		// p[n->offset] = n->count;
		BI_set_int(m + n->offset, n->count);
		break;

	    case T_CALC:
		// p[n->offset] = n->count
		//	    + n->count2 * p[n->offset2]
		//	    + n->count3 * p[n->offset3];

		if (n->count == 0 && n->count2 == 1) {
		    if (n->offset == n->offset2) {
			if (n->count3 == 1) {
			    BI_add(m + n->offset, m + n->offset3);
			    break;
			}
			if (n->count3 == -1) {
			    BI_sub(m + n->offset, m + n->offset3);
			    break;
			}
		    } else if (n->count3 == 0) {
			BI_copy(m + n->offset, m + n->offset2);
			break;
		    }
		}

		BI_set_int(&t1, n->count);

		if (n->count2 == 1) {
		    BI_add(&t1, m + n->offset2);
		} else if (n->count2 > 0) {
		    BI_copy(&t2, m + n->offset2);
		    BI_mul_uint(&t2, n->count2);
		    BI_add(&t1, &t2);
		} else if (n->count2 < 0) {
		    BI_copy(&t2, m + n->offset2);
		    BI_mul_uint(&t2, -n->count2);
		    BI_sub(&t1, &t2);
		}

		if (n->count3 == 1) {
		    BI_add(&t1, m + n->offset3);
		} else if (n->count3 > 0) {
		    BI_copy(&t3, m + n->offset3);
		    BI_mul_uint(&t3, n->count3);
		    BI_add(&t1, &t3);
		} else if (n->count3 < 0) {
		    BI_copy(&t3, m + n->offset3);
		    BI_mul_uint(&t3, -n->count3);
		    BI_sub(&t1, &t3);
		}

		BI_copy(m + n->offset, &t1);
		break;

	    case T_CALCMULT:
		if (n->count == 0 && n->count2 == 1 && n->count3 == 1) {
		    BI_mul(m + n->offset, m + n->offset2, m + n->offset3);
		    break;
		}
		fprintf(stderr, "Error on code generation:\n"
		       "Bad T_CALCMULT node with incorrect counts.\n");
		exit(99);
		break;

	    case T_IF: case T_MULT: case T_CMULT:

	    case T_WHL:
		if (m[n->offset])
		    m[n->offset][mask_offset] &= mask_value;
		if( BI_is_zero(m + n->offset) ) n=n->jmp;
		break;

	    case T_END:
		if (m[n->offset])
		    m[n->offset][mask_offset] &= mask_value;
		if(!BI_is_zero(m + n->offset) ) n=n->jmp;
		break;

	    case T_ENDIF:
		break;

	    case T_CHR:
		putch(n->count);
		break;
	    case T_PRT:
		putch(m[n->offset][0]);
		break;
	    case T_INP:
		{   /* Cell is too large for an int */
		    int ch = -256;
		    ch = getch(ch);
		    if (ch != -256) {
			BI_set_int(m + n->offset, ch);
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

    /* TODO: free(mem); */
    if (t1) free(t1);
    if (t2) free(t2);
    if (t3) free(t3);
}

#endif
