#ifdef __STDC__
#include <limits.h>
/* LLONG_MAX came in after inttypes.h, limits.h is very old. */
#if _POSIX_VERSION >= 199506L || defined(LLONG_MAX)
#include <inttypes.h>
#endif
#endif

#ifndef uintbig_t
#ifdef __SIZEOF_INT128__
#define uintbig_t unsigned __int128
#else
#ifdef _UINT128_T
#define uintbig_t __uint128_t
#else
#if defined(ULLONG_MAX) || defined(__LONG_LONG_MAX__)
#define uintbig_t unsigned long long
#else
#if defined(UINTMAX_MAX)
#define uintbig_t uintmax_t
#else
#define uintbig_t unsigned long
#endif
#endif
#endif
#endif
#endif

#define UINTBIG_BIT ((int)(CHAR_BIT*sizeof(uintbig_t)))
