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

#include "gmp.h"
#ifndef NULL
#define NULL ((void *)0)
#endif

#define iabs(x) ((x>0) ? (x) : (-x))
#define imax(x,y) ((x>y)?x:y)
#define LONGBITS (sizeof(mp_limb) * 8)
#define DIGITBITS (LONGBITS - 2)
#define HALFDIGITBITS ((LONGBITS -2)/2)
#ifndef init
#define init(g) { g = (MP_INT *)malloc(sizeof(MP_INT));  mpz_init(g); }
#endif
#ifndef clear
#define clear(g) { mpz_clear(g); free(g); }
#endif
#ifndef even
#define even(a) (!((a)->p[0] & 1))
#endif
#ifndef odd
#define odd(a) (((a)->p[0] & 1))
#endif


#ifndef B64
/* 
 * The values below are for 32 bit machines (i.e. machines with a 
 *  32 bit long type)
 * You'll need to change them, if you're using something else
 * If DIGITBITS is odd, see the comment at the top of mpz_sqrtrem
 */
#define LMAX 0x3fffffffL
#define LC   0xc0000000L
#define OVMASK 0x2
#define CMASK (LMAX+1)
#define FC ((double)CMASK)
#define HLMAX 0x7fffL
#define HCMASK (HLMAX + 1)
#define HIGH(x) (((x) & 0x3fff8000L) >> 15)
#define LOW(x)  ((x) & 0x7fffL)
#else

/* 64 bit long type */
#define LMAX 0x3fffffffffffffffL
#define LC 0xc000000000000000L
#define OVMASK 0x2
#define CMASK (LMAX+1)
#define FC ((double)CMASK)
#define HLMAX 0x7fffffffL
#define HCMASK (HLMAX + 1)
#define HIGH(x) (((x) & 0x3fffffff80000000L) >> 31)
#define LOW(x) ((x) & 0x7fffffffL)

#endif

#define hd(x,i)  (((i)>=2*(x->sz))? 0:(((i)%2) ? HIGH((x)->p[(i)/2]) \
    : LOW((x)->p[(i)/2])))
#define dg(x,i) ((i < x->sz) ? (x->p)[i] : 0)

#ifdef __GNUC__
#define INLINE inline
#else
#define INLINE
#endif

#define lowdigit(x) (((x)->p)[0])

struct is {
    mp_limb v;
    struct is *next;
};


INLINE static void push(i,sp)
mp_limb i;struct is **sp;
{
    struct is *tmp;
    tmp = *sp;
    *sp = (struct is *) malloc(sizeof(struct is));
    (*sp)->v = i;
    (*sp)->next=tmp;
}

INLINE static mp_limb pop(sp)
struct is **sp;
{
    struct is *tmp;
    mp_limb i;
    if (!(*sp))
        return (-1);
    tmp = *sp;
    *sp = (*sp)->next;
    i = tmp->v;
    tmp->v = 0;
    free(tmp);
    return i;
}

void fatal(s)
char *s;
{
    fprintf(stderr,"%s\n",s);
    exit(123);
}

void mpz_init(s)
MP_INT *s;
{
    s->p = (mp_limb *)malloc(sizeof(mp_limb)*2);
#ifdef DEBUG
    printf("malloc: 8 bytes, %08lx\n", (long)(s->p));
#endif
    if (!(s->p))
        fatal("mpz_init: cannot allocate memory");
    (s->p)[0] = 0;
    (s->p)[1] = 0;
    s->sn=0;
    s->sz=2;
}

void mpz_init_set(s,t)
MP_INT *s,*t;
{
    int i;
    s->p = (mp_limb *)malloc(sizeof(mp_limb) * t->sz);
    if (!(s->p))
        fatal("mpz_init: cannot allocate memory");
    for (i=0;i < t->sz ; i++)
        (s->p)[i] = (t->p)[i];

    s->sn = t->sn;
    s->sz = t->sz;
}

void mpz_init_set_ui(s,v)
MP_INT *s;
unsigned long v;
{
    s->p = (mp_limb *)malloc(sizeof(mp_limb)*2);
    if (!(s->p))
        fatal("mpz_init: cannot allocate memory");
    s->p[0] = (v & LMAX);
    s->p[1] = (v & LC) >> DIGITBITS;
    if (v) 
        s->sn = 1;
    else
        s->sn = 0;
    s->sz = 2;
}

void mpz_init_set_si(y,v)
MP_INT *y;
long v;
{
    y->p = (mp_limb *)malloc(sizeof(mp_limb)*2);
    if (!(y->p))
        fatal("mpz_init: cannot allocate memory");
    if (v < 0) {
        y->sn = -1;
        y->p[0] = (-v) & LMAX;
        y->p[1] = ((-v) & LC) >> DIGITBITS;
    }
    else if (v > 0) {
        y->sn = 1;
        y->p[0] = v & LMAX;
        y->p[1] = (v & LC) >> DIGITBITS;
    }
    else {
        y->sn=0;
        y->p[0] = 0;
        y->p[1] = 0;
    }
    y -> sz = 2;
}



void mpz_clear(s)
MP_INT *s;
{
#ifdef DEBUG
    printf("free: %08lx\n", (long)(s->p));
#endif
    if (s->p)
        free(s->p);
    s->p=NULL;
    s->sn=0;
    s->sz=0;
}

/* number of leading zero bits in digit */
static int lzb(a)
mp_limb a;
{
    mp_limb i; int j;
    j = 0;
    for (i = ((mp_limb)1 << (DIGITBITS-1)); i && !(a&i) ; j++,i>>=1) 
        ;
    return j;
}

void _mpz_realloc(x,size)
MP_INT *x; mp_size size;
{
    if (size > 1 && x->sz < size) {
        int i;
#ifdef DEBUG
    printf("realloc %08lx to size = %ld ", (long)(x->p),(long)(size));
#endif

        if (x->p) 
            x->p=(mp_limb *)realloc(x->p,size * sizeof(mp_limb));
        else
            x->p=(mp_limb *)malloc(size * sizeof(mp_limb));
#ifdef DEBUG
    printf("becomes %08lx\n", (long)(x->p));
#endif

        if (!(x->p))
            fatal("_mpz_realloc: cannot allocate memory");
        for (i=x->sz;i<size;i++)
            (x->p)[i] = 0;
        x->sz = size;
    }
}

void dgset(x,i,n)
MP_INT *x; unsigned int i; mp_limb n;
{
    if (n) {
        if (i >= x->sz) {
            _mpz_realloc(x,(mp_size)(i+1));
            x->sz = i+1;
        }
        (x->p)[i] = n;
    }
}

