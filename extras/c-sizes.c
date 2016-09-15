/* First some definitions to fix brain dead defaults */

/* Make printf work correctly for the C standards */
#if (defined(_WIN32) && defined(__MINGW32__) ) && !defined(__USE_MINGW_ANSI_STDIO)
/*# define printf __mingw_printf */
#define __USE_MINGW_ANSI_STDIO 1
#endif

/* The mess that is "inline" */
#if defined(__STRICT_ANSI__) || !defined(__STDC_VERSION__)
#define inline
#endif

/* Make glibc expose everything. */
#define _GNU_SOURCE 1

/******************************************************************************/

#include <stdio.h>
#include <time.h>

#if defined(__STDC__) || defined(L_cuserid)
#include <limits.h>
#endif

#ifdef __STDC__
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <float.h>
#if _POSIX_VERSION >= 199506L || defined(LLONG_MAX)
#include <inttypes.h>
#endif
#endif

#ifndef PRIdLLONG
/* These are mostly for Win32, but probably still a good idea elsewhere. */
#if defined(LLONG_MAX) && defined(INT64_MAX) && LLONG_MAX == INT64_MAX
#define PRIdLLONG PRId64
#else
#define PRIdLLONG "lld"
#endif

#if defined(ULLONG_MAX) && defined(UINT64_MAX) && ULLONG_MAX == UINT64_MAX
#define PRIuLLONG PRIu64
#else
#define PRIuLLONG "llu"
#endif
#endif

/*
   # include <inttypes.h>

    intmax_t strtoimax(const char *nptr, char **endptr, int base);
    uintmax_t strtoumax(const char *nptr, char **endptr, int base);
    imaxdiv_t imaxdiv(intmax_t numerator, intmax_t denominator);
    intmax_t imaxabs(intmax_t j);

    intmax_t wcstoimax(const wchar_t *nptr, wchar_t **endptr, int base);
    uintmax_t wcstoumax(const wchar_t *nptr, wchar_t **endptr, int base);

*/

/* Guess alignment of structures */
struct alignchar { char a; char b;};
struct alignshort { short a; char b; };
struct alignint { int a; char b; };
struct alignlong { long a; char b; };
struct aligndouble { double a; char b; };

#ifdef __STDC__
void check_weird(void);
#endif

#ifdef __STDC__
static inline int
is_big_endian(void)
#else
int is_big_endian()
#endif
{
    union {
	long int l;
	char    c[sizeof(long int)];
    } u;

    u.l = 42;

    if (u.c[sizeof(long int) - 1] == 42 && u.c[0] == 42)
	return 2;
    else if (u.c[sizeof(long int) - 1] == 42)
	return 1;
    else if (u.c[0] == 42)
	return 0;
    else
	return 3;
}

#ifdef __STDC__
int
#ifdef BROKEN_MAIN
main(int argc, char ** argv)
#else
main(void)
#endif

