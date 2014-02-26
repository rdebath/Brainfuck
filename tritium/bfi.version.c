
#include <stdio.h>
#include "bfi.tree.h"
#include "bfi.version.h"

#define VS_Q(x) #x
#define VS_S(x) VS_Q(x)
#define VS(x) VS_S(VERSION_ ## x)
#define VERSION VS(MAJOR) "." VS(MINOR) "." VS(BUILD) VERSION_SUFFIX

void
print_banner(FILE * fd, char const * program)
{
    fprintf(fd, "%s: Version "VERSION"\n", program);
}