static unsigned int digits(x)
MP_INT *x;
{
    int i;
    for (i = (x->sz) - 1; i>=0 && (x->p)[i] == 0 ; i--) 
        ;
    return i+1;
}
        
/* y = x */
void mpz_set(y,x)
MP_INT *y; MP_INT *x; 
{
    unsigned int i,k = x->sz;
    if (y->sz < k) {
        k=digits(x);
        _mpz_realloc(y,(mp_size)k);
    }
    if (y->sz > x->sz) {
        mpz_clear(y); mpz_init(y); _mpz_realloc(y,(mp_size)(x->sz));
    }

    for (i=0;i < k; i++)
        (y->p)[i] = (x->p)[i];

    for (;i<y->sz;i++)
        (y->p)[i] = 0;

    y->sn = x->sn;
}

void mpz_set_ui(y,v)
MP_INT *y; unsigned long v; {
    int i;
    for (i=1;i<y->sz;i++)
        y->p[i] = 0;
    y->p[0] = (v & LMAX);
    y->p[1] = (v & LC) >> (DIGITBITS);
    if (v)
        y->sn=1;
    else
        y->sn=0;
}

unsigned long mpz_get_ui(y) 
MP_INT *y; {
    return (y->p[0] | (y->p[1] << DIGITBITS));
}

long mpz_get_si(y)
MP_INT *y; {
    if (y->sn == 0)
        return 0;
    else
        return (y->sn * (y->p[0] | (y->p[1] & 1) << DIGITBITS));
}
        
void mpz_set_si(y,v)
MP_INT *y; long v; {
    int i;
    for (i=1;i<y->sz;i++)
        y->p[i] = 0;
    if (v < 0) {
        y->sn = -1;
        y->p[0] = (-v) & LMAX;
        y->p[1] = ((-v) & LC) >> DIGITBITS;
    }
    else if (v > 0) {
        y->sn = 1;
        y->p[0] = v & LMAX;
        y->p[1] = (v & LC) >> DIGITBITS;
    }
    else {
        y->sn=0;
        y->p[0] = 0;
        y->p[1] = 0;
    }
}

/* z = x + y, without regard for sign */
static void uadd(z,x,y)
MP_INT *z; MP_INT *x; MP_INT *y;
{
    mp_limb c;
    int i;
    MP_INT *t;

    if (y->sz < x->sz) {
        t=x; x=y; y=t;
    }

    /* now y->sz >= x->sz */

    _mpz_realloc(z,(mp_size)((y->sz)+1));

    c=0;
    for (i=0;i<x->sz;i++) {
        if (( z->p[i] = y->p[i] + x->p[i] + c ) & CMASK) {
            c=1; 
            (z->p[i]) &=LMAX;
        }
        else 
            c=0;
    }
    for (;i<y->sz;i++) {
        if ((z->p[i] = (y->p[i] + c)) & CMASK)
            z->p[i]=0;
        else
            c=0;
    }
    (z->p)[y->sz]=c;
}

/* z = y - x, ignoring sign */
/* precondition: abs(y) >= abs(x) */
static void usub(z,y,x)
MP_INT *z; MP_INT *y; MP_INT *x; 
{
    int i;
    mp_limb b,m;
    _mpz_realloc(z,(mp_size)(y->sz));
    b=0;
    for (i=0;i<y->sz;i++) {
        m=((y->p)[i]-b)-dg(x,i);
        if (m < 0) {
            b = 1;
            m = LMAX + 1 + m;
        }
        else
            b = 0;
        z->p[i] = m;
    }
}

/* compare abs(x) and abs(y) */
static int ucmp(y,x)
MP_INT *y;MP_INT *x;
{
    int i;
    for (i=imax(x->sz,y->sz)-1;i>=0;i--) {
        if (dg(y,i) < dg(x,i))
            return (-1);
        else if (dg(y,i) > dg(x,i))
            return 1;
    }
    return 0;
}

int mpz_cmp(x,y)
MP_INT *x; MP_INT *y;
{
    int abscmp;
    if (x->sn < 0 && y->sn > 0)
        return (-1);
    if (x->sn > 0 && y->sn < 0)
        return 1;
    abscmp=ucmp(x,y);
    if (x->sn >=0 && y->sn >=0)
        return abscmp;
    if (x->sn <=0 && y->sn <=0)
        return (-abscmp);
}
    
/* is abs(x) == 0 */
static int uzero(x)
MP_INT *x;
{
    int i;
    for (i=0; i < x->sz; i++)
        if ((x->p)[i] != 0)
            return 0;
    return 1;
}

static void zero(x)
MP_INT *x;
{
    int i;
    x->sn=0;
    for (i=0;i<x->sz;i++)
        (x->p)[i] = 0;
}


/* w = u * v */
void mpz_mul(ww,u,v)
MP_INT *ww;MP_INT *u; MP_INT *v; {
    int i,j;
    mp_limb t0,t1,t2,t3;
    mp_limb cc;
    MP_INT *w;

    w = (MP_INT *)malloc(sizeof(MP_INT));
    mpz_init(w);
    _mpz_realloc(w,(mp_size)(u->sz + v->sz));
    for (j=0; j < 2*u->sz; j++) {
        cc = (mp_limb)0;
        t3 = hd(u,j);
            for (i=0; i < 2*v->sz; i++) {
                t0 = t3 * hd(v,i);
                t1 = HIGH(t0); t0 = LOW(t0);
                if ((i+j)%2) 
                    t2 = HIGH(w->p[(i+j)/2]);
                else
                    t2 = LOW(w->p[(i+j)/2]);
                t2 += cc;
                if (t2 & HCMASK) {
                    cc = 1; t2&=HLMAX;
                }
                else
                    cc = 0;
                t2 += t0;
                if (t2 & HCMASK) {
                    cc++ ; t2&=HLMAX;
                }
                cc+=t1;
                if ((i+j)%2)
                    w->p[(i+j)/2] = LOW(w->p[(i+j)/2]) |
                        (t2 << HALFDIGITBITS);
                else
                    w->p[(i+j)/2] = (HIGH(w->p[(i+j)/2]) << HALFDIGITBITS) |
                        t2;
                    
            }
        if (cc) {
            if ((j+i)%2) 
                w->p[(i+j)/2] += cc << HALFDIGITBITS;
            else
                w->p[(i+j)/2] += cc;
        }
    }
    w->sn = (u->sn) * (v->sn);
    if (w != ww) {
        mpz_set(ww,w);
        mpz_clear(w);
        free(w);
    }
}

