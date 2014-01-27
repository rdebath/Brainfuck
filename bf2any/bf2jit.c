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

#include "../tools/dynasm/dasm_proto.h"

void link_and_run(dasm_State **state);
int tape_step = sizeof(int);

#if defined(__amd64__) || defined(_M_AMD64)
#include "../tools/dynasm/dasm_x86.h"
#include "bf2jit.amd64.h"
#elif defined(__i386__) || defined(_M_IX86)
#include "../tools/dynasm/dasm_x86.h"
#include "bf2jit.i686.h"
#endif

#ifdef DASM_S_OK

typedef int (*fnptr)(char* memory);
fnptr code = 0;
size_t codelen;
void * tapemem;

int
check_arg(char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    return 0;
}

void
link_and_run(dasm_State ** state)
{
    size_t  size;
    int     dasm_status = dasm_link(state, &size);
    assert(dasm_status == DASM_S_OK);

    char   *codeptr =
	(char *) mmap(NULL, size,
		      PROT_READ | PROT_WRITE | PROT_EXEC,
		      MAP_ANON | MAP_PRIVATE, -1, 0);
    assert(codeptr != MAP_FAILED);
    codelen = size;

    dasm_encode(state, codeptr);
    dasm_free(state);

    assert(mprotect(codeptr, size, PROT_EXEC | PROT_READ) == 0);

#if 0
    /* Write generated machine code to a temporary file.
    // View with:
    //  objdump -D -b binary -mi386 -Mx86,intel code.bin
    // or
    //  ndisasm -b32 code.bin
    */
    FILE   *f = fopen("/tmp/code.bin", "w");
    fwrite(codeptr, size, 1, f);
    fclose(f);
#endif

    if (isatty(STDOUT_FILENO)) setbuf(stdout, 0);
    tapemem = calloc(32768, tape_step);
    code = (void *) codeptr;
    code(tapemem + BOFF);

    assert(munmap(code, codelen) == 0);
}

#else
#warning "Supported processor not detected; using bf2run."
#include "bf2run.c"
#endif
