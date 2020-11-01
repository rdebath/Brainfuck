
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

#include "bfi.tree.h"
#include "bfi.gmp.h"

#ifndef DISABLE_GMP
#ifdef PD_GMP
#include "gmp.h"
#define DISABLE_LONGMASK
#else
#include <gmp.h>
#endif

#include "bfi.run.h"
#include "bfi.runarray.h"
#include "clock.h"
#include "ov_long.h"

typedef struct { char f, i; long v; mpz_t b; } mem_cell;
static mem_cell * mem = 0;
static int dyn_memsize = 0;

#define MINALLOC 256

static
mem_cell *
alloc_ptr(mem_cell *p)
{
    int amt, memoff, off;
    mem_cell * nmem = 0;
    if (p-mem >= 0 && p-mem < dyn_memsize) return p;

    memoff = p-mem;
    memoff = (int)((size_t)p/sizeof(*mem)) - (int)((size_t)mem/sizeof(*mem));
    off = 0;
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

static inline void
mpx_add_si(mpz_ptr r, mpz_ptr a, long b)
{
    if (b >= 0)
	mpz_add_ui(r, a, b);
    else
	mpz_sub_ui(r, a, -b);	/* LONG_MIN -> LONG_MIN -> -LONG_MIN : OK */
}

static inline void
mpx_add_cell_si(mem_cell *m, long b) {
    if (!m->f) {
	int ov=0 ; long v;
	v = ov_longadd(m->v, b, &ov);
	if (!ov) {
	    m->v = v;
	    return;
	}
	if (!m->i)
	    mpz_init_set_si(m->b, m->v);
	else
	    mpz_set_si(m->b, m->v);
	m->i = m->f = 1;
    }

    mpx_add_si(m->b, m->b, b);
}

void
run_gmparray(void)
{
    int * progarray, * p;
    mpz_t t1, t2, t3;
    mpz_t cell_and;
    int do_mask = 0;
    register mem_cell * m;
#ifndef DISABLE_LONGMASK
    long long_mask = 0;
#endif

    p = progarray = convert_tree_to_runarray(0);

    if (!mem) mem = calloc(1, sizeof(mem_cell));
    m = move_ptr(mem, 0);

    mpz_init(cell_and);
    mpz_init(t1);
    mpz_init(t2);
    mpz_init(t3);

    if(cell_length>0 && cell_length<INT_MAX) {
	do_mask = 1;
	mpz_set_si(cell_and, 1);
	mpz_mul_2exp(cell_and, cell_and, cell_length);
	mpz_sub_ui(cell_and, cell_and, 1);
#ifndef DISABLE_LONGMASK
	if (mpz_fits_slong_p(cell_and))
	    long_mask = mpz_get_si(cell_and);
#endif
    }

    only_uses_putch = 1;
    start_runclock();

    for(;;) {
	switch(p[0])
	{
	case T_MOV:
	    m = move_ptr(m, p[1]);
	    p += 2;
	    break;

	case T_ADD:
	    // m[p[1]] += p[2]
	    mpx_add_cell_si(&m[p[1]], p[2]);
	    p += 3;
	    break;

	case T_SET:
	    // m[p[1]] = p[2]
	    m[p[1]].f = 0;
	    m[p[1]].v = p[2];
	    p += 3;
	    break;

	case T_END:
	    if (m[p[1]].f) {
		if(do_mask) mpz_and(m[p[1]].b, m[p[1]].b, cell_and);
		if(mpz_sgn(m[p[1]].b) != 0) p += p[2];
	    } else {
#ifndef DISABLE_LONGMASK
		if (long_mask) m[p[1]].v &= long_mask;
#endif
		if(m[p[1]].v != 0) p += p[2];
	    }
	    p += 3;
	    break;

	case T_WHL:
	    if (m[p[1]].f) {
		if(do_mask) mpz_and(m[p[1]].b, m[p[1]].b, cell_and);
		if(mpz_sgn(m[p[1]].b) == 0) p += p[2];
	    } else {
#ifndef DISABLE_LONGMASK
		if (long_mask) m[p[1]].v &= long_mask;
#endif
		if(m[p[1]].v == 0) p += p[2];
	    }
	    p += 3;
	    break;

	case T_CALC:
	    // m[p[1]] = p[2] + m[p[3]] * p[4] + m[p[5]] * p[6];
	    do {
		mpz_ptr m2, m3;
		if (!m[p[3]].f && !m[p[5]].f) {
		    int ov=0 ; long v;
		    v = ov_longadd(p[2],ov_longadd(
				    ov_longmul(m[p[3]].v, p[4], &ov),
				    ov_longmul(m[p[5]].v, p[6], &ov),
				    &ov), &ov);
		    if (!ov) {
			m[p[1]].f = 0;
			m[p[1]].v = v;
			break;
		    }
		}

		if (!m[p[3]].f) {
		    mpz_set_si(t2, m[p[3]].v);
		    m2 = t2;
		} else
		    m2 = m[p[3]].b;
		mpz_mul_si(t2, m2, p[4]);

		if (!m[p[5]].f) {
		    mpz_set_si(t3, m[p[5]].v);
		    m3 = t3;
		} else
		    m3 = m[p[5]].b;
		mpz_mul_si(t3, m3, p[6]);

		if (!m[p[1]].i)
		    mpz_init_set_si(m[p[1]].b, p[2]);
		else
		    mpz_set_si(m[p[1]].b, p[2]);
		m[p[1]].i = m[p[1]].f = 1;

		mpz_add(m[p[1]].b, m[p[1]].b, t2);
		mpz_add(m[p[1]].b, m[p[1]].b, t3);

		if(do_mask) mpz_and(m[p[1]].b, m[p[1]].b, cell_and);

	    } while(0);
	    p += 7;
	    break;

	case T_CALC2:
	    // m[p[1]] = p[2] + m[p[3]] * p[4];
	    do {
		mpz_ptr m3;

		if (!m[p[3]].f) {
		    int ov=0 ; long v;
		    v = ov_longadd(p[2],
				    ov_longmul(m[p[3]].v, p[4], &ov),
				    &ov);
		    if (!ov) {
			m[p[1]].f = 0;
			m[p[1]].v = v;
			break;
		    }
		}
		if (!m[p[3]].f) {
		    mpz_set_si(t3, m[p[3]].v);
		    m3 = t3;
		} else
		    m3 = m[p[3]].b;

		if (!m[p[1]].i)
		    mpz_init(m[p[1]].b);
		m[p[1]].i = m[p[1]].f = 1;

		mpz_set_si(t2, p[2]);
		mpz_mul_si(t3, m3, p[4]);
		mpz_add(m[p[1]].b, t2, t3);

		if(do_mask) mpz_and(m[p[1]].b, m[p[1]].b, cell_and);
	    } while(0);
	    p += 5;
	    break;

	case T_CALC3:
	    // m[p[1]] += m[p[2]] * p[3];
	    do {
		mpz_ptr m2;

		if (!m[p[1]].f && !m[p[2]].f) {
		    int ov=0 ; long v;
		    v = ov_longadd(m[p[1]].v,
				    ov_longmul(m[p[2]].v, p[3], &ov),
				    &ov);
		    if (!ov) {
			m[p[1]].v = v;
			break;
		    }
		}
		if (!m[p[1]].f) {
		    if (!m[p[1]].i)
			mpz_init_set_si(m[p[1]].b, m[p[1]].v);
		    else
			mpz_set_si(m[p[1]].b, m[p[1]].v);
		    m[p[1]].i = m[p[1]].f = 1;
		}

		if (!m[p[2]].f) {
		    mpz_set_si(t2, m[p[2]].v);
		    m2 = t2;
		} else
		    m2 = m[p[2]].b;

		mpz_mul_si(t2, m2, p[3]);
		mpz_add(m[p[1]].b, m[p[1]].b, t2);

		if(do_mask) mpz_and(m[p[1]].b, m[p[1]].b, cell_and);
	    } while(0);
	    p += 4;
	    break;

	case T_CALC4:
	    // m[p[1]] = m[p[2]]
	    if (!m[p[2]].f) {
		m[p[1]].f = 0;
		m[p[1]].v = m[p[2]].v;
	    } else {
		if (!m[p[1]].i)
		    mpz_init(m[p[1]].b);

		m[p[1]].i = m[p[1]].f = 1;
		mpz_set(m[p[1]].b, m[p[2]].b);
	    }
	    p += 3;
	    break;

	case T_CALC5:
	    // m[p[1]] += m[p[2]];
	    do {
		if (!m[p[1]].f && !m[p[2]].f) {
		    int ov=0 ; long v;
		    v = ov_longadd(m[p[1]].v, m[p[2]].v, &ov);
		    if (!ov) {
			m[p[1]].v = v;
			break;
		    }
		}
		if (!m[p[1]].f) {
		    if (!m[p[1]].i)
			mpz_init_set_si(m[p[1]].b, m[p[1]].v);
		    else
			mpz_set_si(m[p[1]].b, m[p[1]].v);
		    m[p[1]].i = m[p[1]].f = 1;
		}

		if (m[p[2]].f)
		    mpz_add(m[p[1]].b, m[p[1]].b, m[p[2]].b);
		else
		    mpx_add_si(m[p[1]].b, m[p[1]].b, m[p[2]].v);

		if(do_mask) mpz_and(m[p[1]].b, m[p[1]].b, cell_and);
	    } while(0);
	    p += 3;
	    break;

	case T_CALCMULT:
	    // m[p[1]] = m[p[2]] * m[p[3]];
	    do {
		mpz_ptr m2, m3;

		if (!m[p[2]].f && !m[p[3]].f)
		{
                    int ov=0 ; long v;
                    v = ov_longmul(m[p[2]].v, m[p[3]].v, &ov);
                    if (!ov) {
                        m[p[1]].f = 0;
                        m[p[1]].v = v;
                        break;
                    }
		}

		if (!m[p[2]].f) {
		    mpz_set_si(t2, m[p[2]].v);
		    m2 = t2;
		} else
		    m2 = m[p[2]].b;
		if (!m[p[3]].f) {
		    mpz_set_si(t3, m[p[3]].v);
		    m3 = t3;
		} else
		    m3 = m[p[3]].b;

		if (!m[p[1]].i)
		    mpz_init(m[p[1]].b);
		m[p[1]].i = m[p[1]].f = 1;

		mpz_mul(m[p[1]].b, m2, m3);

		if(do_mask) mpz_and(m[p[1]].b, m[p[1]].b, cell_and);
	    } while(0);
	    p += 4;
	    break;

	case T_LT:
	    // m[p[1]] += (m[p[2]] < m[p[3]])
	    do {
		int flg = -1;
		if (!m[p[2]].f && !m[p[3]].f)
		{
#ifndef DISABLE_LONGMASK
		    if (long_mask) m[p[2]].v &= long_mask;
		    if (long_mask) m[p[3]].v &= long_mask;
#endif

		    flg = ((unsigned long) m[p[2]].v
			<  (unsigned long) m[p[3]].v);
		}
		if (flg < 0) {
		    mpz_ptr m2 = m[p[2]].b, m3 = m[p[3]].b;

		    if (!m[p[2]].f) {
			mpz_set_si(t2, m[p[2]].v);
			m2 = t2;
		    }
		    if(do_mask) mpz_and(m2, m2, cell_and);

		    if (!m[p[3]].f) {
			mpz_set_si(t3, m[p[3]].v);
			m3 = t3;
		    }
		    if(do_mask) mpz_and(m3, m3, cell_and);

		    // This is an UNSIGNED comparison

		    if (mpz_sgn(m2) >= 0 && mpz_sgn(m3) >= 0 ) {
			flg = (mpz_cmp(m2, m3) < 0);
		    } else if (mpz_sgn(m2) < 0 && mpz_sgn(m3) < 0) {
			flg = (mpz_cmp(m2, m3) < 0);
		    } else
			flg = (mpz_sgn(m3) < 0);
		}
		if (flg == 1)
		    mpx_add_cell_si(&m[p[1]], 1);
	    } while(0);
	    p += 4;
	    break;

	case T_DIV:
	    /* Positive integers do the simple process. */
	    if (!m[p[1]].f && !m[p[1]+1].f && m[p[1]].v>=0 && m[p[1]+1].v>0) {
		long N=m[p[1]].v, D=m[p[1]+1].v;
		m[p[1]+2].f = 0;
		m[p[1]+2].v = N % D;
		m[p[1]+3].f = 0;
		m[p[1]+3].v = N / D;

		p += 2;
		break;
	    }

	    /* NOTE, mpz_divmod is signed. If a mask is defined everything
	     * will become positive, otherwise negative values will cause
	     * problems.
	     *
	     * Dividend/Divisor = Quotient + Remainder/Divisor
	     *         Dividend = Quotient*Divisor + Remainder
	     *
	     * Dividend>0 & Divisor>0       Normal
	     * Dividend>0 & Divisor==0      R=Dividend, Q=0
	     * Dividend>0 & Divisor<0       R=Dividend, Q=0
	     *
	     * Dividend==0                  R=0 Q=0
	     *
	     * Dividend<0 & ...
	     *    Divisor==0                    R=Dividend, Q=0
	     *    Divisor<0 & Dividend<Divisor  R=Dividend, Q=0
	     *    Divisor==1                    R=0 Q=Dividend
	     *    Dividend==Divisor             R=0 Q=1
	     *    Else                          R=Infinity Q=Infinity/D
	     *
	     * A negative divisor would be a value larger than any
	     * positive dividend and so give the same result as
	     * division by zero.
	     *
	     * A negative dividend would cause the original code to
	     * run until negative infinity in order to wrap, without
	     * a known limit the unsigned value is also unknown.
	     * However, a result could be assumed under some conditions.
	     */

	    {
		int flg = 0;
		mpz_ptr m0, m1;
		/* Mask and test dividend cell */
		if (m[p[1]].f) {
		    if(do_mask) mpz_and(m[p[1]].b, m[p[1]].b, cell_and);
		    else
			flg = (mpz_sgn(m[p[1]].b) < 0);
		} else {
#ifndef DISABLE_LONGMASK
		    if (long_mask) m[p[1]].v &= long_mask;
#endif
		    flg = (m[p[1]].v < 0 && !do_mask);
		}
		if (flg) {
		    fprintf(stderr, "DIV command gave infinite loop\n");
		    exit(1);
		}

		/* Mask and test divisor cell */
		if (m[p[1]+1].f) {
		    if(do_mask) mpz_and(m[p[1]+1].b, m[p[1]+1].b, cell_and);
		    flg = (mpz_sgn(m[p[1]+1].b) > 0);
		} else {
#ifndef DISABLE_LONGMASK
		    if (long_mask) m[p[1]+1].v &= long_mask;
#endif
		    flg = (m[p[1]+1].v > 0);
		}

		if (flg) { /* If divisor is not zero */
		    // m[p[1]+2] = N % D
		    // m[p[1]+3] = N / D
		    // mpz_divmod (MP_INT *quotient, MP_INT *remainder, MP_INT *dividend, MP_INT *divisor)

		    /* Convert dividend to large int */
		    if (!m[p[1]].f) {
			mpz_set_si(t2, m[p[1]].v);
			if(do_mask && m[p[1]].v < 0) mpz_and(t2, t2, cell_and);
			m0 = t2;
		    } else
			m0 = m[p[1]].b;

		    /* Convert divisor to large int */
		    if (!m[p[1]+1].f) {
			mpz_set_si(t3, m[p[1]+1].v);
			if(do_mask && m[p[1]].v < 0) mpz_and(t3, t3, cell_and);
			m1 = t3;
		    } else
			m1 = m[p[1]+1].b;

		    if (!m[p[1]+2].i)
			mpz_init(m[p[1]+2].b);
		    m[p[1]+2].i = m[p[1]+2].f = 1;

		    if (!m[p[1]+3].i)
			mpz_init(m[p[1]+3].b);
		    m[p[1]+3].i = m[p[1]+3].f = 1;

		    mpz_divmod(m[p[1]+3].b, m[p[1]+2].b, m0, m1);

		} else {
		    /* Division by zero is same as division by dividend+1 */
		    // m[p[1]+2] = m[p[1]]
		    if (!m[p[1]].f) {
			m[p[1]+2].f = 0;
			m[p[1]+2].v = m[p[1]].v;
		    } else {
			if (!m[p[1]+2].i)
			    mpz_init(m[p[1]+2].b);

			m[p[1]+2].i = m[p[1]+2].f = 1;
			mpz_set(m[p[1]+2].b, m[p[1]].b);
		    }

		    m[p[1]+3].f = 0;
		    m[p[1]+3].v = 0;
		}
	    }

	    p += 2;
	    break;

	case T_ADDWZ:
	    /* This is normally a running dec, it cleans up a rail */
	    // while(m[p[1]] != 0) { m[p[2]] += p[3]; m += p[4]; }

	    do {
		int flg;
		if (m[p[1]].f) {
		    if(do_mask) mpz_and(m[p[1]].b, m[p[1]].b, cell_and);
		    flg = (mpz_sgn(m[p[1]].b) != 0);
		} else {
#ifndef DISABLE_LONGMASK
		    if (long_mask) m[p[1]].v &= long_mask;
#endif
		    flg = (m[p[1]].v != 0);
		}
		if (flg == 0) break;

		mpx_add_cell_si(&m[p[2]], p[3]);
		m += p[4];

		m = move_ptr(m, 0);
	    } while(1);

	    p += 5;
	    break;

	case T_ZFIND:
	    /* Search along a rail til you find the end of it. */
	    // while(m[p[1]] != 0) { m += p[2]; }

	    do {
		int flg;
		if (m[p[1]].f) {
		    if(do_mask) mpz_and(m[p[1]].b, m[p[1]].b, cell_and);
		    flg = (mpz_sgn(m[p[1]].b) != 0);
		} else {
#ifndef DISABLE_LONGMASK
		    if (long_mask) m[p[1]].v &= long_mask;
#endif
		    flg = (m[p[1]].v != 0);
		}
		if (flg == 0) break;

		m += p[2];
	    } while(1);
	    m = move_ptr(m, 0);

	    p += 3;
	    break;

	case T_MFIND:
	    /* Search along a rail for a minus 1 */
            // m[p[1]] -= 1; while(m[p[1]] != -1) m += p[2]; m[p[1]] += 1;

	    mpx_add_cell_si(&m[p[1]], -1);
	    {
		long v = -1;
#ifndef DISABLE_LONGMASK
		if (long_mask) v &= long_mask;
#endif

		do {
		    int flg;
		    if (m[p[1]].f) {
			if(do_mask) mpz_and(m[p[1]].b, m[p[1]].b, cell_and);
			flg = (mpz_cmp(m[p[1]].b, cell_and) != 0);
		    } else {
#ifndef DISABLE_LONGMASK
			if (long_mask) m[p[1]].v &= long_mask;
#endif
			flg = (m[p[1]].v != v);
		    }
		    if (flg == 0) break;

		    m += p[2];
		} while(1);
	    }
	    m = move_ptr(m, 0);

	    mpx_add_cell_si(&m[p[1]], 1);
	    p += 3;
	    break;

	case T_INP:
	    {
		int nv = -256;
		nv = getch(nv);
		if( nv != -256 ) {
		    m[p[1]].f = 0;
		    m[p[1]].v = nv;
		}
	    }
	    p += 2;
	    break;

	case T_PRT:
	    if (!m[p[1]].f)
		putch(m[p[1]].v);
	    else
		putch(mpz_get_si(m[p[1]].b));
	    p += 2;
	    break;

#ifndef PD_GMP
	case T_INPI:
	    // Input number
	    if (!m[p[1]].i)
		mpz_init(m[p[1]].b);
	    m[p[1]].i = m[p[1]].f = 1;
	    mpz_inp_str(m[p[1]].b, stdin, 0);
	    if (mpz_fits_slong_p(m[p[1]].b)) {
		m[p[1]].f = 0;
		m[p[1]].v = mpz_get_si(m[p[1]].b);
	    }
	    p += 2;
	    break;

	case T_PRTI:
	    if (!m[p[1]].f)
		printf("%ld", m[p[1]].v);
	    else {
		// Output number
		if(do_mask) mpz_and(m[p[1]].b, m[p[1]].b, cell_and);
		mpz_out_str(stdout, 10, m[p[1]].b);
	    }
	    p += 2;
	    break;
#endif

#ifdef PD_GMP
	case T_INPI:
	    {
		char *buf = 0;
		int maxp=0, ptr=0, ch;
		/* This is for Old Unix so no: rv = scanf("%ms", &bntxtptr); */

		while((ch=getchar()) != EOF) {
		    if(ch>='0' && ch<='9') {
			while (ptr+2 > maxp) {
			    buf = realloc(buf, maxp += 1024);
			    if(!buf) { perror("realloc"); exit(1); }
			}
			buf[ptr++] = ch;
			buf[ptr] = 0;
		    } else if (ptr>0) {
			ungetc(ch, stdin);
			break;
		    }
		}

		if (buf) {
		    mpz_set_str(m[p[1]].b, buf, 0);
		    free(buf);
		}
	    }
	    break;

	case T_PRTI:
	    if (!m[p[1]].f)
		printf("%ld", m[p[1]].v);
	    else {
		// Output number
		char * s;
		if(do_mask) mpz_and(m[p[1]].b, m[p[1]].b, cell_and);
		s = mpz_get_str((char*)0, 10, m[p[1]].b);
		if (s)
		    printf("%s", s);
	    }
	    p += 2;
	    break;
#endif

	case T_CHR:
	    putch(p[1]);
	    p += 2;
	    break;

	default:
	    fprintf(stderr, "Execution error:\nBad node %s\n",
		tokennames[p[0]]);
	    /*FALLTHROUGH*/
	case T_STOP:
	    fprintf(stderr, "STOP Command executed.\n");
	    /*FALLTHROUGH*/
	case T_FINI:
	    goto break_break;
	}
    }
break_break:;

    finish_runclock(&run_time, &io_time);

    free(progarray);
    mpz_clear(cell_and);
    mpz_clear(t1);
    mpz_clear(t2);
    mpz_clear(t3);
    for(m=mem; m<mem+dyn_memsize; m++)
	if (m->i)
	    mpz_clear(m->b);
    free(mem);
    mem = 0;
}

#endif