/* n must be < DIGITBITS */
static void urshift(c1,a,n)
MP_INT *c1;MP_INT *a;int n;
{
    mp_limb cc = 0;
    if (n >= DIGITBITS)
        fatal("urshift: n >= DIGITBITS");
    if (n == 0)
        mpz_set(c1,a);
    else {
        MP_INT c; int i;
        mp_limb rm = (((mp_limb)1<<n) - 1);
        mpz_init(&c); _mpz_realloc(&c,(mp_size)(a->sz));
        for (i=a->sz-1;i >= 0; i--) {
            c.p[i] = ((a->p[i] >> n) | cc) & LMAX;
            cc = (a->p[i] & rm) << (DIGITBITS - n);
        }
        mpz_set(c1,&c);
        mpz_clear(&c);
    }
}
    
/* n must be < DIGITBITS */
static void ulshift(c1,a,n)
MP_INT *c1;MP_INT *a;int n;
{
    mp_limb cc = 0;
    if (n >= DIGITBITS)
        fatal("ulshift: n >= DIGITBITS");
    if (n == 0)
        mpz_set(c1,a);
    else {
        MP_INT c; int i;
        mp_limb rm = (((mp_limb)1<<n) - 1) << (DIGITBITS -n);
        mpz_init(&c); _mpz_realloc(&c,(mp_size)(a->sz + 1));
        for (i=0;i < a->sz; i++) {
            c.p[i] = ((a->p[i] << n) | cc) & LMAX;
            cc = (a->p[i] & rm) >> (DIGITBITS -n);
        }
        c.p[i] = cc;
        mpz_set(c1,&c);
        mpz_clear(&c);
    }
}

void mpz_div_2exp(z,x,e) 
MP_INT *z; MP_INT *x; unsigned long e;
{
    short sn = x->sn;
    if (e==0)
        mpz_set(z,x);
    else {
        unsigned long i;
        long digs = (e / DIGITBITS);
        unsigned long bs = (e % (DIGITBITS));
        MP_INT y; mpz_init(&y);
        _mpz_realloc(&y,(mp_size)((x->sz) - digs));
        for (i=0; i < (x->sz - digs); i++)
            (y.p)[i] = (x->p)[i+digs];
        if (bs) {
            urshift(z,&y,bs);
        }
        else {
            mpz_set(z,&y);
        }
        if (uzero(z))
            z->sn = 0;
        else
            z->sn = sn;
        mpz_clear(&y);
    }
}

void mpz_mul_2exp(z,x,e) 
MP_INT *z; MP_INT *x; unsigned long e;
{
    short sn = x->sn;
    if (e==0)
        mpz_set(z,x);
    else {
        unsigned long i;
        long digs = (e / DIGITBITS);
        unsigned long bs = (e % (DIGITBITS));
        MP_INT y; mpz_init(&y);
        _mpz_realloc(&y,(mp_size)((x->sz)+digs));
        for (i=digs;i<((x->sz) + digs);i++)
            (y.p)[i] = (x->p)[i - digs];
        if (bs) {
            ulshift(z,&y,bs);
        }
        else {
            mpz_set(z,&y);
        }
        z->sn = sn;
        mpz_clear(&y);
    }
}

void mpz_mod_2exp(z,x,e) 
MP_INT *z; MP_INT *x; unsigned long e;
{
    short sn = x->sn;
    if (e==0)
        mpz_set_ui(z,0L);
    else {
        unsigned long i;
        MP_INT y;
        long digs = (e / DIGITBITS);
        unsigned long bs = (e % (DIGITBITS));
        if (digs > x->sz || (digs == (x->sz) && bs)) {
            mpz_set(z,x);
            return;
        }
            
        mpz_init(&y);
        if (bs)
            _mpz_realloc(&y,(mp_size)(digs+1));
        else
            _mpz_realloc(&y,(mp_size)digs);
        for (i=0; i<digs; i++)
            (y.p)[i] = (x->p)[i];
        if (bs) {
            (y.p)[digs] = ((x->p)[digs]) & (((mp_limb)1 << bs) - 1);
        }
        mpz_set(z,&y);
        if (uzero(z))
            z->sn = 0;
        else
            z->sn = sn;
        mpz_clear(&y);
    }
}
    

