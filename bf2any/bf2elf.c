#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#if _POSIX_VERSION >= 199506L
#include <inttypes.h>
#endif

#include "bf2any.h"

/*
 * ELF32 translation from BF, runs at about 2,700,000,000 instructions per second.
 *
 * http://www.muppetlabs.com/~breadbox/software/tiny/teensy.html
 * http://www.muppetlabs.com/~breadbox/software/tiny/bf.asm.txt
 * http://www.muppetlabs.com/~breadbox/pub/software/tiny.tar.gz
 * http://www.muppetlabs.com/~breadbox/software/elfkickers.html
 *
 * Portions (c) 1999-2001 by Brian Raiter
 *
 * This is a stable version of the elf-166 compiler in that this code
 * generator will make a valid executable for all known BF programs.
 * But it doesn't use a true ELF library like the elfkickers page.
 */

int ind = 0;

/*
 * The bits of machine language that, together, make up a compiled
 * program.
 */

#define	MLBIT(size)	static struct { unsigned char _dummy[size]; } const

/* This is a pregenerated ELF32 header for 80386 Linux
 * With the references.
 */

const size_t base_address = 0x08048000;
const size_t p_filesz = 68;
const size_t p_memsz = 72;

MLBIT(84) elfheader = {{
  0x7f, 0x45, 0x4c, 0x46, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x54, 0x80, 0x04, 0x08, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x34, 0x00, 0x20, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x80, 0x04, 0x08, 0x00, 0x80, 0x04, 0x08, 0x54, 0x00, 0x00, 0x00,
  0x54, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00
} };

MLBIT(4) start =    { { 0x31, 0xC0,			/* xor  eax, eax   */
			0x99,				/* cdq             */
			0x42 } };			/* inc  edx        */

MLBIT(2) incchar =  { { 0xFE, 0x01 } };			/* inc  [ecx]      */
MLBIT(2) decchar =  { { 0xFE, 0x09 } };			/* dec  [ecx]      */
MLBIT(1) incptr =   { { 0x41 } };			/* inc  ecx        */
MLBIT(1) decptr =   { { 0x49 } };			/* dec  ecx        */

MLBIT(6) readchar = { { 0xB0, 0x03,			/* mov  al, 3      */
			0x31, 0xDB,			/* xor  ebx, ebx   */
			0xCD, 0x80 } };			/* int  0x80       */
MLBIT(6) writechar= { { 0xB0, 0x04,			/* mov  al, 4      */
			0x89, 0xD3,			/* mov  ebx, edx   */
			0xCD, 0x80 } };			/* int  0x80       */

MLBIT(5) jump =     { { 0xE9, 0, 0, 0, 0 } };		/* jmp  near ???   */
MLBIT(4) branch =   { { 0x3A, 0x31,			/* cmp  dh, [ecx]  */
			0x0F, 0x85 } };			/* jnz  near ...   */

MLBIT(2) addchar =  { { 0x80, 0x01 } };			/* add  [ecx], ... */
MLBIT(2) subchar =  { { 0x80, 0x29 } };			/* sub  [ecx], ... */
MLBIT(2) addptr =   { { 0x83, 0xC1 } };			/* add  ecx, ...   */
MLBIT(2) subptr =   { { 0x83, 0xE9 } };			/* sub  ecx, ...   */

MLBIT(5)
	epilog_ex = { { 0x92,				/* xchg eax, edx   */
			0x31, 0xDB,			/* xor  ebx, ebx   */
			0xCD, 0x80 } };			/* int  0x80       */

const size_t prolog_meminit_offset = 1;
MLBIT(5) prolog_ex = { { 0xB9, 0, 0, 0, 0 } };		/* mov  ecx, ???   */


/* A pointer to the text buffer.
 */
static char	       *textbuf;

/* The amount of memory allocated for the text buffer.
 */
static size_t		textsize;

/* The first byte past the program being compiled in the text buffer.
 */
static size_t		pos;

/* Appends the given bytes to the program being compiled.
 */
static void emit(void const *bytes, size_t size)
{
    if (pos + size > textsize) {
	textsize += 4096;
	if (!(textbuf = realloc(textbuf, textsize))) {
	    fputs("Out of memory!\n", stderr);
	    exit(EXIT_FAILURE);
	}
    }
    memcpy(textbuf + pos, bytes, size);
    pos += size;
}

/* Macros to simplify calling emit with predefined bits of code.
 */
#define	emitobj(obj)		(emit(&(obj), sizeof (obj)))
#define	emitarg(seq, arg)	(emitobj(seq), emitobj(arg))

/* Modifies previously emitted code at the given position.
 */
#define	insertobj(at, obj)	(memcpy(textbuf + (at), &(obj), sizeof (obj)))

/* File to deposit the final executable.
 */
static const char * filename = "a.out";

/* Stack for holding addresses of while loops.
 */
static size_t  stack[1024];
static size_t *st;

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-b") == 0) return 1;
    if (strncmp(arg, "-o", 2) == 0 && arg[2]) {
	filename = arg+2;
	return 1;
    }
    return 0;
}

void
outcmd(int ch, int count)
{
    FILE * ofd;
#if _POSIX_VERSION >= 199506L
    int32_t p;
    int8_t arg;
#else
    int p;		/* BEWARE: Assuming this is a good int. */
    signed char arg;	/* Another assembler type: one byte signed. */
#endif

    /* limit count to the range of a signed char */
    while (count>127) { outcmd(ch, 127); count -= 127; }
    arg = count;

    switch(ch) {
    case '!':
	emitobj(elfheader);
	emitobj(start);
	emitobj(prolog_ex);
	st = stack;
	break;

    case '+': arg == 1 ? emitobj(incchar) : emitarg(addchar, arg); break;
    case '-': arg == 1 ? emitobj(decchar) : emitarg(subchar, arg); break;
    case '<': arg == 1 ? emitobj(decptr)  : emitarg(subptr, arg); break;
    case '>': arg == 1 ? emitobj(incptr)  : emitarg(addptr, arg); break;

    case '[':
	if (st - stack > (int)(sizeof stack / sizeof *stack)) {
	    fprintf(stderr,
		    "too many levels of nested loops (%d configured)\n",
		    (int)(sizeof stack / sizeof *stack));
	    exit(1);
	}
	emitobj(jump);
	*st = pos;
	++st;
	break;
    case ']':
	--st;
	p = pos - *st;
	insertobj(*st - sizeof p, p);
	p = -(p + sizeof p + sizeof branch);
	emitarg(branch, p);
	break;

    case ',': emitobj(readchar); break;
    case '.': emitobj(writechar); break;

    case '~':
	emitobj(epilog_ex);

	p = pos;
	insertobj(p_filesz, p);

	p = pos + tapesz;
	insertobj(p_memsz, p);

	p = pos + base_address + tapeinit;
	insertobj(sizeof(elfheader) + sizeof(start) + prolog_meminit_offset, p);

	{
#ifndef _WIN32
	    mode_t umsk = umask(S_IRWXG + S_IRWXO);
#endif
	    if ((ofd = fopen(filename, "w")) == NULL) {
		perror(filename);
		exit(1);
	    } else {
		fwrite(textbuf, 1, pos, ofd);
		fclose(ofd);
#ifndef _WIN32
		if (chmod(filename, (S_IRWXU + S_IRWXG + S_IRWXO) & ~umsk) < 0)
		    perror(filename);
#endif
	    }
#ifndef _WIN32
	    umask(umsk);
#endif
	}
	break;
    }
}
