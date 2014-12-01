
#include <stdio.h>
#include "bfi.tree.h"
#include "bfi.version.h"

#define VS_Q(x) #x
#define VS_S(x) VS_Q(x)
#define VS(x) VS_S(VERSION_ ## x)
#ifdef GITHASH
#ifdef GITDECO
#define VERSION VS_S(GITDECO) " " VS_S(GITHASH) " " VS_S(GITTIME)
#else
#define VERSION VS(MAJOR) "." VS(MINOR) "." VS(BUILD) "? " \
		VS_S(GITHASH) " " VS_S(GITTIME)
#endif
#else
#define VERSION "v" VS(MAJOR) "." VS(MINOR) "." VS(BUILD) VERSION_SUFFIX
#endif

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_AMD64)
#define PROCESSOR	"x64"
#define BFI_FOUND_CPU_X86_64
#elif defined(__i386__) || defined(_M_IX86)
#define PROCESSOR	"i386"
#define BFI_FOUND_CPU_X86_32
#elif defined(__powerpc__) || defined(__PPC__)
#define PROCESSOR	"PPC"
#define BFI_FOUND_CPU_PPC
#elif defined(__sparc__)
#define PROCESSOR	"Sparc"
#define BFI_FOUND_CPU_SPARC
#elif defined(__arm__)
#ifdef __thumb2__
#define PROCESSOR	"ARMv7"	/* Approx, actually ARMv6t2 */
#elif __thumb__
#define PROCESSOR	"ARMv6"	/* Approx, actually ARMv5t */
#else
#define PROCESSOR	"ARM"
#endif
#define BFI_UNSUPPORTED_CPU_ARM
#elif defined(__MIPSEL__) || defined(__MIPSEB__)
#define PROCESSOR	"MIPS"
#define BFI_UNSUPPORTED_CPU_MIPS
#elif defined(__SH4__)
#define PROCESSOR	"SuperH-4"
#define BFI_UNSUPPORTED_CPU_SH4
#elif defined(__ia64__)
#define PROCESSOR	"ia64"
#define BFI_UNSUPPORTED_CPU_IA64
#elif defined(__alpha__)
#define PROCESSOR	"DEC Alpha"
#define BFI_UNSUPPORTED_CPU_ALPHA
#else
#define PROCESSOR	"using an unknown processor"
#endif

#ifdef __linux__
#define OSNAME	"Linux"
#elif defined(_WIN32)
#define OSNAME	"Windows"
#elif defined(__FreeBSD__)
#define OSNAME	"FreeBSD"
#elif defined(__NetBSD__)
#define OSNAME	"NetBSD"
#elif defined(__hpux__)
#define OSNAME	"HP-UX"
#elif defined(__osf__)
#define OSNAME	"DEC OSF/1"
#elif defined(__APPLE__)
#define OSNAME	"Apple"
#elif defined(__unix__)
#define OSNAME	"Unix"
#else
#define OSNAME	"an unknown OS"
#endif

#define HOST  OSNAME " " PROCESSOR

void
print_banner(FILE * fd, char const * program)
{
    fprintf(fd, "%s: Version "VERSION" on "HOST"\n", program);
}