/* internal routine to compute x/y and x%y ignoring signs */
static void udiv(qq,rr,xx,yy)
MP_INT *qq; MP_INT *rr; MP_INT *xx; MP_INT *yy;
{
    MP_INT *q, *x, *y, *r;
    int ns,xd,yd,j,f,i,ccc;
    mp_limb zz,z,qhat,b,u,m;
    ccc=0;

    if (uzero(yy))
        return;
    q = (MP_INT *)malloc(sizeof(MP_INT));
    r = (MP_INT *)malloc(sizeof(MP_INT));
    x = (MP_INT *)malloc(sizeof(MP_INT)); y = (MP_INT *)malloc(sizeof(MP_INT));
    if (!x || !y || !q || !r)
        fatal("udiv: cannot allocate memory");
    mpz_init(q); mpz_init(x);mpz_init(y);mpz_init(r);
    _mpz_realloc(x,(mp_size)((xx->sz)+1));
    yd = digits(yy);
    ns = lzb(yy->p[yd-1]);
    ulshift(x,xx,ns);
    ulshift(y,yy,ns);
    xd = digits(x); 
    _mpz_realloc(q,(mp_size)xd);
    xd*=2; yd*=2;
    z = hd(y,yd-1);;
    for (j=(xd-yd);j>=0;j--) {
#ifdef DEBUG
    printf("y=");
    for (i=yd-1;i>=0;i--)
        printf(" %04lx", (long)hd(y,i));
    printf("\n");
    printf("x=");
    for (i=xd-1;i>=0;i--)
        printf(" %04lx", (long)hd(x,i));
    printf("\n");
    printf("z=%08lx\n",(long)z);
#endif
        
        if (z == LMAX)
            qhat = hd(x,j+yd);
        else {
            qhat = ((hd(x,j+yd)<< HALFDIGITBITS) + hd(x,j+yd-1)) / (z+1);
        }
#ifdef DEBUG
    printf("qhat=%08lx\n",(long)qhat);
    printf("hd=%04lx/%04lx\n",(long)hd(x,j+yd),(long)hd(x,j+yd-1));
#endif
        b = 0; zz=0;
        if (qhat) {
            for (i=0; i < yd; i++) {
                zz = qhat * hd(y,i);
                u = hd(x,i+j);
                u-=b;
                if (u<0) {
                    b=1; u+=HLMAX+1;
                }
                else
                    b=0;
                u-=LOW(zz);
                if (u < 0) {
                    b++;
                    u+=HLMAX+1;
                }
                b+=HIGH(zz);
                if ((i+j)%2) 
                    x->p[(i+j)/2] = LOW(x->p[(i+j)/2]) | (u << HALFDIGITBITS);
                else
                    x->p[(i+j)/2] = (HIGH(x->p[(i+j)/2]) << HALFDIGITBITS) | u;
            }
            if (b) {
                if ((j+i)%2) 
                    x->p[(i+j)/2] -= b << HALFDIGITBITS;
                else
                    x->p[(i+j)/2] -= b;
            }
        }
#ifdef DEBUG
        printf("x after sub=");
        for (i=xd-1;i>=0;i--)
            printf(" %04lx", (long)hd(x,i));
        printf("\n");
#endif
        for(;;zz++) {
            f=1;
            if (!hd(x,j+yd)) {
                for(i=yd-1; i>=0; i--) {
                    if (hd(x,j+i) > hd(y,i)) {
                        f=1;
                        break;
                    }
                    if (hd(x,j+i) < hd(y,i)) {
                        f=0;
                        break;
                    }
                }
            }
            if (!f)
                break;
            qhat++;
            ccc++;
#ifdef DEBUG
            printf("corrected qhat = %08lx\n", (long)qhat);
#endif
            b=0;
            for (i=0;i<yd;i++) {
                m = hd(x,i+j)-hd(y,i)-b;
                if (m < 0) {
                    b = 1;
                    m = HLMAX + 1 + m;
                }
                else
                    b = 0;
                if ((i+j)%2) 
                    x->p[(i+j)/2] = LOW(x->p[(i+j)/2]) | (m << HALFDIGITBITS);
                else
                    x->p[(i+j)/2] = (HIGH(x->p[(i+j)/2]) << HALFDIGITBITS) | m;
            }
            if (b) {
                if ((j+i)%2) 
                    x->p[(i+j)/2] -= b << HALFDIGITBITS;
                else
                    x->p[(i+j)/2] -= b;
            }
#ifdef DEBUG
            printf("x after cor=");
            for (i=2*(x->sz)-1;i>=0;i--)
                printf(" %04lx", (long)hd(x,i));
            printf("\n");
#endif
        }
        if (j%2) 
            q->p[j/2] |= qhat << HALFDIGITBITS;
        else
            q->p[j/2] |= qhat;
#ifdef DEBUG
            printf("x after cor=");
            for (i=xd - 1;i>=0;i--)
                printf(" %04lx", (long)hd(q,i));
            printf("\n");
#endif
    }
    _mpz_realloc(r,(mp_size)(yy->sz));
    zero(r);
    urshift(r,x,ns);
    mpz_set(rr,r);
    mpz_set(qq,q);
    mpz_clear(x); mpz_clear(y);
    mpz_clear(q);  mpz_clear(r);
    free(q); free(x); free(y); free(r);
}

void mpz_add(zz,x,y)
MP_INT *zz;MP_INT *x;MP_INT *y;
{
    int mg;
    MP_INT *z;
    if (x->sn == 0) {
        mpz_set(zz,y);
        return;
    }
    if (y->sn == 0) {
        mpz_set(zz,x);
        return;
    }
    z = (MP_INT *)malloc(sizeof(MP_INT));
    mpz_init(z);

    if (x->sn > 0 && y->sn > 0) {
        uadd(z,x,y); z->sn = 1;
    }
    else if (x->sn < 0 && y->sn < 0) {
        uadd(z,x,y); z->sn = -1;
    }
    else {
        /* signs differ */
        if ((mg = ucmp(x,y)) == 0) {
            zero(z);
        }
        else if (mg > 0) {  /* abs(y) < abs(x) */
            usub(z,x,y);    
            z->sn = (x->sn > 0 && y->sn < 0) ? 1 : (-1);
        }
        else { /* abs(y) > abs(x) */
            usub(z,y,x);    
            z->sn = (x->sn < 0 && y->sn > 0) ? 1 : (-1);
        }
    }
    mpz_set(zz,z);
    mpz_clear(z);
    free(z);
}

void mpz_add_ui(x,y,n)
MP_INT *x;MP_INT *y; unsigned long n;
{
    MP_INT z;
    mpz_init_set_ui(&z,n);
    mpz_add(x,y,&z);
    mpz_clear(&z);
}

int mpz_cmp_ui(x,n)
MP_INT *x; unsigned long n;
{
    MP_INT z; int ret;
    mpz_init_set_ui(&z,n);
    ret=mpz_cmp(x,&z);
    mpz_clear(&z);
    return ret;
}

int mpz_cmp_si(x,n)
MP_INT *x; long n;
{
    MP_INT z; int ret;
    mpz_init(&z);
    mpz_set_si(&z,n);
    ret=mpz_cmp(x,&z);
    mpz_clear(&z);
    return ret;
}


void mpz_mul_ui(x,y,n)
MP_INT *x;MP_INT *y; unsigned long n;
{
    MP_INT z;
    mpz_init_set_ui(&z,n);
    mpz_mul(x,y,&z);
    mpz_clear(&z);
}
    
    
/* z = x - y  -- just use mpz_add - I'm lazy */
void mpz_sub(z,x,y)
MP_INT *z;MP_INT *x; MP_INT *y;
{
    MP_INT u;
    mpz_init(&u);
    mpz_set(&u,y);
    u.sn = -(u.sn);
    mpz_add(z,x,&u);
    mpz_clear(&u);
}

void mpz_sub_ui(x,y,n)
MP_INT *x;MP_INT *y; unsigned long n;
{
    MP_INT z;
    mpz_init_set_ui(&z,n);
    mpz_sub(x,y,&z);
    mpz_clear(&z);
}

void mpz_div(q,x,y)
MP_INT *q; MP_INT *x; MP_INT *y;
{
    MP_INT r; 
    short sn1 = x->sn, sn2 = y->sn;
    mpz_init(&r);
    udiv(q,&r,x,y);
    q->sn = sn1*sn2;
    if (uzero(q))
        q->sn = 0;
    mpz_clear(&r);
}

