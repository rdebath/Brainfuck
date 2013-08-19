/*****************************************************************************
This is an interpreter for the "brainfuck" language.

It simply takes the original code, runlength encodes it and adds a couple
of minor instruction tweaks.

It runs embarrassingly quickly.

But please do compile it with an optimising compiler; the interpretation 
loop is only a good start not perfect assembler.

gcc -O3 -Wall -pedantic -ansi -o bf bf.c

*****************************************************************************/
#ifdef __STDC__
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define XTRAINST
#define no_ALIGNTO sizeof(int)

void read_image(char * imagename);
void process_image(void);
void run_image(void);
void hex_image(void);

unsigned char * image;
int imagelen;
unsigned char * mem;
int memlen = 128*1024;

int
main(int argc, char **argv)
{
#ifdef ALIGNTO
/* This triggers a Bus Error on an unaligned access. */
#if defined(__GNUC__)
# if defined(__i386__)
    /* Enable Alignment Checking on x86 */
    __asm__("pushf\norl $0x40000,(%esp)\npopf");
# elif defined(__x86_64__) 
     /* Enable Alignment Checking on x86_64 */
    __asm__("pushf\norl $0x40000,(%rsp)\npopf");
# endif
#endif
#endif

    if(argc < 2) {
	fprintf(stderr, "Usage: %s filename\n", argv[0]);
	exit(24);
    }
    read_image(argv[1]);
    process_image();
    run_image();
    /*hex_image();*/

    free(image); *image = 0; imagelen = 0;
    free(mem); *mem = 0; memlen = 0;
    return 0;
}

void
read_image(char * imagename)
{
   int fd;
   struct stat st;

   fd = open(imagename, 0);
   if( fstat(fd, &st) < 0 )
   {
      fprintf(stderr, "Cannot stat file %s\n", imagename);
      exit(9);
   }
   imagelen=st.st_size;
   if( imagelen!=st.st_size || (image = malloc(imagelen)) == 0 )
   {
      fprintf(stderr, "Out of memory\n");
      exit(7);
   }
   if( read(fd, image, imagelen) != imagelen )
   {
      fprintf(stderr, "Read error reading %s\n", imagename);
      exit(7);
   }
   close(fd);
}

void
process_image(void)
{
    int i, p;
    int c, lastc = 0, lastcount = 0;
    int len = 0, newimagelen;
    unsigned char * newimage = 0;
    int depth = 0, maxdepth = 0;
    int * stack;

    for(p=i=0; i<imagelen; i++) {
	switch(c = image[i])
	{
	    case '<': case '>': case '+': case '-':
		if (c == lastc) lastcount++;
		else {
		    lastc = c;
		    lastcount=1;
		    len++;
		}
		if (lastcount == 31) lastc = 0;
		image[p++] = c;
		break;

	    case '.': case ',':
		lastc = c;
		lastcount=1;
		len++;
		image[p++] = c;
		break;
	    case '[': case ']':
		lastc = c;
		lastcount=1;
#ifdef ALIGNTO
		while (len % ALIGNTO != ALIGNTO-1) 
		    len++;
#endif
		len += 1+sizeof(int);
		if (c == '[') {
		    depth++;
		    if (depth > maxdepth) maxdepth = depth;
		} else if (depth>0)
		    depth --;
		image[p++] = c;
		break;
	}
    }
    while(p<imagelen) image[p++] = 0;

    /* 32 bytes between program and data. */
    newimagelen = len+8;
    newimage = calloc(newimagelen, 1);
    stack = calloc(maxdepth+1, sizeof*stack);

    lastc = 0;
    for(p=i=0; i<imagelen; i++) {
	switch(c = image[i])
	{
	    case '<': case '>': case '+': case '-':
		if (c == lastc) {
		    lastcount++;
		    newimage[p-1] += 8;
		} else {
		    switch (c) {
			case '<': newimage[p++] = 0 + 8; break;
			case '>': newimage[p++] = 1 + 8; break;
			case '+': newimage[p++] = 2 + 8; break;
			case '-': newimage[p++] = 3 + 8; break;
		    }
		    lastc = c;
		    lastcount=1;
		}
		if (lastcount == 31) lastc = 0;
		break;

	    case '.': case ',':
		lastc = c;
		lastcount=1;
		newimage[p++] = 4 + (c == ',');
		break;
	    case '[': case ']':
		lastc = c;
		lastcount=1;
#ifdef XTRAINST
		if (image[i] == '[' && image[i+2] == ']') {
		    if (image[i+1] == '>'){
			newimage[p++] = (1<<3) + 7;
			i+=2;
			break;
		    }
		    if (image[i+1] == '<'){
			newimage[p++] = (2<<3) + 7;
			i+=2;
			break;
		    }
		    if (image[i+1] == '-' || image[i+1] == '+'){
			newimage[p++] = (3<<3) + 7;
			i+=2;
			break;
		    }
		}
#endif
#ifdef ALIGNTO
		while (p % ALIGNTO != ALIGNTO-1) 
		    newimage[p++] = 0xFF;
#endif
		newimage[p] = 6 + (c == ']');

		if (c == '[') {
		    stack[depth] = p;
		    depth++;
		    if (depth > maxdepth) maxdepth = depth;
		    *((int*)(newimage+(p+1))) = sizeof(int);
		} else if (depth>0) {
		    /* WARNING: Unaligned access to ints.
		     * If your machine can't cope you need to define the
		     * ALIGNTO macro with the alignment you need.
		     */
		    depth --;
		    *((int*)(newimage+(stack[depth]+1))) =
			    (p+1+sizeof(int)) - (stack[depth]+1);

		    *((int*)(newimage+(p+1))) = stack[depth] - (p+1);
		} else
		    *((int*)(newimage+(p+1))) = sizeof(int);

		p += 1+sizeof(int);
		break;
	}
    }
    newimage[p] = 0;

    free(stack);
    free(image);
    image = newimage;
    imagelen = newimagelen;
}

