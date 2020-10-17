/*
 * FREE GMP - a public domain implementation of a subset of the 
 *           gmp library
 *
 * I hearby place the file in the public domain.
 *
 * Do whatever you want with this code. Change it. Sell it. Claim you
 *  wrote it. 
 * Bugs, complaints, flames, rants: please send email to 
 *    Mark Henderson <markh@wimsey.bc.ca>
 * I'm already aware that fgmp is considerably slower than gmp
 *
 * CREDITS:
 *  Paul Rouse <par@r-cube.demon.co.uk> - generic bug fixes, mpz_sqrt and
 *    mpz_sqrtrem, and modifications to get fgmp to compile on a system 
 *    with int and long of different sizes (specifically MSDOS,286 compiler)
 *  Also see the file "notes" included with the fgmp distribution, for
 *    more credits.
 *
 * VERSION 1.0 - beta 5
 */

#include <stdio.h>
#include <sys/types.h>

/* for malloc and free */
#include <stdlib.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef long mp_limb;
typedef unsigned mp_size;

typedef struct mp_int {
    mp_limb *p;
    short sn;
    mp_size sz;
} MP_INT;

#ifdef __STDC__
#define PROTO(x) x
#else
#define PROTO(x) ()
#endif

void mpz_init PROTO((MP_INT *s));
void mpz_init_set PROTO((MP_INT *s, MP_INT *t));
void mpz_init_set_ui PROTO((MP_INT *s, unsigned long v));
void mpz_init_set_si PROTO((MP_INT *y, long v));
void mpz_clear PROTO((MP_INT *s));
void _mpz_realloc PROTO((MP_INT *x, mp_size size));
void mpz_set PROTO((MP_INT *y, MP_INT *x));
void mpz_set_ui PROTO((MP_INT *y, unsigned long v));
unsigned long mpz_get_ui PROTO((MP_INT *y));
long mpz_get_si PROTO((MP_INT *y));
void mpz_set_si PROTO((MP_INT *y, long v));
int mpz_cmp PROTO((MP_INT *x, MP_INT *y));
void mpz_mul PROTO((MP_INT *ww,MP_INT *u, MP_INT *v));
void mpz_mul_2exp PROTO((MP_INT *z, MP_INT *x, unsigned long e));
void mpz_div_2exp PROTO((MP_INT *z, MP_INT *x, unsigned long e));
void mpz_mod_2exp PROTO((MP_INT *z, MP_INT *x, unsigned long e));
void mpz_add PROTO((MP_INT *zz,MP_INT *x,MP_INT *y));
void mpz_add_ui PROTO((MP_INT *x,MP_INT *y, unsigned long n));
void mpz_mul_ui PROTO((MP_INT *x,MP_INT *y, unsigned long n));
void mpz_sub PROTO((MP_INT *z,MP_INT *x, MP_INT *y));
void mpz_sub_ui PROTO((MP_INT *x,MP_INT *y, unsigned long n));
void mpz_div PROTO((MP_INT *q, MP_INT *x, MP_INT *y));
void mpz_mdiv PROTO((MP_INT *q, MP_INT *x, MP_INT *y));
void mpz_mod PROTO((MP_INT *r, MP_INT *x, MP_INT *y));
void mpz_divmod PROTO((MP_INT *q, MP_INT *r, MP_INT *x, MP_INT *y));
void mpz_mmod PROTO((MP_INT *r, MP_INT *x, MP_INT *y));
void mpz_mdivmod PROTO((MP_INT *q,MP_INT *r, MP_INT *x, MP_INT *y));
void mpz_mod_ui PROTO((MP_INT *x,MP_INT *y, unsigned long n));
void mpz_mmod_ui PROTO((MP_INT *x,MP_INT *y, unsigned long n));
void mpz_div_ui PROTO((MP_INT *x,MP_INT *y, unsigned long n));
void mpz_mdiv_ui PROTO((MP_INT *x,MP_INT *y, unsigned long n));
void mpz_divmod_ui PROTO((MP_INT *q,MP_INT *x,MP_INT *y, unsigned long n));
void mpz_mdivmod_ui PROTO((MP_INT *q,MP_INT *x,MP_INT *y, unsigned long n));
unsigned int mpz_sizeinbase PROTO((MP_INT *x, int base));
char *mpz_get_str PROTO((char *s,  int base, MP_INT *x));
int mpz_set_str PROTO((MP_INT *x, char *s, int base));
int mpz_init_set_str PROTO((MP_INT *x, char *s, int base));
void mpz_random PROTO((MP_INT *x, mp_size size));
void mpz_random2 PROTO((MP_INT *x, mp_size size));
size_t mpz_size PROTO((MP_INT *x));
void mpz_abs PROTO((MP_INT *, MP_INT *));
void mpz_neg PROTO((MP_INT *, MP_INT *));
void mpz_fac_ui PROTO((MP_INT *, unsigned long));
void mpz_gcd PROTO((MP_INT *, MP_INT *, MP_INT *));
void mpz_gcdext PROTO((MP_INT *, MP_INT *, MP_INT *, MP_INT *, MP_INT *));
int mpz_jacobi PROTO((MP_INT *, MP_INT *));
int mpz_cmp_ui PROTO((MP_INT *, unsigned long));
int mpz_cmp_si PROTO((MP_INT *, long));
void mpz_and PROTO((MP_INT *, MP_INT *, MP_INT *));
void mpz_or PROTO((MP_INT *, MP_INT *, MP_INT *));
void mpz_xor PROTO((MP_INT *, MP_INT *, MP_INT *));
void mpz_pow_ui PROTO((MP_INT *, MP_INT *, unsigned long));
void mpz_powm PROTO((MP_INT *, MP_INT *, MP_INT *, MP_INT *));
void mpz_powm_ui PROTO((MP_INT *, MP_INT *, unsigned long, MP_INT *));
int mpz_probab_prime_p  PROTO((MP_INT *, int));
void mpz_sqrtrem PROTO((MP_INT *, MP_INT *, MP_INT *));
void mpz_sqrt PROTO((MP_INT *, MP_INT *));