void mpz_mdiv(q,x,y)
MP_INT *q; MP_INT *x; MP_INT *y;
{
    MP_INT r; 
    short sn1 = x->sn, sn2 = y->sn, qsign;
    mpz_init(&r);
    udiv(q,&r,x,y);
    qsign = q->sn = sn1*sn2;
    if (uzero(q))
        q->sn = 0;
    /* now if r != 0 and q < 0 we need to round q towards -inf */
    if (!uzero(&r) && qsign < 0) 
        mpz_sub_ui(q,q,1L);
    mpz_clear(&r);
}

void mpz_mod(r,x,y)
MP_INT *r; MP_INT *x; MP_INT *y;
{
    MP_INT q;
    short sn = x->sn;
    mpz_init(&q);
    if (x->sn == 0) {
        zero(r);
        return;
    }
    udiv(&q,r,x,y);
    r->sn = sn;
    if (uzero(r))
        r->sn = 0;
    mpz_clear(&q);
}

void mpz_divmod(q,r,x,y)
MP_INT *q; MP_INT *r; MP_INT *x; MP_INT *y;
{
    short sn1 = x->sn, sn2 = y->sn;
    if (x->sn == 0) {
        zero(q);
        zero(r);
        return;
    }
    udiv(q,r,x,y);
    q->sn = sn1*sn2;
    if (uzero(q))
        q->sn = 0;
    r->sn = sn1;
    if (uzero(r))
        r->sn = 0;
}

void mpz_mmod(r,x,y)
MP_INT *r; MP_INT *x; MP_INT *y;
{
    MP_INT q;
    short sn1 = x->sn, sn2 = y->sn;
    mpz_init(&q);
    if (sn1 == 0) {
        zero(r);
        return;
    }
    udiv(&q,r,x,y);
    if (uzero(r)) {
        r->sn = 0;
        return;
    }
    q.sn = sn1*sn2;
    if (q.sn > 0) 
        r->sn = sn1;
    else if (sn1 < 0 && sn2 > 0) {
        r->sn = 1;
        mpz_sub(r,y,r);
    }
    else {
        r->sn = 1;
        mpz_add(r,y,r);
    }
}

void mpz_mdivmod(q,r,x,y)
MP_INT *q;MP_INT *r; MP_INT *x; MP_INT *y;
{
    short sn1 = x->sn, sn2 = y->sn, qsign;
    if (sn1 == 0) {
        zero(q);
        zero(r);
        return;
    }
    udiv(q,r,x,y);
    qsign = q->sn = sn1*sn2;
    if (uzero(r)) {
        /* q != 0, since q=r=0 would mean x=0, which was tested above */
        r->sn = 0;
        return;
    }
    if (q->sn > 0) 
        r->sn = sn1;
    else if (sn1 < 0 && sn2 > 0) {
        r->sn = 1;
        mpz_sub(r,y,r);
    }
    else {
        r->sn = 1;
        mpz_add(r,y,r);
    }
    if (uzero(q))
        q->sn = 0;
    /* now if r != 0 and q < 0 we need to round q towards -inf */
    if (!uzero(r) && qsign < 0) 
        mpz_sub_ui(q,q,1L);
}

void mpz_mod_ui(x,y,n)
MP_INT *x;MP_INT *y; unsigned long n;
{
    MP_INT z;
    mpz_init(&z);
    mpz_set_ui(&z,n);
    mpz_mod(x,y,&z);
    mpz_clear(&z);
}

void mpz_mmod_ui(x,y,n)
MP_INT *x;MP_INT *y; unsigned long n;
{
    MP_INT z;
    mpz_init(&z);
    mpz_set_ui(&z,n);
    mpz_mmod(x,y,&z);
    mpz_clear(&z);
}

void mpz_div_ui(x,y,n)
MP_INT *x;MP_INT *y; unsigned long n;
{
    MP_INT z;
    mpz_init_set_ui(&z,n);
    mpz_div(x,y,&z);
    mpz_clear(&z);
}

void mpz_mdiv_ui(x,y,n)
MP_INT *x;MP_INT *y; unsigned long n;
{
    MP_INT z;
    mpz_init_set_ui(&z,n);
    mpz_mdiv(x,y,&z);
    mpz_clear(&z);
}
void mpz_divmod_ui(q,x,y,n)
MP_INT *q;MP_INT *x;MP_INT *y; unsigned long n;
{
    MP_INT z;
    mpz_init_set_ui(&z,n);
    mpz_divmod(q,x,y,&z);
    mpz_clear(&z);
}

void mpz_mdivmod_ui(q,x,y,n)
MP_INT *q;MP_INT *x;MP_INT *y; unsigned long n;
{
    MP_INT z;
    mpz_init_set_ui(&z,n);
    mpz_mdivmod(q,x,y,&z);
    mpz_clear(&z);
}


/* 2<=base <=36 - this overestimates the optimal value, which is OK */
unsigned int mpz_sizeinbase(x,base)
MP_INT *x; int base;
{
    int i,j;
    int bits = digits(x) * DIGITBITS;
    if (base < 2 || base > 36)
        fatal("mpz_sizeinbase: invalid base");
    for (j=0,i=1; i<=base;i*=2,j++)
        ;
    return ((bits)/(j-1)+1);
}

char *mpz_get_str(s,base,x)
char *s;  int base; MP_INT *x; {
    MP_INT xx,q,r,bb;
    char *p,*t,*ps;
    int sz = mpz_sizeinbase(x,base);
    mp_limb d;
    if (base < 2 || base > 36)
        return s;
    t = (char *)malloc(sz+2);
    if (!t) 
        fatal("cannot allocate memory in mpz_get_str");
    if (!s) {
        s=(char *)malloc(sz+2);
        if (!s) 
            fatal("cannot allocate memory in mpz_get_str");
    }
    if (uzero(x)) {
        *s='0';
        *(s+1)='\0';
        return s;
    }
    mpz_init(&xx); mpz_init(&q); mpz_init(&r); mpz_init(&bb);
    mpz_set(&xx,x);
    mpz_set_ui(&bb,(unsigned long)base);
    ps = s;
    if (x->sn < 0) {
        *ps++= '-';
        xx.sn = 1;
    }
    p = t;
    while (!uzero(&xx)) {
        udiv(&xx,&r,&xx,&bb);
        d = r.p[0];
        if (d < 10) 
            *p++  = (char) (r.p[0] + '0');
        else 
            *p++  = (char) (r.p[0] + -10 + 'a');
    }

    p--;
    for (;p>=t;p--,ps++)
        *ps = *p;
    *ps='\0';
    
    mpz_clear(&q); mpz_clear(&r); free(t);  
    return s;
}


