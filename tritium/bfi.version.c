
#include <stdio.h>
#include "bfi.tree.h"
#include "bfi.version.h"

#define VS_Q(x) #x
#define VS_S(x) VS_Q(x)
#define VS(x) VS_S(VERSION_ ## x)
#define VERSION VS(MAJOR) "." VS(MINOR) "." VS(BUILD) VERSION_SUFFIX

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_AMD64)
#define PROCESSOR	" x64"
#elif defined(__i386__) || defined(_M_IX86)
#define PROCESSOR	" i386"
#elif defined(__powerpc__) || defined(__PPC__)
#define PROCESSOR	" PPC"
#elif defined(__sparc__)
#define PROCESSOR	" Sparc"
#else
#define PROCESSOR	/*Unknown, no special code in this application.*/
#endif

#ifdef __linux__
#define HOST	"Linux"PROCESSOR
#elif defined(_WIN32)
#define HOST	"Windows"PROCESSOR
#elif defined(__FreeBSD__)
#define HOST	"FreeBSD"PROCESSOR
#elif defined(__NetBSD__)
#define HOST	"NetBSD"PROCESSOR
#elif defined(__unix__)
#define HOST	"Unix"PROCESSOR
#else
#define HOST	"an unknown OS"
#endif

void
print_banner(FILE * fd, char const * program)
{
    fprintf(fd, "%s: Version "VERSION" on "HOST"\n", program);
}
