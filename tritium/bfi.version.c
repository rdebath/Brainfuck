
#include <stdio.h>

#ifdef CNF
#ifndef VERSION
#include "bfi.version.h"
#endif
#else
#define VERSION "Manual build"
#endif

#define VS_Q(x) #x
#define VS_S(x) VS_Q(x)
#ifndef VERSION
#define VS(x) VS_S(VERSION_ ## x)
#ifdef GITHASH
#ifdef GITDECO
#ifdef GITTIME
#define VERSION VS_S(GITDECO) " " VS_S(GITHASH) " " VS_S(GITTIME)
#else
#define VERSION VS_S(GITDECO) " " VS_S(GITHASH)
#endif
#else
#define VERSION VS(MAJOR) "." VS(MINOR) "." VS(BUILD) "? " \
		VS_S(GITHASH) " " VS_S(GITTIME)
#endif
#else
#define VERSION "v" VS(MAJOR) "." VS(MINOR) "." VS(BUILD) VERSION_SUFFIX
#endif
#endif

/* This list of Operating systems is all those I've actually tried it on. */
#ifdef __linux__
#define OSNAME	"Linux"
#elif defined(_WIN32)
#define OSNAME	"Windows"
#elif defined(__FreeBSD__)
#define OSNAME	"FreeBSD"
#elif defined(__OpenBSD__)
#define OSNAME	"OpenBSD"
#elif defined(__NetBSD__)
#define OSNAME	"NetBSD"
#elif defined(__hpux__)
#define OSNAME	"HP-UX"
#elif defined(__osf__)
#define OSNAME	"DEC OSF/1"
#elif defined(__APPLE__)
#define OSNAME	"Apple"
#elif defined(__EMSCRIPTEN__)
#define OSNAME	"Emscripten"
#elif defined(__ultrix__)
#define OSNAME	"Ultrix"
#elif defined(__unix__)
#define OSNAME	"Unix"
#else
#define OSNAME	"an unknown OS"
#endif

/* And here's the CPUs those OSs were running on */
#if defined(__x86_64__) || defined(__amd64__) || defined(_M_AMD64)
#ifndef __ILP32__
#define PROCESSOR	"x86_64"
#else
#define PROCESSOR	"x86_32"
#endif
#if defined(__ELF__) || defined(_WIN32) || defined(__APPLE__)
#define BFI_FOUND_CPU_X86_64
#endif
#elif defined(__i386__) || defined(_M_IX86)
#define PROCESSOR	"i386"
#if defined(__ELF__) || defined(__APPLE__)
#define BFI_FOUND_CPU_X86_32
#endif
#if defined(_WIN32)
#define BFI_FOUND_CPU_X86_32_WINDOWS
#endif
#elif defined(__PPC64__)
#define PROCESSOR	"PPC64"
#elif (defined(__powerpc__) || defined(__PPC__)) && defined(__BIG_ENDIAN__)
#define PROCESSOR	"PPC"
#ifdef __ELF__
#define BFI_FOUND_CPU_PPC
#endif
#elif defined(__powerpc__) || defined(__PPC__)
#define PROCESSOR	"PPC"
#elif defined(__sparc__)
#define PROCESSOR	"Sparc"
#ifdef __ELF__
#define BFI_FOUND_CPU_SPARC
#endif
#elif defined(__arm__)
#ifdef __thumb2__
#define PROCESSOR	"ARMv7"	/* Approx, actually ARMv6t2 */
#elif __thumb__
#define PROCESSOR	"ARMv6"	/* Approx, actually ARMv5t */
#else
#define PROCESSOR	"ARM"
#endif
#elif defined(__AARCH64EL__)
#define PROCESSOR	"ARM64"
#elif defined(__MIPSEL__) || defined(__MIPSEB__)
#define PROCESSOR	"MIPS"
#elif defined(__SH4__)
#define PROCESSOR	"SuperH-4"
#elif defined(__ia64__)
#define PROCESSOR	"ia64"
#elif defined(__alpha__) || defined(__alpha)
#define PROCESSOR	"DEC Alpha"
#elif defined(__vax__)
#define PROCESSOR	"VAX"
#elif defined(__s390x__)
#define PROCESSOR	"s390x"
#elif defined(__asmjs__)
#define PROCESSOR	"using asm.js"
#else
#define PROCESSOR	"?"
#endif

/* This is what I've spotted this time. */
#define HOST  OSNAME " " PROCESSOR

void
print_banner(FILE * fd, char const * program)
{
    fprintf(fd, "%s: Version "VERSION" on "HOST"\n", program);
}
