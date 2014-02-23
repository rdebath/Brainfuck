#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/mman.h>

#include "bf2any.h"

/*
 * dasm64 translation from BF, runs at about 5,000,000,000 instructions per second.
 */

#include "dynasm/dasm_proto.h"

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_AMD64)
#include "dynasm/dasm_x86.h"
#include "bf2jit.amd64.h"
#elif defined(__i386__) || defined(_M_IX86)
#include "dynasm/dasm_x86.h"
#include "bf2jit.i686.h"
#else
#warning "Supported processor not detected."

int check_arg(const char * arg)
{
    fprintf(stderr, "Build failed: Supported processor not detected.\n");
    exit(255);
    return 0;
}
void outcmd(int ch, int count) { }
#endif

