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

#include "bfi.run.h"

#include "dynasm/dasm_proto.h"

static void link_and_run(dasm_State **state);
static int tape_step = sizeof(int);
static int dump_code = 0;

int
checkarg_dynasm(char * opt, char * arg)
{
    if (!strcmp(opt, "-dump")) {
	dump_code = 1;
	return 1;
    }
    return 0;
}

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_AMD64)
#include "dynasm/dasm_x86.h"
#include "bfi.dasm.amd64.h"
#define CPUID "x86_64"
#elif defined(__i386__) || defined(_M_IX86)
#include "dynasm/dasm_x86.h"
#include "bfi.dasm.i686.h"
#define CPUID "i686"
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
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

typedef int (*fnptr)(char* memory);
fnptr code = 0;
size_t codelen;

static void
link_and_run(dasm_State ** state)
{
    char   *codeptr;
    size_t  size;
    int     dasm_status = dasm_link(state, &size);
    if (dasm_status != DASM_S_OK) {
	fprintf(stderr, "Process dasm_link() failed\n");
	exit(1);
    }

#if !defined(LEGACYOS) && !defined(_WIN32)
#ifdef MAP_ANONYMOUS
    codeptr =
	(char *) mmap(NULL, size,
		      PROT_READ | PROT_WRITE | PROT_EXEC,
		      MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
#else
    /* This'll probably only work with Linux ... Oh, FreeBSD too. */
    int fd = open("/dev/zero", O_RDWR);
    if (fd >= 0) {
	codeptr =
	    (char *) mmap(NULL, size,
			  PROT_READ | PROT_WRITE | PROT_EXEC,
			  MAP_PRIVATE, fd, 0);
	close(fd);
    }
#endif
    if (codeptr == MAP_FAILED) {
	perror("Unable to allocate executable memory");
	exit(1);
    }
#elif defined(_WIN32)
    codeptr = (char*) VirtualAlloc(0, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!codeptr) {
	perror("Unable to allocate executable memory");
	exit(1);
    }
#else
    codeptr = malloc(size);
#endif
    codelen = size;

    dasm_encode(state, codeptr);
    dasm_free(state);

#if !defined(LEGACYOS) && !defined(_WIN32)
    if (mprotect(codeptr, size, PROT_EXEC | PROT_READ) != 0) {
	if (verbose)
	    perror("mprotect read only");
    };
#elif defined(_WIN32)
    DWORD oldProt = 0;
    if (!VirtualProtect(codeptr, size, PAGE_EXECUTE_READ, &oldProt)) {
	if (verbose)
	    perror("VirtualProtect read only failed");
    }
#endif

    /* Write generated machine code to a temporary file.
    // View with:
    //  objdump -D -b binary -mi386 -Mx86,intel code.bin
    // or
    //  ndisasm -b32 code.bin
    */
    if (dump_code)
    {
	const char *fname = "/tmp/code-dasm.bin";
	FILE   *f = fopen(fname, "w");
	fwrite(codeptr, size, 1, f);
	fclose(f);
	fprintf(stderr, "Dynasm "CPUID" code dumped to file '%s'\n", fname);
    }

    if (verbose)
	fprintf(stderr, "Compiled %d bytes of "CPUID" Dynasm code, running.\n", (int)codelen);

    /* The C99 standard leaves casting from "void *" to a function
       pointer undefined.  The assignment used below is the POSIX.1-2003
       (Technical Corrigendum 1) workaround; see the Rationale for the
       POSIX specification of dlsym(). */
					     /* -- Linux man page dlsym() */
    *(void **) (&code) = codeptr;
    start_runclock();
    code(map_hugeram());
    finish_runclock();

    if (verbose>1)
	fprintf(stderr, "Run complete\n");

#if !defined(LEGACYOS) && !defined(_WIN32)
    if (munmap(codeptr, codelen) != 0)
	if (verbose)
	    perror("munmap(code..)");
#elif defined(_WIN32)
    (void)VirtualFree(codeptr, 0, MEM_RELEASE);
#else
    free(codeptr);
#endif
}

#endif
#endif