int mpz_set_str(x,s,base)
MP_INT *x; char *s; int base;
{
    int l,i;
    int retval = 0;
    MP_INT t,m,bb;
    short sn;
    unsigned int k;
    mpz_init(&m);
    mpz_init(&t);
    mpz_init(&bb);
    mpz_set_ui(&m,1L);
    zero(x);
    while (*s==' ' || *s=='\t' || *s=='\n')
        s++;
    if (*s == '-') {
        sn = -1; s++;
    }
    else
        sn = 1;
    if (base == 0) {
        if (*s == '0') {
            if (*(s+1) == 'x' || *(s+1) == 'X') {
                base = 16;
                s+=2;   /* toss 0x */
            }
            else {
                base = 8;
                s++;    /* toss 0 */
            }
        }
        else
            base=10;
    }
    if (base < 2 || base > 36)
        fatal("mpz_set_str: invalid base");
    mpz_set_ui(&bb,(unsigned long)base);
    l=strlen(s);
    for (i = l-1; i>=0; i--) {
        if (s[i]==' ' || s[i]=='\t' || s[i]=='\n') 
            continue;
        if (s[i] >= '0' && s[i] <= '9')
            k = (unsigned int)s[i] - (unsigned int)'0';
        else if (s[i] >= 'A' && s[i] <= 'Z')
            k = (unsigned int)s[i] - (unsigned int)'A'+10;
        else if (s[i] >= 'a' && s[i] <= 'z')
            k = (unsigned int)s[i] - (unsigned int)'a'+10;
        else {
            retval = (-1);
            break;
        }
        if (k >= base) {
            retval = (-1);
            break;
        }
        mpz_mul_ui(&t,&m,(unsigned long)k);
        mpz_add(x,x,&t);
        mpz_mul(&m,&m,&bb);
#ifdef DEBUG
        printf("k=%d\n",k);
        printf("t=%s\n",mpz_get_str(NULL,10,&t));
        printf("x=%s\n",mpz_get_str(NULL,10,x));
        printf("m=%s\n",mpz_get_str(NULL,10,&m));
#endif
    }
    if (x->sn)
        x->sn = sn;
    mpz_clear(&m);
    mpz_clear(&bb);
    mpz_clear(&t);
    return retval;
}

int mpz_init_set_str(x,s,base)
MP_INT *x; char *s; int base;
{
    mpz_init(x); return mpz_set_str(x,s,base);
}

#define mpz_randombyte (rand()& 0xff)

void mpz_random(x,size)
MP_INT *x; unsigned int size;
{
    unsigned int bits = size * LONGBITS;
    unsigned int digits = bits/DIGITBITS;
    unsigned int oflow = bits % DIGITBITS;
    unsigned int i,j;
    long t;
    if (oflow)
        digits++;
    _mpz_realloc(x,(mp_size)digits);

    for (i=0; i<digits; i++) {
        t = 0;
        for (j=0; j < DIGITBITS; j+=8)
            t = (t << 8) | mpz_randombyte; 
        (x->p)[i] = t & LMAX;
    }
    if (oflow) 
        (x->p)[digits-1] &= (((mp_limb)1 << oflow) - 1);
}
void mpz_random2(x,size)
MP_INT *x; unsigned int size;
{
    unsigned int bits = size * LONGBITS;
    unsigned int digits = bits/DIGITBITS;
    unsigned int oflow = bits % DIGITBITS;
    unsigned int i,j;
    long t;
    if (oflow)
        digits++;
    _mpz_realloc(x,(mp_size)digits);

    for (i=0; i<digits; i++) {
        t = 0;
        for (j=0; j < DIGITBITS; j+=8)
            t = (t << 8) | mpz_randombyte; 
        (x->p)[i] = (t & LMAX) % 2;
    }
    if (oflow) 
        (x->p)[digits-1] &= (((mp_limb)1 << oflow) - 1);
}

size_t mpz_size(x)
MP_INT *x;
{
    int bits = (x->sz)*DIGITBITS;
    size_t r;
    
    r = bits/LONGBITS;
    if (bits % LONGBITS)
        r++;
    return r;
}

void mpz_abs(x,y)
MP_INT *x; MP_INT *y;
{
    if (x!=y)
        mpz_set(x,y);
    if (uzero(x))
        x->sn = 0;
    else
        x->sn = 1;
}

void mpz_neg(x,y)
MP_INT *x; MP_INT *y;
{
    if (x!=y)
        mpz_set(x,y);
    x->sn = -(y->sn);
}

void mpz_fac_ui(x,n)
MP_INT *x; unsigned long n;
{
    mpz_set_ui(x,1L);
    if (n==0 || n == 1)
        return;
    for (;n>1;n--)
        mpz_mul_ui(x,x,n);
}

void mpz_gcd(gg,aa,bb)
MP_INT *gg;
MP_INT *aa;
MP_INT *bb;
{
    MP_INT *g,*t,*a,*b;
    int freeg;
    
    t = (MP_INT *)malloc(sizeof(MP_INT));
    g = (MP_INT *)malloc(sizeof(MP_INT));
    a = (MP_INT *)malloc(sizeof(MP_INT));
    b = (MP_INT *)malloc(sizeof(MP_INT));
    if (!a || !b || !g || !t)
        fatal("mpz_gcd: cannot allocate memory");
    mpz_init(g); mpz_init(t); mpz_init(a); mpz_init(b);
    mpz_abs(a,aa); mpz_abs(b,bb); 

    while (!uzero(b)) {
        mpz_mod(t,a,b);
        mpz_set(a,b);
        mpz_set(b,t);
    }
    
    mpz_set(gg,a);
    mpz_clear(g); mpz_clear(t); mpz_clear(a); mpz_clear(b);
    free(g); free(t); free(a); free(b);
}

void mpz_gcdext(gg,xx,yy,aa,bb)
MP_INT *gg,*xx,*yy,*aa,*bb;
{
    MP_INT *x, *y, *g, *u, *q;

    if (uzero(bb)) {
        mpz_set(gg,aa);
        mpz_set_ui(xx,1L);
        if (yy)
            mpz_set_ui(yy,0L);
        return;
    }
    
    g = (MP_INT *)malloc(sizeof(MP_INT)); mpz_init(g);
    q = (MP_INT *)malloc(sizeof(MP_INT)); mpz_init(q);
    y = (MP_INT *)malloc(sizeof(MP_INT)); mpz_init(y);
    x = (MP_INT *)malloc(sizeof(MP_INT)); mpz_init(x);
    u = (MP_INT *)malloc(sizeof(MP_INT)); mpz_init(u);

    if (!g || !q || !y || !x || !u)
        fatal("mpz_gcdext: cannot allocate memory");

    mpz_divmod(q,u,aa,bb);
    mpz_gcdext(g,x,y,bb,u);
    if (yy) {
        mpz_mul(u,q,y);
        mpz_sub(yy,x,u);
    }
    mpz_set(xx,y);
    mpz_set(gg,g);
    mpz_clear(g); mpz_clear(q); mpz_clear(y); mpz_clear(x); mpz_clear(u);
    free(g); free(q); free(y); free(x); free(u);
}