void
hex_output(FILE * ofd, int ch)
{
static char linebuf[80];
static char buf[20];
static int pos = 0, addr = 0;

    if( ch == EOF ) {
        if(pos)
            fprintf(ofd, "%06x:  %.66s\n", addr, linebuf);
        pos = 0;
             addr = 0;
    } else {
        if(!pos)
            memset(linebuf, ' ', sizeof(linebuf));
        sprintf(buf, "%02x", ch&0xFF);
        memcpy(linebuf+pos*3+(pos>7), buf, 2);

        if( ch > ' ' && ch <= '~' )
                linebuf[50+pos] = ch;
        else    linebuf[50+pos] = '.';
        pos = ((pos+1) & 0xF);
        if( pos == 0 ) {
			fprintf(ofd, "%06x:  %.66s\n", addr, linebuf);
			addr += 16;
		}
    }
}

void hex_image(void)
{
    int     i, j = 0;
    for (i = 0; i < imagelen; i++)
	hex_output(stdout, image[i]);
    hex_output(stdout, EOF);
    putchar('\n');
    for (i = 0; i < memlen + 1; i++)
	if (mem[i])
	    j = i + 1;
    j = ((j + 15) & ~0xF);
    for (i = 0; i < j; i++)
	hex_output(stdout, mem[i]);
    hex_output(stdout, EOF);
}

/* Byte code interpreter.
 * 
 * Byte		Code
 * 00000000	End.
 * xxxxx000	m -= x
 * xxxxx001	m += x
 * xxxxx010	m[0] += x
 * xxxxx011	m[0] -= x
 * 00000100	print m[0]
 * 00000101	input m[0]
 * 00000110	start loop (followed by an int.)
 * 00000111	end loop (followed by an int.)
 * 00001111	[>]
 * 00010111	[<]
 * 00011111	[-] or [+]
 * 11111111	Defined NOP instruction.
 */
void run_image(void)
{
    unsigned char *m, *p;
    int c;

    m = mem = calloc(memlen,sizeof*m);
    p = image;
#if DEBUG
    fprintf(stderr, "p=0x%08x+%d, m=0x%08x\n", p, m-p, m);
#endif

    while((c = *p++)) 
    {
#if DEBUG
	fprintf(stderr, "p=%d, m=%d, *m=%d, c=0x%03o\n",
		p-image-1, m-mem, *m, c);
#endif
	switch(c & 7)
	{
	    case 0: m -= (c >> 3); break;
	    case 1: m += (c >> 3); break;
	    case 2: *m += (c >> 3); break;
	    case 3: *m -= (c >> 3); break;
	    case 4: write(1,m,1); break;
	    case 5: read(0,m,1); break;
	    case 6:
		if (*m) p += sizeof(int);
		else
		    p = p + *((int*)p);
		break;
#ifndef XTRAINST
	    case 7:
		p = p + *((int*)p);
		break;
#else
	    case 7:
		switch(c>>3)
		{
		case 0:
		    p = p + *((int*)p);
		    break;
		case 1:
#ifndef __OPTIMIZE__
		    m += strlen(m);
#else
		    while(*m) m++;
#endif
		    break;
		case 2:
		    while(*m) m--;
		    break;
		case 3:
		    *m = 0;
		    break;
		}
		break;
#endif
	}
#if DEBUG
	fprintf(stderr, "p=%d, m=%d, *m=%d\n", p-image, m-mem, *m);
#endif
    }
}

/* Byte code interpreter.	(NEW)
 * 
 * Byte		Code
 * 00000000	End
 * 00000001	NOP
 * 00010000	loop start[i]
 * 00100000	loop end[i]
 * 00110000	[>]		extend: [>*x]
 * 01000000	[<]		extend: [<*x]
 * 01010000	[-] or [+]
 * 01100000			[<+>-]
 * 01110000
 * 100xxxxx	m += x
 * 101xxxxx	m -= x
 * 110xxxxx	m[0] += x
 * 111xxxxx	m[0] -= x
 *
 * Byte		Code
 * 00000000	End.
 * xxxxx000	m -= x
 * xxxxx001	m += x
 * xxxxx010	m[0] += x
 * xxxxx011	m[0] -= x
 * 00000100	print m[0]
 * 00000101	input m[0]
 * 00000110	start loop (followed by an int.)
 * 00000111	end loop (followed by an int.)
 * 00001111	[>]
 * 00010111	[<]
 * 00011111	[-] or [+]
 * 11111111	Defined NOP instruction.
 */

#ifndef __OPTIMIZE__
#warning The interpreter should be optimised for best performance.
#endif
#ifdef  __TINYC__
#warning Beware TCC does NOT optimise
#ifdef __BOUNDS_CHECKING_ON
#warning and is even slower with bounds checking!
#endif
#endif
#endif

/*
struct sigaction saSegf;
struct sigaction oldAct;
     
saSegf.sa_sigaction = sighandler;
sigemptyset(&saSegf.sa_mask);
saSegf.sa_flags = SA_SIGINFO;
     
     
if(0 > sigaction(SIGSEGV, &saSegf, NULL)) //&oldAct))
    perror("Error SigAction SIGSEG");

*/
