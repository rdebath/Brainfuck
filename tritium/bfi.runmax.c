/*
 * This file contains an interpretation engine that is slow but has
 * large cells. It doesn't use any external library.
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
#include "big_int.h"
#include "bfi.runarray.h"

/******************************************************************************/

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

typedef struct { char f; int v; uint_cell * b[1]; } mem_cell;
static mem_cell * mem = 0;
static int dyn_memsize = 0;
static int ints_per_cell = 0;
static unsigned byte_per_cell = 0;

#define MINALLOC 256

static
mem_cell *
alloc_ptr(mem_cell *p)
{
    int amt, memoff, off;
    mem_cell * nmem = 0;
    if (p >= mem && p < mem+dyn_memsize) return p;

    memoff = p-mem; off = 0;
    if (memoff<0) off = -memoff;
    else if(memoff>=dyn_memsize) off = memoff-dyn_memsize;
    amt = off / MINALLOC;
    amt = (amt+1) * MINALLOC;
    /* Current (2020) C compilers are broken for arrays > 50% of memory. */
    /* I am only allowing INT_MAX array elements */
    /* And I want to leave some memory for bignums */
#ifdef SSIZE_MAX
    if (dyn_memsize < SSIZE_MAX/(int)sizeof(*mem)-amt &&
	dyn_memsize < INT_MAX-amt )
#else
    if (dyn_memsize < INT_MAX/(int)sizeof(*mem)-amt)
#endif
	nmem = realloc(mem, (dyn_memsize+amt)*sizeof(*mem));
    if (!nmem) {
	fprintf(stderr, "Memory overflow when expanding tape to %d cells.\n",
		dyn_memsize+amt);
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
    if (off>=0 && p+max_safe_mem >= mem+dyn_memsize)
        p = alloc_ptr(p+max_safe_mem)-max_safe_mem;
    if (off<=0 && p+min_safe_mem <= mem)
        p = alloc_ptr(p+min_safe_mem)-min_safe_mem;
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
    if (*a == *b) return;
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

static inline int BI_lessthan(uint_cell ** pa, uint_cell ** pb)
{
    uint_cell *a, *b;
    int i;
    if (!*pa) return !BI_is_zero(pb);

    a = *pa; b = *pb;
    for(i=ints_per_cell-1; i>0; i--) {
	if (a[i] != b[i])
	    return a[i] < b[i];
    }
    return *a < *b;
}

static inline int BI_input(uint_cell ** pa)
{
    int ch, ret_eof = 1, do_neg = 0;
    BI_zero(pa);
    while((ch = getchar()) != EOF) {
	if (ch >= '0' && ch <= '9') {
	    if (!ret_eof)
		BI_mul_uint(pa, 10);
	    if (ch != '0')
		BI_add_cell(pa, ch - '0');
	    ret_eof = 0;
	} else if (ret_eof && ch == '-') {
	    do_neg = 1;
	    ret_eof = 0;
	} else if (!ret_eof)
	    break;
    }
    ungetc(ch, stdin);
    if (do_neg)
	BI_neg(pa);
    return ret_eof;
}

#ifndef NO_BIG_INT
static inline void BI_div_uint(uint_cell ** pa, unsigned int b, unsigned int * r)
{
    uint_cell *a;
    uint_long d = 0;
    int i;

    /* Div by zero */
    if (b == 0) BI_zero(pa);
    /* or Div by one or zero div by any */
    if (b == 0 || b == 1 || BI_is_zero(pa) ) {
	*r = 0;
	return;
    }

    a = *pa;
    for(i=ints_per_cell-1; i>=0; i--) {
	d = (d << (sizeof(uint_cell) * CHAR_BIT) ) + a[i];
	if (d != 0) {
	    a[i] = d / b;
	    d = d % b;
	}
    }
    *r = d;
}
#endif

void
run_maxtree(void)
{
    struct bfi * n = bfprog;
    mem_cell * m;
    uint_cell *t1[1] = {0}, *t2[1] = {0}, *t3[1] = {0};
    uint_cell mask_value = -1;
    int do_mask = 0, mask_offset = 0;

    if (cell_length == INT_MAX) {
	ints_per_cell = ((unsigned)INT_MAX+1)/sizeof(uint_cell)/CHAR_BIT;
	byte_per_cell = sizeof(uint_cell)*ints_per_cell;
    } else {
	ints_per_cell = ((unsigned)cell_length + sizeof(uint_cell)*CHAR_BIT -1)
			/ (sizeof(uint_cell)*CHAR_BIT);

	byte_per_cell = sizeof(uint_cell)*ints_per_cell;

	if (byte_per_cell*CHAR_BIT != cell_length) {
	    do_mask = 1;
	    mask_offset = ints_per_cell-1;
	    mask_value >>= (byte_per_cell*CHAR_BIT - cell_length);
	}
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

		{
		    uint_cell ** m2 = m[n->offset2].b;
		    uint_cell ** m3 = m[n->offset3].b;

		    if (n->count2 && !m[n->offset2].f) {
			m2 = t2;
			BI_set_int(m2, m[n->offset2].v);
		    }
		    if (n->count3 && !m[n->offset3].f) {
			m3 = t3;
			BI_set_int(m3, m[n->offset3].v);
		    }

		    m[n->offset].f = 1;
		    if (n->count == 0 && n->count2 == 1) {
			if (n->offset == n->offset2) {
			    if (n->count3 == 1) {
				BI_add(m[n->offset].b, m3);
				break;
			    }
			    if (n->count3 == -1) {
				BI_sub(m[n->offset].b, m3);
				break;
			    }
			} else if (n->count3 == 0) {
			    BI_copy(m[n->offset].b, m2);
			    break;
			}
		    }

		    BI_set_int(t1, n->count);

		    if (n->count2 == 1) {
			BI_add(t1, m2);
		    } else if (n->count2 > 0) {
			BI_copy(t2, m2);
			BI_mul_uint(t2, n->count2);
			BI_add(t1, t2);
		    } else if (n->count2 < 0) {
			BI_copy(t2, m2);
			BI_mul_uint(t2, -n->count2);
			BI_sub(t1, t2);
		    }

		    if (n->count3 == 1) {
			BI_add(t1, m3);
		    } else if (n->count3 > 0) {
			BI_copy(t3, m3);
			BI_mul_uint(t3, n->count3);
			BI_add(t1, t3);
		    } else if (n->count3 < 0) {
			BI_copy(t3, m3);
			BI_mul_uint(t3, -n->count3);
			BI_sub(t1, t3);
		    }

		    BI_copy(m[n->offset].b, t1);
		}
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

	    case T_LT:
		if (n->count != 0 || n->count2 != 1 || n->count3 != 1) {
		    fprintf(stderr, "Error on code generation:\n"
			   "Bad T_LT node with incorrect counts.\n");
		    exit(99);
		}
		{
		    int flg = -1;

		    if (!m[n->offset2].f && !m[n->offset3].f)
		    {
			/* Unsigned comparison!
			 * Have to cast both as GCC is really stupid. */
			flg = ((unsigned) m[n->offset2].v
			    <  (unsigned) m[n->offset3].v);
		    }

		    if (flg < 0) {
			uint_cell ** m2 = m[n->offset2].b;
			uint_cell ** m3 = m[n->offset3].b;

			if (!m[n->offset2].f) {
			    m2 = t2;
			    BI_set_int(m2, m[n->offset2].v);
			}
			if (!m[n->offset3].f) {
			    m3 = t3;
			    BI_set_int(m3, m[n->offset3].v);
			}

			flg = BI_lessthan(m2, m3);
		    }

		    if (flg != 1) break;

		    // p[n->offset] ++;
		    if (!m[n->offset].f) {
			int ov=0, v;
			v = ov_iadd(m[n->offset].v, 1, &ov);
			if (!ov) {
			    m[n->offset].v = v;
			    break;
			}
			m[n->offset].f = 1;
			BI_set_int(m[n->offset].b, m[n->offset].v);
		    }

		    BI_add_cell(m[n->offset].b, 1);
		}
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
	    case T_PRTI:
		if (!m[n->offset].f)
		    putint(m[n->offset].v);
		else {
#ifndef NO_BIG_INT
		    char * number = calloc(byte_per_cell*25/10 + 16, 1);
		    char * sp = number;
		    unsigned int r = 0;
		    BI_copy(t1, m[n->offset].b);

		    do {
			BI_div_uint(t1, 10, &r);
			*sp++ = '0' + r;
		    } while(!BI_is_zero(t1));
		    do {
			putch(*--sp);
		    } while(sp>number);

		    free(number);
#else
		    /* Don't bother supporting "softmult". */
		    int i;
		    for(i=0; i< (int)sizeof(uint_cell)*25/10; i++)
			putch('*');
#endif
		}
		break;

	    case T_INPI:
		{
		    int eof = BI_input(t1), c = 1;
		    if (!eof) {
			m[n->offset].f = 1;
			BI_copy(m[n->offset].b, t1);
			break;
		    }
		    switch(eofcell)
		    {
		    case 2: c = -1; break;
		    case 3: c = 0; break;
		    case 4: c = EOF; break;
		    }
		    if (c <= 0) {
			m[n->offset].f = 0;
			m[n->offset].v = c;
		    }
		}
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
	BI_free(t1); BI_free(t2); BI_free(t3);
	for(i=0; i<dyn_memsize; i++) BI_free(mem[i].b);
	free(mem);
    }
}