/*
 *    a is a quadratic residue mod b if
 *    x^2 = a mod b      has an integer solution x.
 *
 *    J(a,b) = if a==1 then 1 else
 *             if a is even then J(a/2,b) * ((-1)^(b^2-1)/8))
 *             else J(b mod a, a) * ((-1)^((a-1)*(b-1)/4)))
 *
 *  b>0  b odd
 *
 *
 */

int mpz_jacobi(ac,bc)
MP_INT *ac, *bc;
{
    int sgn = 1;
    unsigned long c;
    MP_INT *t,*a,*b; 
    if (bc->sn <=0)
        fatal("mpz_jacobi call with b <= 0");
    if (even(bc))
        fatal("mpz_jacobi call with b even");

    init(t); init(a); init(b);

    /* if ac < 0, then we use the fact that 
     *  (-1/b)= -1 if b mod 4 == 3
     *          +1 if b mod 4 == 1
     */

    if (mpz_cmp_ui(ac,0L) < 0 && (lowdigit(bc) % 4) == 3)
        sgn=-sgn;

    mpz_abs(a,ac); mpz_set(b,bc);

    /* while (a > 1) { */
    while(mpz_cmp_ui(a,1L) > 0) {

        if (even(a)) {

            /* c = b % 8 */
            c = lowdigit(b) & 0x07;

            /* b odd, then (b^2-1)/8 is even iff (b%8 == 3,5) */

            /* if b % 8 == 3 or 5 */
            if (c == 3 || c == 5)
                sgn = -sgn;

            /* a = a / 2 */
            mpz_div_2exp(a,a,1L); 

        } 
        else {
            /* note: (a-1)(b-1)/4 odd iff a mod 4==b mod 4==3 */

            /* if a mod 4 == 3 and b mod 4 == 3 */
            if (((lowdigit(a) & 3) == 3) && ((lowdigit(b) & 3) == 3))
                sgn = -sgn;

            /* set (a,b) = (b mod a,a) */
            mpz_set(t,a); mpz_mmod(a,b,t); mpz_set(b,t);
        } 
    }

    /* if a == 0 then sgn = 0 */
    if (uzero(a))
        sgn=0;

    clear(t); clear(a); clear(b);
    return (sgn);
}

void mpz_and(z,x,y) /* not the most efficient way to do this */
MP_INT *z,*x,*y;
{
    int i,sz;
    sz = imax(x->sz, y->sz);
    _mpz_realloc(z,(mp_size)sz);
    for (i=0; i < sz; i++)
        (z->p)[i] = dg(x,i) & dg(y,i);
    if (x->sn < 0 && y->sn < 0)
        z->sn = (-1);
    else
        z->sn = 1;
    if (uzero(z))
        z->sn = 0;
}

void mpz_or(z,x,y)  /* not the most efficient way to do this */
MP_INT *z,*x,*y;
{
    int i,sz;
    sz = imax(x->sz, y->sz);
    _mpz_realloc(z,(mp_size)sz);
    for (i=0; i < sz; i++)
        (z->p)[i] = dg(x,i) | dg(y,i);
    if (x->sn < 0 || y->sn < 0)
        z->sn = (-1);
    else
        z->sn = 1;
    if (uzero(z))
        z->sn = 0;
}

void mpz_xor(z,x,y)  /* not the most efficient way to do this */
MP_INT *z,*x,*y;
{
    int i,sz;
    sz = imax(x->sz, y->sz);
    _mpz_realloc(z,(mp_size)sz);
    for (i=0; i < sz; i++)
        (z->p)[i] = dg(x,i) ^ dg(y,i);
    if ((x->sn <= 0 && y->sn > 0) || (x->sn > 0 && y->sn <=0))
        z->sn = (-1);
    else
        z->sn = 1;
    if (uzero(z))
        z->sn = 0;
}
void mpz_pow_ui(zz,x,e)
MP_INT *zz, *x;
unsigned long e;
{
    MP_INT *t;
    unsigned long mask = (1L<< (LONGBITS-1));
    
    if (e==0) {
        mpz_set_ui(zz,1L);
        return;
    }

    init(t);
    mpz_set(t,x);
    for (;!(mask &e); mask>>=1) 
        ;
    mask>>=1;
    for (;mask!=0; mask>>=1) {
        mpz_mul(t,t,t);
        if (e & mask)
            mpz_mul(t,t,x);
    }
    mpz_set(zz,t);
    clear(t);
}

void mpz_powm(zz,x,ee,n)
MP_INT *zz,*x,*ee,*n;
{
    MP_INT *t,*e;
    struct is *stack = NULL;
    int k,i;
    
    if (uzero(ee)) {
        mpz_set_ui(zz,1L);
        return;
    }

    if (ee->sn < 0) {
        return;
    }
    init(e); init(t); mpz_set(e,ee);

    for (k=0;!uzero(e);k++,mpz_div_2exp(e,e,1L))
        push(lowdigit(e) & 1,&stack);
    k--;
    i=pop(&stack);

    mpz_mod(t,x,n);  /* t=x%n */

    for (i=k-1;i>=0;i--) {
        mpz_mul(t,t,t); 
        mpz_mod(t,t,n);  
        if (pop(&stack)) {
            mpz_mul(t,t,x); 
            mpz_mod(t,t,n);
        }
    }
    mpz_set(zz,t);
    clear(t);
}

void mpz_powm_ui(z,x,e,n)
MP_INT *z,*x,*n; unsigned long e;
{
    MP_INT f;
    mpz_init(&f);
    mpz_set_ui(&f,e);
    mpz_powm(z,x,&f,n);
    mpz_clear(&f);
}
    
    

