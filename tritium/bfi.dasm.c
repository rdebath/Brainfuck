#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "bfi.tree.h"

#ifndef DISABLE_DYNASM
#include <stdint.h>
#include <assert.h>
#include <sys/mman.h>

#include "bfi.run.h"

#include "dynasm/dasm_proto.h"

void link_and_run(dasm_State **state);
int tape_step = sizeof(int);

#if defined(__amd64__) || defined(_M_AMD64)
#include "dynasm/dasm_x86.h"
#include "bfi.dasm.amd64.h"
#elif defined(__i386__) || defined(_M_IX86)
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

#ifdef DASM_S_OK
typedef int (*fnptr)(char* memory);
fnptr code = 0;
size_t codelen;

void
link_and_run(dasm_State ** state)
{
    size_t  size;
    int     dasm_status = dasm_link(state, &size);
    assert(dasm_status == DASM_S_OK);

    if (verbose) fprintf(stderr, "Linking Dynasm code\n");
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
    FILE   *f = fopen("/tmp/code-dasm.bin", "w");
    fwrite(codeptr, size, 1, f);
    fclose(f);
#endif

    if (verbose)
	fprintf(stderr, "Link %d bytes of Dynasm code, running.\n", (int)codelen);

    if (isatty(STDOUT_FILENO)) setbuf(stdout, 0);
    code = (void *) codeptr;
    code(map_hugeram());

    assert(munmap(code, codelen) == 0);
}

#endif
#endif
