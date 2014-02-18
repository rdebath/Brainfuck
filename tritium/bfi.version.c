
#include <stdio.h>
#include "bfi.tree.h"
#include "bfi.version.h"

#define VERSION_MAJOR	0
#define VERSION_MINOR	10
/* Commit number for last MINOR build */
#define MINOR_BUILD	129

#define VS_Q(x) #x
#define VS_S(x) VS_Q(x)
#define VS(x) VS_S(VERSION_ ## x)
#define VERSION VS(MAJOR) "." VS(MINOR)

void
print_banner(FILE * fd, char const * program)
{
    fprintf(fd, "%s: Version "VERSION".%d%s, date "BUILD_DATE"\n",
	    program, VERSION_BUILD-MINOR_BUILD, VERSION_SUFFIX);
}
