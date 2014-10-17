#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "bfi.tree.h"

#ifndef DISABLE_DYNASM
#include <stdint.h>
#include <fcntl.h>
#if !defined(LEGACYOS) && !defined(_WIN32)
#include <sys/mman.h>
#elif defined(_WIN32)
# include <windows.h>
#endif

#include "bfi.dasm.h"
#include "bfi.run.h"
#include "clock.h"

#include "dynasm/dasm_proto.h"

static int dump_code = 0;

int
checkarg_dynasm(char * opt, char * arg UNUSED)
{
    if (!strcmp(opt, "-dump")) {
	dump_code = 1;
	return 1;
    }
    return 0;
}

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_AMD64)
#define CPUID "x86_64"
#include "dynasm/dasm_x86.h"
#include "bfi.dasm.amd64.h"
#elif defined(__i386__) || defined(_M_IX86)
#define CPUID "i686"
#include "dynasm/dasm_x86.h"
#include "bfi.dasm.i686.h"
#else
#warning "Supported processor not detected for DYNASM."
int dynasm_ok = 0;

void
run_dynasm(void)
{
    fprintf(stderr,
	"Dynasm has not been configured for this processor and compiler.\n");
    exit(1);
}
#endif
#endif
