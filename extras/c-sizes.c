#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include <stdint.h>
#include <inttypes.h>

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

/* Make printf work correctly for the C standards */
/* See also: -D__USE_MINGW_ANSI_STDIO */
#if (defined(_WIN32) && defined(__MINGW32__) && defined(__SIZEOF_LONG_DOUBLE__) ) && !defined(__USE_MINGW_ANSI_STDIO)
#define printf __mingw_printf
#endif

/*
    #include <inttypes.h>

    intmax_t strtoimax(const char *nptr, char **endptr, int base);
    uintmax_t strtoumax(const char *nptr, char **endptr, int base);
    imaxdiv_t imaxdiv(intmax_t numerator, intmax_t denominator);
    intmax_t imaxabs(intmax_t j);

    intmax_t wcstoimax(const wchar_t *nptr, wchar_t **endptr, int base);
    uintmax_t wcstoumax(const wchar_t *nptr, wchar_t **endptr, int base);

*/

static inline int
is_big_endian(void)
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

int
main(int argc, char ** argv)
{
    int e = is_big_endian();
#if !defined(CHAR_BIT) || !defined(CHAR_MIN) || !defined(CHAR_MAX)
    int char_bit, char_min = 0, char_max = 0;
#endif

    if (e == 2)
	printf("NOT ENDIAN");
    else if (e > 2)
	printf("WEIRD ENDIAN");
    else if (e)
	printf("BIG ENDIAN");
    else
	printf("LITTLE ENDIAN");

    printf(" %d:%d:%d:%d %d\n",
	(int)sizeof(char), (int)sizeof(short), (int)sizeof(int),
	(int)sizeof(long), (int)sizeof(char*));

    if ( !(~1==-2) )
	printf("WARNING: This is not a twos-complement machine\n\n");

    if (sizeof(void *) == sizeof(int)) {
	printf("Bytes PT%2d, Pointer        int pointer\n",
	       (int)sizeof(void *));
    } else if (sizeof(void *) == sizeof(long)) {
	printf("Bytes PT%2d, Pointer <***** long pointer\n",
	       (int)sizeof(void *));
    } else if (sizeof(void *) >= sizeof(long)) {
	printf("Bytes PT%2d, Pointer <***** HUGE MODEL pointer\n",
	       (int)sizeof(void *));
    } else {
	printf("Bytes PT%2d, Pointer <***** WTF pointer\n",
	       (int)sizeof(void *));
    }
#ifdef UINTPTR_MAX
    printf("Bytes PI%2d, uintptr_t      %"PRIuPTR"\n",
	    (int)sizeof(uintptr_t), UINTPTR_MAX);
#endif
    printf("Bytes SZ%2d, sizeof()\n", (int)sizeof(sizeof(int)));
    printf("\n");

#if !defined(CHAR_BIT) || !defined(CHAR_MIN) || !defined(CHAR_MAX)
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

#if defined(CHAR_BIT) && defined(CHAR_MIN) && defined(CHAR_MAX)
    if (CHAR_BIT == 8 && ((unsigned char)(3<<7)==0x80)) {
	printf("Bytes C %2d, char           %d..%d in eight bits\n",
		(int)sizeof(char), (int)CHAR_MIN, (int)CHAR_MAX);
    } else {
	printf("Bytes C %2d, char           %.0f..%.0f in %d bits\n",
		(int)sizeof(char), (double)CHAR_MIN, (double)CHAR_MAX, CHAR_BIT);
    }
#endif

#if defined(SCHAR_MIN) && defined(SCHAR_MAX)
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
    if (UCHAR_MAX != (1U<<CHAR_BIT)-1)
	printf("***********> NON CONFORMING IMPLEMENTATION\n");
#endif
#endif

#if defined(SHRT_MAX)
    printf("Bytes SI%2d, short int      %d\n",
	    (int)sizeof(short int), (int)SHRT_MAX);
#endif
#if defined(INT_MAX)
    printf("Bytes I %2d, int            %d\n",
	    (int)sizeof(int), INT_MAX);
#else
    printf("Bytes I %2d, int            %d\n",
	    (int)sizeof(int), ((unsigned)-1)>>1 );		/*TWOS*/
#endif
#if defined(LONG_MAX)
    printf("Bytes LI%2d, long int       %ld\n",
	    (int)sizeof(long int), LONG_MAX);
#else
    printf("Bytes LI%2d, long int       %ld\n",
	    (int)sizeof(long int), ((unsigned long)-1L)>>1);	/*TWOS*/
#endif

    printf("\n");

#ifdef LLONG_MAX
#if !defined(LONG_MAX) || LONG_MAX != LLONG_MAX
#if !defined(INT64_MAX) || INT64_MAX != LLONG_MAX
    printf("Bytes LL%2d, long long      %"PRIdLLONG"\n",
	    (int)sizeof(long long), LLONG_MAX);
#endif
#endif
#endif
#ifdef INT64_MAX
    printf("Bytes I*%2d, int64_t        %"PRId64"%s\n",
	    (int)sizeof(int64_t), INT64_MAX, ""
#if defined(LLONG_MAX) && INT64_MAX == LLONG_MAX
	    " (long long)"
#endif
	    );
#endif
#ifdef INT128_MAX
    printf("Bytes I*%2d, int128_t       %"PRId128"\n",
	    (int)sizeof(int128_t), INT128_MAX);
#endif
#ifdef INTMAX_MAX
    printf("Bytes IM%2d, intmax_t       %"PRIdMAX"\n",
	    (int)sizeof(intmax_t), INTMAX_MAX);
#ifdef __SIZEOF_INT128__
    if (sizeof(__int128) > sizeof(intmax_t))
	printf("Bytes I*%2d, __int128 <**** Larger than intmax_t\n",
	    (int)sizeof(__int128));
#endif
#endif

#if defined(INTMAX_MAX)
    printf("\n");
#endif

#if defined(USHRT_MAX)
    printf("Bytes US%2d, unsigned short %u\n",
	    (int)sizeof(unsigned short), USHRT_MAX);
#endif
#if defined(UINT_MAX)
    printf("Bytes UI%2d, unsigned int   %u\n",
	    (int)sizeof(unsigned int), UINT_MAX);
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
    printf("Bytes UL%2d, unsigned long  %lu\n",
	    (int)sizeof(unsigned long), ULONG_MAX);
#else
    printf("Bytes UL%2d, unsigned long  %lu\n",
	    (int)sizeof(unsigned long), (unsigned long)-1L);	/*STDC*/
#endif
#if defined(ULLONG_MAX)
#if !defined(ULONG_MAX) || ULONG_MAX != ULLONG_MAX
#if !defined(UINT64_MAX) || UINT64_MAX != ULLONG_MAX
    printf("Bytes LL%2d, unsigned long l%"PRIuLLONG"\n",
	    (int)sizeof(unsigned long long), ULLONG_MAX);
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
    printf("Bytes UM%2d, uintmax_t      %"PRIuMAX"\n",
	    (int)sizeof(uintmax_t), UINTMAX_MAX);
#endif

#if defined(UINT_MAX)
    printf("\n");
#endif

#if defined(FLT_MAX)
    printf("Bytes FL%2d, float         %g\n",
	    (int)sizeof(float), FLT_MAX);
#endif
#if defined(DBL_MAX)
    printf("Bytes DB%2d, double        %g\n",
	    (int)sizeof(double), DBL_MAX);
#endif

#if !defined(_WIN32) || defined(__MINGW32__)
#if defined(LDBL_MAX) && defined(LDBL_MAX_EXP) && defined(DBL_MAX_EXP) && LDBL_MAX_EXP != DBL_MAX_EXP
    printf("Bytes LD%2d, long double   %Lg\n",
	    (int)sizeof(long double), LDBL_MAX);
#endif
#endif

    return 0;
}