/* Miller-Rabin */
static int witness(x,n)
MP_INT *x, *n;
{
    MP_INT *t,*e, *e1;
    struct is *stack = NULL;
    int trivflag = 0;
    int k,i;
    
    init(e1); init(e); init(t); mpz_sub_ui(e,n,1L); mpz_set(e1,e);

    for (k=0;!uzero(e);k++,mpz_div_2exp(e,e,1L))
        push(lowdigit(e) & 1,&stack);
    k--;
    i=pop(&stack);

    mpz_mod(t,x,n);  /* t=x%n */

    for (i=k-1;i>=0;i--) {
        trivflag = !mpz_cmp_ui(t,1L) || !mpz_cmp(t,e1);
        mpz_mul(t,t,t); mpz_mod(t,t,n);  
        if (!trivflag && !mpz_cmp_ui(t,1L)) {
            clear(t); clear(e); clear(e1);
            return 1;
        }
            
        if (pop(&stack)) {
            mpz_mul(t,t,x); 
            mpz_mod(t,t,n);
        }
    }
    if (mpz_cmp_ui(t,1L))  { /* t!=1 */
        clear(t); clear(e); clear(e1);
        return 1;
    }
    else {
        clear(t); clear(e); clear(e1);
        return 0;
    }
}

unsigned long smallp[] = {2,3,5,7,11,13,17};
int mpz_probab_prime_p(nn,s)
MP_INT *nn; int s;
{   
    MP_INT *a,*n;
    int j,k,i;
    long t;

    if (uzero(nn))
        return 0;
    init(n); mpz_abs(n,nn);
    if (!mpz_cmp_ui(n,1L)) {
        clear(n);
        return 0;
    }
    init(a);
    for (i=0;i<sizeof(smallp)/sizeof(unsigned long); i++) {
        mpz_mod_ui(a,n,smallp[i]);
        if (uzero(a)) {
            clear(a); clear(n); return 0;
        }
    }
    _mpz_realloc(a,(mp_size)(n->sz));
    for (k=0; k<s; k++) {

        /* generate a "random" a */
            for (i=0; i<n->sz; i++) {
                t = 0;
                for (j=0; j < DIGITBITS; j+=8)
                    t = (t << 8) | mpz_randombyte; 
                (a->p)[i] = t & LMAX;
            }
            a->sn = 1;
            mpz_mod(a,a,n);

        if (witness(a,n)) {
            clear(a); clear(n);
            return 0;
        }
    }
    clear(a);clear(n);
    return 1;
}   


/* Square roots are found by Newton's method, but since we are working
 * with integers we have to consider carefully the termination conditions.
 * If we are seeking x = floor(sqrt(a)), the iteration is
 *     x_{n+1} = floor ((floor (a/x_n) + x_n)/2) == floor ((a + x_n^2)/(2*x))
 * If eps_n represents the error (exactly, eps_n and sqrt(a) real) in the form:
 *     x_n = (1 + eps_n) sqrt(a)
 * then it is easy to show that for a >= 4
 *     if 0 <= eps_n, then either 0 <= eps_{n+1} <= (eps_n^2)/2
 *                         or x_{n+1} = floor (sqrt(a))
 *     if x_n = floor (sqrt(a)), then either x_{n+1} = floor (sqrt(a))
 *                               or x_{n+1} = floor (sqrt(a)) + 1
 * Therefore, if the initial approximation x_0 is chosen so that
 *     0 <= eps_0 <= 1/2
 * then the error remains postive and monotonically decreasing with
 *     eps_n <= 2 ^ (-(2^(n+1) - 1))
 * unless we reach the correct floor(sqrt(a)), after which we may oscillate
 * between it and the value one higher.
 * We choose the number of iterations, k, to guarantee
 *     eps_k sqrt(a) < 1,  so x_k <= floor (sqrt (a)) + 1
 * so finally x_k is either the required answer or one too big (which we
 * check by a simple multiplication and comparison).
 *
 * The calculation of the initial approximation *assumes* DIGITBITS is even.
 * It probably always will be, so for now let's leave the code simple,
 * clear and all reachable in testing!
 */
void mpz_sqrtrem (root, rem, a)
    MP_INT *root;
    MP_INT *rem;
    MP_INT *a;
{
    MP_INT tmp;
    MP_INT r;
    mp_limb z;
    unsigned long j, h;
    int k;

    if (a->sn < 0)
        /* Negative operand, nothing correct we can do */
        return;

    /* Now, a good enough approximation to the root is obtained by writing
     *     a = z * 2^(2j) + y,  4 <= z <= 15 and 0 <= y < 2^(2j)
     * then taking
     *     root = ciel (sqrt(z+1)) * 2^j
     */

    for (j = a->sz - 1; j != 0 && (a->p)[j] == 0; j--);
    z = (a->p)[j];
    if (z < 4) {
        if (j == 0) {
            /* Special case for small operand (main interation not valid) */
            mpz_set_ui (root, (z>0) ? 1L : 0L);
            if (rem)
                mpz_set_ui (rem, (z>1) ? z-1L : 0L);
            return;
        } else {
            z = (z << 2) + ((a->p)[j-1] >> (DIGITBITS-2));
            j = j * DIGITBITS - 2;
        }
    } else {
        j = j * DIGITBITS;
        while (z & ~0xf) {
            z >>= 2;
            j += 2;
        }
    }
    j >>= 1;                            /* z and j now as described above */
    for (k=1, h=4; h < j+3; k++, h*=2); /* 2^(k+1) >= j+3, since a < 2^(2j+4) */
    mpz_init_set_ui (&r, (z>8) ? 4L : 3L);
    mpz_mul_2exp (&r, &r, j);

#ifdef DEBUG
    printf ("mpz_sqrtrem: z = %lu, j = %lu, k = %d\n", (long)z, j, k);
#endif

    /* Main iteration */
    mpz_init (&tmp);
    for ( ; k > 0; k--) {
        mpz_div (&tmp, a, &r);
        mpz_add (&r, &tmp, &r);
        mpz_div_2exp (&r, &r, 1L);
    }

    /* Adjust result (which may be one too big) and set remainder */
    mpz_mul (&tmp, &r, &r);
    if (mpz_cmp (&tmp, a) > 0) {
        mpz_sub_ui (root, &r, 1L);
        if (rem) {
            mpz_sub (rem, a, &tmp);
            mpz_add (rem, rem, &r);
            mpz_add (rem, rem, &r);
            mpz_sub_ui (rem, rem, 1L);
        }
    } else {
        mpz_set (root, &r);
        if (rem)
            mpz_sub (rem, a, &tmp);
    }

    mpz_clear (&tmp);
    mpz_clear (&r);
}


void mpz_sqrt (root, a)
    MP_INT *root;
    MP_INT *a;
{
    mpz_sqrtrem (root, NULL, a);
}