#else
main()
#endif
{
    int e = is_big_endian();

    if (e == 2)
	printf("NOT ENDIAN");
    else if (e > 2)
	printf("WEIRD ENDIAN");
    else if (e)
	printf("BIG ENDIAN");
    else
	printf("LITTLE ENDIAN");

    printf(" C%d:S%d:I%d:L%d P%d Struct-[%d,%d,%d,%d,%d]\n",
	(int)sizeof(char), (int)sizeof(short), (int)sizeof(int),
	(int)sizeof(long), (int)sizeof(char*),
	(int)(sizeof(struct alignchar)-sizeof(char)),
	(int)(sizeof(struct alignshort)-sizeof(short)),
	(int)(sizeof(struct alignint)-sizeof(int)),
	(int)(sizeof(struct alignlong)-sizeof(long)),
	(int)(sizeof(struct aligndouble)-sizeof(double)) );

    if ( !(~1==-2) )
	printf("WARNING: This is not a twos-complement machine\n\n");

    check_weird();

#ifdef __STDC_VERSION__
# if __STDC_VERSION__ == 199409L
    printf("Macro __STDC_VERSION__     %ldL C89/C90 ISO/IEC 9899:1990\n", (long) __STDC_VERSION__);
    printf("%34s Plus amendment 1 ISO/IEC 9899-1:1994\n","");
# elif __STDC_VERSION__ == 199901L
    printf("Macro __STDC_VERSION__     %ldL C99 ISO/IEC 9899:1999\n", (long) __STDC_VERSION__);
# elif __STDC_VERSION__ == 201112L
    printf("Macro __STDC_VERSION__     %ldL C11 ISO/IEC 9899:2011\n", (long) __STDC_VERSION__);
# elif __STDC_VERSION__ > 201112L
    printf("Macro __STDC_VERSION__     %ldL recent version\n", (long) __STDC_VERSION__);
# else
    printf("Macro __STDC_VERSION__     %ldL C89/C90 ISO/IEC 9899:1990\n", (long) __STDC_VERSION__);
# endif
#else
#ifdef __STDC__
    printf("Macro __STDC__             %6ldL C89/C90 ANSI X3.159-1989\n", (long) __STDC__);
#else
    printf("__STDC__ NOT defined       NOT ANSI C\n");
#endif
#endif

#if defined(__STDC__) && defined(__STRICT_ANSI__)
    printf("Macro __STRICT_ANSI__      %-6d  This may turn off items in libc\n", (int) __STRICT_ANSI__);
#endif

#ifdef __cplusplus
# if __cplusplus == 199711L
    printf("Macro __cplusplus          %dL C++ ISO/IEC 14882:1998\n", (int) __cplusplus);
# elif __cplusplus == 201103L
    printf("Macro __cplusplus          %dL C++ ISO/IEC 14882:2011\n", (int) __cplusplus);
# elif __cplusplus == 201402L
    printf("Macro __cplusplus          %dL C++ ISO/IEC 14882:2014\n", (int) __cplusplus);
# elif __cplusplus/100*100 == __cplusplus
    printf("Macro __cplusplus          %dL C++ Draft implementation\n", (int) __cplusplus);
# else
    printf("Macro __cplusplus          %dL C++\n", (int) __cplusplus);
# endif
#endif

#ifdef _POSIX_VERSION
# if _POSIX_VERSION == 198808L
    printf("Macro _POSIX_VERSION       %ldL POSIX.1-1988 (first release)\n", (unsigned long) _POSIX_VERSION);
# elif _POSIX_VERSION == 199009L
    printf("Macro _POSIX_VERSION       %ldL POSIX.1-1990\n", (unsigned long) _POSIX_VERSION);
# elif _POSIX_VERSION == 199309L
    printf("Macro _POSIX_VERSION       %ldL POSIX.1b-1993\n", (unsigned long) _POSIX_VERSION);
# elif _POSIX_VERSION == 199506L
    printf("Macro _POSIX_VERSION       %ldL POSIX.1c-1996\n", (unsigned long) _POSIX_VERSION);
# elif _POSIX_VERSION == 200112L
    printf("Macro _POSIX_VERSION       %ldL POSIX.1-2001 (OG Issue 6)\n", (unsigned long) _POSIX_VERSION);
# elif _POSIX_VERSION == 200809L
    printf("Macro _POSIX_VERSION       %ldL POSIX.1-2008 (OG Issue 7)\n", (unsigned long) _POSIX_VERSION);
# elif _POSIX_VERSION > 200809L
    printf("Macro _POSIX_VERSION       %ldL recent version\n", (unsigned long) _POSIX_VERSION);
# else
    printf("Macro _POSIX_VERSION       %ldL BAD VALUE\n", (unsigned long) _POSIX_VERSION);
# endif
#else
    printf("Macro _POSIX_VERSION not defined!\n");
#endif

#ifdef _POSIX2_VERSION
    printf("Macro _POSIX2_VERSION      %ld\n", (unsigned long) _POSIX2_VERSION);
#endif

#ifdef _XOPEN_VERSION
# if _XOPEN_VERSION == 3
    printf("Macro _XOPEN_VERSION       %6ld  X/Open Portability Guide 3 (1989)\n", (unsigned long) _XOPEN_VERSION);
# elif _XOPEN_VERSION == 4
#  ifndef _XOPEN_SOURCE_EXTENDED
    printf("Macro _XOPEN_VERSION       %6ld  X/Open Portability Guide 4 (1992)\n", (unsigned long) _XOPEN_VERSION);
#  else
    printf("Macro _XOPEN_VERSION/EXT   %6ld  X/Open Single UNIX Specification (1995)\n", (unsigned long) _XOPEN_VERSION);
#  endif
# elif _XOPEN_VERSION == 500
    printf("Macro _XOPEN_VERSION       %6ld  SUS Version 2 1997\n", (unsigned long) _XOPEN_VERSION);
# elif _XOPEN_VERSION == 600
    printf("Macro _XOPEN_VERSION       %6ld  POSIX.1-2001 (OG Issue 6)\n", (unsigned long) _XOPEN_VERSION);
# elif _XOPEN_VERSION == 700
    printf("Macro _XOPEN_VERSION       %6ld  POSIX.1-2008 (OG Issue 7)\n", (unsigned long) _XOPEN_VERSION);
# else
    printf("Macro _XOPEN_VERSION       %6ld\n", (unsigned long) _XOPEN_VERSION);
# endif
#else
    printf("Macro _XOPEN_VERSION not defined!\n");
#endif

#if defined(CHAR_BIT) || defined(WORD_BIT) || defined(LONG_BIT)
    {
	char txtbuf[256];
	txtbuf[0] = 0;

#ifdef CHAR_BIT
	sprintf(txtbuf+strlen(txtbuf), " %d", CHAR_BIT);
#else
	strcat(txtbuf, " N/A");
#endif
#ifdef WORD_BIT
	sprintf(txtbuf+strlen(txtbuf), " %d", WORD_BIT);
#else
	strcat(txtbuf, " N/A");
#endif
#ifdef LONG_BIT
	sprintf(txtbuf+strlen(txtbuf), " %d", LONG_BIT);
#else
	strcat(txtbuf, " N/A");
#endif
	printf("Bits for CHAR/WORD/LONG   %s\n", txtbuf);

    }
#endif

    if (sizeof(void *) == sizeof(int)) {
	printf("Bytes PT%2d, Pointer        int pointer\n",
	       (int)sizeof(void *));
    } else if (sizeof(void *) == sizeof(long)) {
	printf("Bytes PT%2d, Pointer <***** long pointer\n",
	       (int)sizeof(void *));
    } else if (sizeof(void *) >= sizeof(long)) {
	printf("Bytes PT%2d, Pointer <***** HUGE pointer\n",
	       (int)sizeof(void *));
    } else {
	printf("Bytes PT%2d, Pointer <***** weird pointer\n",
	       (int)sizeof(void *));
    }
#ifdef UINTPTR_MAX
    printf("Bytes PI%2d, uintptr_t      %"PRIuPTR"\n", (int)sizeof(uintptr_t), UINTPTR_MAX);
#endif
    printf("Bytes SZ%2d, sizeof()\n", (int)sizeof(sizeof(int)));
    printf("\n");

#if defined(CHAR_BIT) && defined(CHAR_MIN) && defined(CHAR_MAX)
    if (CHAR_BIT == 8 && ((unsigned char)(3<<7)==0x80)) {
	printf("Bytes C %2d, char           %d..%d in eight bits\n",
		(int)sizeof(char), (int)CHAR_MIN, (int)CHAR_MAX);
    } else {
	printf("Bytes C %2d, char           %.0f..%.0f in %d bits\n",
		(int)sizeof(char), (double)CHAR_MIN, (double)CHAR_MAX, CHAR_BIT);
    }
#endif

#if defined(SCHAR_MIN) && defined(SCHAR_MAX) && defined(__STDC__)
    printf("Bytes SC%2d, signed char    %d..%d\n",
	    (int)sizeof(signed char), (int)SCHAR_MIN, (int)SCHAR_MAX);
#endif

#ifndef UCHAR_MAX
#define UCHAR_MAX   ((unsigned char)-1)
#endif
#if defined(UCHAR_MAX)
    printf("Bytes UC%2d, unsigned char  %d..%d\n",
	    (int)sizeof(unsigned char), 0, (int)UCHAR_MAX);
#ifdef CHAR_BIT
    if (UCHAR_MAX != (((unsigned int)1)<<CHAR_BIT)-1)
	printf("***********> NON CONFORMING IMPLEMENTATION\n");
#endif
#endif

#if defined(SHRT_MAX)
    printf("Bytes SI%2d, short int      %d\n",
	    (int)sizeof(short int), (int)SHRT_MAX);
#else
    printf("Bytes SI%2d, short int      %d\n",
	    (int)sizeof(int), (int)(((unsigned short)-1)>>1));	/*TWOS*/
#endif
#if defined(INT_MAX)
    printf("Bytes I %2d, int            %d%s\n",
	    (int)sizeof(int), INT_MAX,
	    (sizeof(int)==sizeof(time_t)?" (time_t)":"")
	    );
#else
    printf("Bytes I %2d, int            %d%s\n",
	    (int)sizeof(int), ((unsigned)-1)>>1,		/*TWOS*/
	    (sizeof(int)==sizeof(time_t)?" (time_t)":"")
	    );
#endif

#if defined(LONG_MAX)
    printf("Bytes LI%2d, long int       %ld%s%s\n",
	    (int)sizeof(long int), LONG_MAX,
	    (sizeof(long)==sizeof(time_t) &&
	     sizeof(int)!=sizeof(time_t) ?" (time_t)":""), ""
#if defined(LLONG_MAX) && LLONG_MAX == LONG_MAX
	    " (long long)"
#define GOT_LLONG
#endif
#if defined(INT64_MAX) && INT64_MAX == LONG_MAX
	    " (int64_t)"
#define GOT_INT64
#endif
#if defined(INTMAX_MAX) && INTMAX_MAX == LONG_MAX
	    " (intmax_t)"
#define GOT_INTMAX
#endif
	    );
#else
    printf("Bytes LI%2d, long int       %ld\n",
	    (int)sizeof(long int), ((unsigned long)-1L)>>1);	/*TWOS*/
#endif

#if defined(LLONG_MAX) && !defined(GOT_LLONG)
#define GOT_LLONG
    printf("Bytes LL%2d, long long      %"PRIdLLONG"%s%s\n",
	    (int)sizeof(long long), LLONG_MAX,
	    (sizeof(long long)==sizeof(time_t)?" (time_t)":""), ""
#if defined(INT64_MAX) && INT64_MAX == LLONG_MAX
	    " (int64_t)"
#define GOT_INT64
#endif
#if defined(INTMAX_MAX) && INTMAX_MAX == LLONG_MAX
	    " (intmax_t)"
#define GOT_INTMAX
#endif
	    );
#endif

#if defined(LONG_LONG_MAX) && !defined(GOT_LLONG)
    printf("Bytes LL%2d, long long      %llu (non-POSIX)\n",
	    (int)sizeof(long long), LONG_LONG_MAX);
#endif

#if defined(INT64_MAX) && !defined(GOT_INT64)
    printf("Bytes I*%2d, int64_t        %"PRId64"\n", (int)sizeof(int64_t), INT64_MAX);
#endif

#if defined(INT128_MAX) && !defined(GOT_INT128)
    printf("Bytes I*%2d, int128_t       %"PRId128"\n", (int)sizeof(int128_t), INT128_MAX);
#endif

#if defined(INTMAX_MAX) && !defined(GOT_INTMAX)
    printf("Bytes IM%2d, intmax_t       %"PRIdMAX"\n", (int)sizeof(intmax_t), INTMAX_MAX);
#endif

#if defined(__SIZEOF_INT128__) && defined(INTMAX_MAX)
    if (sizeof(__int128) > sizeof(intmax_t))
	printf("Bytes I*%2d, __int128 <**** Larger than intmax_t\n", (int)sizeof(__int128));
#endif

    printf("\n");

#if defined(USHRT_MAX)
    printf("Bytes US%2d, unsigned short %u\n", (int)sizeof(unsigned short), USHRT_MAX);
#endif
#if defined(UINT_MAX)
    printf("Bytes UI%2d, unsigned int   %u\n", (int)sizeof(unsigned int), UINT_MAX);
#else
    {
	int int_bit, int_min = 0, int_max = 0;
	unsigned x1 = 0, x2 = 0, x3 = 1;
	for (int_bit=0; int_bit<256; int_bit++) {
	    x1 = x2;
	    x2 = x2 * 2 + 1;
	    x3 = (x3<<1);
	    if (x2 == x1 || (x2+1) == x2) break;

	    if (int_min > x2) int_min = x2;
	    if (int_min > x3) int_min = x3;
	    if (int_max < x2) int_max = x2;
	}
	printf("Bytes UI%2d, unsigned int   %u..%u\n",
		(int)sizeof(unsigned int), int_min, int_max);
    }
#endif
#if defined(ULONG_MAX)
    printf("Bytes UL%2d, unsigned long  %lu\n", (int)sizeof(unsigned long), ULONG_MAX);
#else
    printf("Bytes UL%2d, unsigned long  %lu\n", (int)sizeof(unsigned long), (unsigned long)-1L);	/*STDC*/
#endif
#if defined(ULLONG_MAX)
#if !defined(ULONG_MAX) || ULONG_MAX != ULLONG_MAX
#if !defined(UINT64_MAX) || UINT64_MAX != ULLONG_MAX
    printf("Bytes LL%2d, unsigned long l%"PRIuLLONG"\n", (int)sizeof(unsigned long long), ULLONG_MAX);
#endif
#endif
#endif
#if defined(UINT64_MAX)
    printf("Bytes U*%2d, uint64_t       %"PRIu64"%s\n",
	    (int)sizeof(uint64_t), UINT64_MAX, ""
#if defined(ULLONG_MAX) && UINT64_MAX == ULLONG_MAX
	    " (unsigned long long)"
#endif
	    );
#endif
#if defined(UINTMAX_MAX)
    printf("Bytes UM%2d, uintmax_t      %"PRIuMAX"\n", (int)sizeof(uintmax_t), UINTMAX_MAX);
#endif

    printf("\n");

#if defined(FLT_MAX)
    printf("Bytes FL%2d, float          %g\n", (int)sizeof(float), FLT_MAX);
#else
    printf("Bytes FL%2d, float          ?\n", (int)sizeof(float));
#endif
#if defined(DBL_MAX)
    printf("Bytes DB%2d, double         %g\n", (int)sizeof(double), DBL_MAX);
#else
    printf("Bytes DB%2d, double         ?\n", (int)sizeof(double));
#endif

#if !defined(_WIN32) || defined(__MINGW32__)
#if defined(LDBL_MAX) && defined(LDBL_MAX_EXP) && defined(DBL_MAX_EXP) && LDBL_MAX_EXP != DBL_MAX_EXP
    printf("Bytes LD%2d, long double    %Lg\n", (int)sizeof(long double), LDBL_MAX);
#endif
#endif

    return 0;
}

