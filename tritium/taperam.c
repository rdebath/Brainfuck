#ifdef __STRICT_ANSI__
/* Required to expose the signal handling in glibc */
#define _GNU_SOURCE 1
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#if defined(_POSIX_MAPPED_FILES) && ((_POSIX_MAPPED_FILES -0) > 0)
#include <sys/mman.h>
#include <signal.h>
#endif

#include "bfi.tree.h"
#include "bfi.run.h"

#if defined(MAP_PRIVATE) && defined(SA_SIGINFO) && defined(SA_RESETHAND) && !defined(__asmjs__)
#ifndef DISABLE_HUGERAM
#define USEHUGERAM
#endif
#endif

#ifdef USEHUGERAM
#define MEMSIZE     2UL*1024*1024*1024
#define MEMGUARD    16UL*1024*1024
#define MEMSKIP     2UL*1024*1024
#define MEMSIZE64   ((size_t)4*1024*1024*1024*(cell_size==8?1:sizeof(int)))

#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

#endif

/* Where's the tape memory pointer start */
static char * cell_array_pointer = 0;
/* Location of the start of the tape memory. */
static char * cell_array_low_addr = 0;
#ifdef USEHUGERAM
static size_t cell_array_alloc_len = 0;
#endif

/* This is the fallback; it doesn't trap "end of tape". */
#ifndef USEHUGERAM
#ifdef __STRICT_ANSI__
#pragma message "WARNING: Using small memory due to __STRICT_ANSI__, use -mem option."
#else
#warning Using small memory, use -mem option.
#endif

int huge_ram_available = 0;

void *
map_hugeram(void)
{
    if(cell_array_pointer==0) {
	if (hard_left_limit<0) {
	    cell_array_low_addr = tcalloc(memsize-hard_left_limit, sizeof(int));
	    cell_array_pointer = cell_array_low_addr - hard_left_limit*sizeof(int);
	} else {
	    cell_array_low_addr = tcalloc(memsize, sizeof(int));
	    cell_array_pointer = cell_array_low_addr;
	}
    }
    return cell_array_pointer;
}

void
unmap_hugeram(void)
{
    if(cell_array_pointer==0) return;
    free(cell_array_low_addr);
    cell_array_pointer = 0;
    cell_array_low_addr = 0;
}
#endif

/* -- */
#ifdef USEHUGERAM
static void sigsegv_report(int signo, siginfo_t * siginfo, void * ptr) __attribute__((__noreturn__));

static void
sigsegv_report(int signo UNUSED, siginfo_t * siginfo, void * ptr UNUSED)
{
/*
 *  A segfault handler is likely to be called from inside library functions so
 *  I'll avoid calling the standard I/O routines.
 *
 *  But it's be pretty much safe once I've checked the failed access is
 *  within the tape range so I will call exit() rather than _exit() to
 *  flush the standard I/O buffers in that case.
 */

#define estr(x) do{ static char p[]=x; write(2, p, sizeof(p)-1);}while(0)
    if(cell_array_pointer != 0) {
	if ((char*)siginfo->si_addr > cell_array_pointer - MEMGUARD &&
	    (char*)siginfo->si_addr <
		    cell_array_pointer + cell_array_alloc_len - MEMGUARD) {

	    if( (char*)siginfo->si_addr > cell_array_pointer )
		estr("Tape pointer has moved above available space\n");
	    else
		estr("Tape pointer has moved below available space\n");
	    exit(1);
	}
    }
    if((char*)siginfo->si_addr < (char*)0 + 128*1024) {
	estr("Program fault: NULL Pointer dereference.\n");
    } else {
	estr("Segmentation protection violation\n");
	abort();
    }
    _exit(4);
}

static struct sigaction saved_segv;
static int saved_segv_valid = 0;

static void
trap_sigsegv(void)
{
    struct sigaction saSegf;

    saSegf.sa_sigaction = sigsegv_report;
    sigemptyset(&saSegf.sa_mask);
    saSegf.sa_flags = SA_SIGINFO|SA_NODEFER|SA_RESETHAND;

    if(0 > sigaction(SIGSEGV, &saSegf, &saved_segv))
	perror("Error trapping SIGSEGV, ignoring");
    else
	saved_segv_valid = 1;
}

static void
restore_sigsegv(void)
{
    if (!saved_segv_valid) return;
    saved_segv_valid = 0;
    if(0 > sigaction(SIGSEGV, &saved_segv, NULL))
	perror("Restoring SIGSEGV handler, ignoring");
}

int huge_ram_available = 1;

void *
map_hugeram(void)
{
    char * mp;
    size_t tapelength, origtapelen;

    if (cell_array_pointer != 0)
	return cell_array_pointer;

    tapelength = MEMSIZE;
    if (sizeof(size_t) > 4)
	tapelength = MEMSIZE64;

    origtapelen = tapelength + 2*MEMGUARD;

    for(;;) {
	cell_array_alloc_len = tapelength + 2*MEMGUARD;
	if (hard_left_limit<0)
	    cell_array_alloc_len += MEMSKIP;

	/* Map the memory and two guard regions */
	mp = mmap(0, cell_array_alloc_len,
		    PROT_READ+PROT_WRITE,
		    MAP_PRIVATE+MAP_ANONYMOUS+MAP_NORESERVE, -1, 0);

	if (mp != MAP_FAILED)
	    break;

	if (tapelength < 2*MEMGUARD) {
	    /* last chance, no guards. */
	    cell_array_alloc_len = tapelength + (hard_left_limit<0?MEMSKIP:0);

	    mp = mmap(0, cell_array_alloc_len,
			PROT_READ+PROT_WRITE,
			MAP_PRIVATE+MAP_ANONYMOUS+MAP_NORESERVE, -1, 0);

	    if (mp != MAP_FAILED)
		break;

	    perror("Cannot map memory for cell array");
	    exit(1);
	}

	tapelength /= 2;
    }

    if (cell_array_alloc_len < origtapelen)
	if (verbose || cell_array_alloc_len < MEMSIZE/8)
	    fprintf(stderr,
		    "%s: Mapped %luMB of cell array, continuing.\n",
		    cell_array_alloc_len>MEMSIZE?"Note":"WARNING",
		    (unsigned long)(tapelength / (1024*1024)));

    cell_array_low_addr = mp;

#ifdef MADV_MERGEABLE
    if( madvise(mp, cell_array_alloc_len, MADV_MERGEABLE | MADV_SEQUENTIAL) )
	if (verbose)
	    perror("madvise() on tape returned error");
#endif

    if (MEMGUARD > 0 && cell_array_alloc_len >= 2*MEMGUARD) {
	/*
	 * Now, unmap the guard regions ... NO we must disable
	 * access to the guard regions, otherwise they may get remapped. ...
	 * That would be bad.
	 */
	mprotect(mp, MEMGUARD, PROT_NONE);
	mp += MEMGUARD;
	mprotect(mp+cell_array_alloc_len-2*MEMGUARD, MEMGUARD, PROT_NONE);

	/* Give a nice message if the program run into the guard regions */
	trap_sigsegv();
    }

    cell_array_pointer = mp;
    if (hard_left_limit<0)
	cell_array_pointer += MEMSKIP;

    return cell_array_pointer;
}

void
unmap_hugeram(void)
{
    if(cell_array_pointer==0) return;
    if(munmap(cell_array_low_addr, cell_array_alloc_len))
	perror("munmap tape");
    cell_array_pointer = 0;
    cell_array_low_addr = 0;

    restore_sigsegv();
}
#endif
