/*****************************************************************************
This is an interpreter for the "brainfuck" language.

It simply takes the original code, runlength encodes it and adds a couple
of minor instruction tweaks.

It runs embarrassingly quickly.

But please do compile it with an optimising compiler; the interpretation 
loop is only a good start not perfect assembler.

gcc -O3 -Wall -pedantic -ansi -o bf bf.c

*****************************************************************************/

/*
TODO:
    Load multiple files into single memory image
    Strip image: Replace all "[...]" sequences after a Start or "]" with spaces
    "!" processing, if data from stdin stop reading on "!"
    Force "[]" to balance, replace unbalanced ones by "{}"
    Strip lines that match "^[\t ]*#.*$"
    Inplace update to bytecode, check the pointers don't collide; if they do
	do the copy to a new calloc()d array.

    Use sigaction and mmap to make the "tape" infinite.
	Add "ADD(0)" tokens evey few kb of tape movement.
	Only create file if memory exceeds N Mb
*/

#ifdef __STDC__
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define XTRAINST
#define NO_ALIGNTO sizeof(int)

typedef unsigned char cell; /* The type of the cell for the 'tape' */

void read_image(char * imagename);
void process_image(void);
void run_image(void);
void hex_image(void);
void set_alignment_trap(void);

unsigned char * image;
int imagelen;
cell * mem;
int memlen = 128*1024;

int
main(int argc, char **argv)
{
    set_alignment_trap();

    if(argc < 2) {
	fprintf(stderr, "Usage: %s filename\n", argv[0]);
	exit(24);
    }
    read_image(argv[1]);
    process_image();
    run_image();
    /*
    hex_image();
    */

    if(image) free(image); image = 0; imagelen = 0;
    if(mem) free(mem); mem = 0; memlen = 0;
    return 0;
}

void
read_image(char * imagename)
{
   int fd = -1;
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
   if (fd > 2) close(fd);
}

void
process_image(void)
{
    int i, p;
    int c, lastc = ']', lastcount = 0;
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
		if (lastcount == 31)
		    lastc = 0;
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
		    newimage[p-1]++;
		} else {
		    switch (c) {
			case '>': newimage[p++] = (0<<4) + 1; break;
			case '<': newimage[p++] = (2<<4) + 1; break;
			case '+': newimage[p++] = (4<<4) + 1; break;
			case '-': newimage[p++] = (6<<4) + 1; break;
		    }
		    lastc = c;
		    lastcount=1;
		}
		if (lastcount == 31)
		    lastc = 0;
		break;

	    case '.': case ',':
		lastc = c;
		lastcount=1;
		newimage[p++] = (10<<4) + (c == ',');
		break;
	    case '[': case ']':
		lastc = c;
		lastcount=1;
#ifdef XTRAINST
		if (image[i] == '[' && image[i+2] == ']') {
		    if (image[i+1] == '-' || image[i+1] == '+'){
			newimage[p++] = 0xB0;
			i+=2;
			break;
		    }
		}

		/* [>>>>>>>>>>>>>>>>] */
		/* 0123456789abcdefg  */
		/*                 v  */
		if (image[i] == '[' && (image[i+1] == '>' || image[i+1] == '<'))
		{
		    int v;
		    for(v=2; v<17; v++) {
			if (image[i+v] != image[i+1])
			    break;
		    }
		    if (image[i+v] == ']') {
			newimage[p++] = 
			    ((12+(image[i+1] == '<'))<<4) +
			    v-2;
			i+=v;
			break;
		    }
		}
#endif
#ifdef ALIGNTO
		while (p % ALIGNTO != ALIGNTO-1) 
		    newimage[p++] = 0x20;
#endif
		newimage[p] = ((8 + (c == ']')) << 4);

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
			    sizeof(int) + p - stack[depth];

		    *((int*)(newimage+(p+1))) = stack[depth] - (p+1);
		} else
		    *((int*)(newimage+(p+1))) = sizeof(int);

		p += 1+sizeof(int);
		break;
	}
    }
    newimage[p++] = 0;
    if (p>newimagelen)
	fprintf(stderr, "Memory overun, conversion to byte code was longer than expected!!\n");

    newimagelen = p;

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
    if (image) {
	for (i = 0; i < imagelen; i++)
	    hex_output(stdout, image[i]);
	hex_output(stdout, EOF);
	putchar('\n');
    }

    if (mem) {
	for (i = 0; i < memlen; i++)
	    if (mem[i])
		j = i + 1;
	j = ((j + 15) & ~0xF);
	for (i = 0; i < j; i++)
	    hex_output(stdout, mem[i]);
	hex_output(stdout, EOF);
    }
}

/* Byte code interpreter.	(TODO)
 * 
 * Byte		Code
 * 00000000	End
 * 00100000	NOP
 *
 * 000xxxxx	m += x
 * 001xxxxx	m -= x
 * 010xxxxx	m[0] += x
 * 011xxxxx	m[0] -= x
 *
 * 10000000	jz [i]
 * 10010000	jnz [i]
 *
 * 10100000	print m[0]
 * 10100001	input m[0]
 * 10100010			SET *m = 0-255
 * 10100011
 * 10100100			ADD *m += 0-255
 * 10100101			SUB *m -= 0-255
 * 10100110			ADD  m += 0-255
 * 10100111			SUB  m -= 0-255
 *
 * 11110000	[-] or [+]	extend: [<+>-]
 * 11000000	[>*x] x=1..16
 * 11010000	[<*x] x=1..16
 *
 * 11100000
 * 11110000	
 */

void run_image(void)
{
    unsigned char *p;
    cell * m;
    int c;

    m = mem = calloc(memlen,sizeof*m);
    p = image;
#if DEBUG
    fprintf(stderr, "p=0x%08x+%d, m=0x%08x+%d\n",
		    image, imagelen, mem, memlen);
#endif

    while((c = *p++)) 
    {
#if DEBUG
	fprintf(stderr, "p=%d, m=%d, *m=%d, c=0x%02x\n",
		p-image-1, m-mem, *m, c);
#endif
	switch(c>>4)
	{
	    case 0: case 1:  m += (c & 0x1F); break;
	    case 2: case 3:  m -= (c & 0x1F); break;
	    case 4: case 5: *m += (c & 0x1F); break;
	    case 6: case 7: *m -= (c & 0x1F); break;

	    case 8:
		if (*m)	p += sizeof(int);
		else	p = p + *((int*)p);
		break;
	    case 9:
		if (*m)	p = p + *((int*)p);
		else	p += sizeof(int);
		break;

	    case 10: 
		switch(c & 0xF)
		{
		    case 0: write(1,m,1); break;
		    case 1: read(0,m,1); break;
		}
		break;

#ifdef XTRAINST
	    case 11:
		*m = 0;
		break;

	    case 12:
		{
		    int v = ((c&0xF) + 1);
		    while(*m) m += v;
		}
		break;
	    case 13:
		{
		    int v = ((c&0xF) + 1);
		    while(*m) m -= v;
		}
		break;
#endif
	}
#if DEBUG
	fprintf(stderr, "p=%d, m=%d, *m=%d\n", p-image, m-mem, *m);
#endif
    }
}

void
set_alignment_trap(void)
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
}

#ifdef  __GNUC__
#ifndef __OPTIMIZE__
#warning The interpreter should be optimised for best performance.
#endif
#endif
#ifdef  __TINYC__
#warning Beware TCC does NOT optimise and the interpreter should be optimised for best performance.
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