#ifdef __STDC__
void check_weird(void)
#else
check_weird()
#endif
{
    int known_weird = 0;
#if !defined(CHAR_BIT) || !defined(CHAR_MIN) || !defined(CHAR_MAX)
    int char_bit, char_min = 0, char_max = 0;

#if !defined(CHAR_BIT)
#define CHAR_BIT    char_bit
#endif
#if !defined(CHAR_MIN)
#define CHAR_MIN    char_min
#endif
#if !defined(CHAR_MAX)
#define CHAR_MAX    char_max
#endif

    {
	char x1 = 0, x2 = 0, x3 = 1;
	for (char_bit=0; char_bit<256; char_bit++) {
	    x1 = x2;
	    x2 = x2 * 2 + 1;
	    x3 = (x3<<1);
	    if (x2 == x1 || (x2+1) == x2) break;

	    if (char_min > x2) char_min = x2;
	    if (char_min > x3) char_min = x3;
	    if (char_max < x2) char_max = x2;
	}
    }
#endif

    if (!known_weird) {
        int x, flg = sizeof(int) * CHAR_BIT +2;
        for(x=1;x+1>x && --flg;x=(x|(x<<1)));
	x = sizeof(int) * CHAR_BIT +3 - flg;

	known_weird = 1;
        if (x >= (int)(sizeof(int) * CHAR_BIT))
            printf("WARNING(A): Signed integers are insane (too big for their bit size).\n");
	else if (x != sizeof(int) * CHAR_BIT -1)
	    printf("WARNING(A): Signed integers have %d magnitude bits.\n", x);
	else
	    known_weird = 0;
    }

    if (!known_weird) {
        int x, y=0, flg = sizeof(int) * CHAR_BIT * 2;
        for(x=1;x>y && --flg;y=x,x=(x<<1));
	x = sizeof(int) * CHAR_BIT * 2  - flg;

	known_weird = 1;
        if (x >= (int)(sizeof(int) * CHAR_BIT))
            printf("WARNING(B): Signed integers are insane (too big for their bit size).\n");
	else if (x != sizeof(int) * CHAR_BIT -1)
	    printf("WARNING(B): Signed integers have %d magnitude bits.\n", x);
	else
	    known_weird = 0;
    }

    if (!known_weird) {
        int x, flg = sizeof(int) * CHAR_BIT +2;
        for(x=1;x+2>=x && --flg;x=(x|(x<<1)));
	x = sizeof(int) * CHAR_BIT +3 - flg;

	known_weird = 1;
        if (x >= (int)(sizeof(int) * CHAR_BIT))
            printf("WARNING(C): Signed integers are insane (too big for their bit size).\n");
	else if (x != sizeof(int) * CHAR_BIT -1)
	    printf("WARNING(C): Signed integers have %d magnitude bits.\n", x);
	else
	    known_weird = 0;
    }
}
