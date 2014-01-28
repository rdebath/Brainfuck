
#include <stdio.h>
#include "bfi.tree.h"
#include "bfi.version.h"

void
print_banner(FILE * fd, char const * program)
{
    fprintf(fd, "%s: Version "VERSION", date "VERSION_DATE"\n", program);
}
