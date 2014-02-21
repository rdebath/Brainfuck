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

void link_and_run(dasm_State **state);
size_t tape_step = sizeof(int);

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_AMD64)
#include "dynasm/dasm_x86.h"
#include "bf2jit.amd64.h"
#elif defined(__i386__) || defined(_M_IX86)
#include "dynasm/dasm_x86.h"
#include "bf2jit.i686.h"
#else
#error "Supported processor not detected."
#endif

#ifdef DASM_S_OK
typedef int (*fnptr)(char * memory);
fnptr code = 0;
size_t codelen;
char * tapemem;

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    return 0;
}

void
link_and_run(dasm_State ** state)
{
    /* dasm_link is the second pass of the assembler, the first phase of the
     * first pass is the lua code which converts the assembler into code
     * fragments in the C file generated from the 'dasc' file. The second
     * phase happens at run time and orders and duplicates these fragments
     * when 'outcmd' is called.
     *
     * This pass resolves all the labels relative to the segments and joins
     * the segments together onto one unit of code. The size of this code
     * is returned in the 'size' argument.
     */
    size_t  size;
    int     dasm_status = dasm_link(state, &size);
    assert(dasm_status == DASM_S_OK);

    /* I allocate this with a 'PROT_EXEC' flag set so the kernel can choose
     * to put it in the 'code area' if there is such a thing */
    char   *codeptr =
	(char *) mmap(NULL, size,
		      PROT_READ | PROT_WRITE | PROT_EXEC,
		      MAP_ANON | MAP_PRIVATE, -1, 0);
    assert(codeptr != MAP_FAILED);
    codelen = size;

    /* Last phase: Stream out the code fragments into proper machine
     * code doing the final absolute relocations.
     *
     * Also after this phase completes the globals array and
     * dasm_getpclabel can be used to find the addresses for
     * '->' and '=>' style labels respectivly.
     */
    dasm_encode(state, codeptr);

    /* This frees up all state for the assembler, everything must go. */
    /* In particular 'dasm_getpclabel()' can no longer be called */
    dasm_free(state);

    /* Make it read only before we run it; also means that a context switch
     * happens so there won't be any 'bad cache' effects */
    assert(mprotect(codeptr, size, PROT_EXEC | PROT_READ) == 0);

#if 0
    /* Write generated machine code to a temporary file.
     * View with:
     *  objdump -D -b binary -mi386 -Mx86,intel code.bin
     * or
     *  ndisasm -b32 code.bin
     */
    FILE   *f = fopen("/tmp/code.bin", "w");
    fwrite(codeptr, size, 1, f);
    fclose(f);
#endif

    /* Remove the stdout output buffer */
    if (isatty(STDOUT_FILENO)) setbuf(stdout, 0);

    /* Memory for the tape ... TODO: make it configurable, dynamic or HUGE */
    tapemem = calloc(32768, tape_step);

    /* I'm not bothering with the ISO type punning for this; you can
     * ignore the warning if your compiler produces one. I'm probably
     * supposed to make the globals array some weird array of unions to
     * stop that happening.
     */
    code = (void *) codeptr;
    code(tapemem + tape_step * BOFF);

    /* Discard code and data */
    assert(munmap(code, codelen) == 0);
    free(tapemem);
}
#endif
