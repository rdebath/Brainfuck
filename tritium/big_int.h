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

