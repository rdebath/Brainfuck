/*
 * This file contains an interpretation engine that is slow but has
 * large cells. It uses the openssl BIGNUM implementation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

#include "bfi.tree.h"
#include "bfi.run.h"
#include "bfi.openssl.h"
#include "clock.h"

#if !defined(DISABLE_SSL)

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
    if (off>=0 && p+max_safe_mem >= mem+dyn_memsize)
        p = alloc_ptr(p+max_safe_mem)-max_safe_mem;
    if (off<=0 && p+min_safe_mem <= mem)
        p = alloc_ptr(p+min_safe_mem)-min_safe_mem;
    return p;
}

void
run_openssltree(void)
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

	    case T_LT:
		if (n->count != 0 || n->count2 != 1 || n->count3 != 1) {
		    fprintf(stderr, "Error on code generation:\n"
			   "Bad T_LT node with incorrect counts.\n");
		    exit(99);
		}
		if(do_mask) {
		    BN_mask_bits(m[n->offset2], cell_length);
		    if (BN_is_negative(m[n->offset2])) {
			if (!BN_set_word(t2, 2))
			    goto BN_op_failed;
			if (!BN_set_word(t3, cell_length))
			    goto BN_op_failed;
			if(!BN_exp(t1, t2, t3, BNtmp))
			    goto BN_op_failed;
			if(!BN_nnmod(m[n->offset2], m[n->offset2], t1, BNtmp))
			    goto BN_op_failed;
		    }
		}
		if(do_mask) {
		    BN_mask_bits(m[n->offset3], cell_length);
		    if (BN_is_negative(m[n->offset3])) {
			if (!BN_set_word(t2, 2))
			    goto BN_op_failed;
			if (!BN_set_word(t3, cell_length))
			    goto BN_op_failed;
			if(!BN_exp(t1, t2, t3, BNtmp))
			    goto BN_op_failed;
			if(!BN_nnmod(m[n->offset3], m[n->offset3], t1, BNtmp))
			    goto BN_op_failed;
		    }
		}

		if (BN_cmp(m[n->offset2], m[n->offset3]) < 0)
		    if (!BN_add_word(m[n->offset], 1))
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
		    if(do_mask) {
			BN_mask_bits(m[n->offset], cell_length);
			if (BN_is_negative(m[n->offset])) {
			    if (!BN_set_word(t2, 2))
				goto BN_op_failed;
			    if (!BN_set_word(t3, cell_length))
				goto BN_op_failed;
			    if(!BN_exp(t1, t2, t3, BNtmp))
				goto BN_op_failed;
			    if(!BN_nnmod(m[n->offset], m[n->offset], t1, BNtmp))
				goto BN_op_failed;
			}
		    }
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
