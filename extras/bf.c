#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define SUPINST

void read_image(char * imagename);
void process_image(void);
void run_image(void);

unsigned char * image;
int imagelen;
int memsize = 30000;

int
main(int argc, char **argv)
{
    read_image(argv[1]);
    process_image();
    run_image();
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
    int len = 1, newimagelen;
    unsigned char * newimage = 0;
    int depth = 0, maxdepth = 0;
    int * stack;

    for(i=0; i<imagelen; i++) {
	switch(c = image[i])
	{
	    case '<': case '>': case '+': case '-':
		if (c == lastc) lastcount++;
		else {
		    lastc = c;
		    lastcount=1;
		    len++;
		}
		if (lastcount == 15) lastc = 0;
		break;

	    case '.': case ',':
		lastc = c;
		lastcount=1;
		len++;
		break;
	    case '[': case ']':
		lastc = c;
		lastcount=1;
		len += 5;
		if (c == '[') {
		    depth++;
		    if (depth > maxdepth) maxdepth = depth;
		} else if (depth>0)
		    depth --;
		break;
	}
    }

    /* 32 bytes between program and data. */
    newimagelen = len + memsize + 32;
    newimage = calloc(newimagelen, 1);
    stack = calloc(maxdepth+1, sizeof*stack);

    lastc = 0;
    for(p=i=0; i<imagelen; i++) {
	switch(c = image[i])
	{
	    case '<': case '>': case '+': case '-':
		if (c == lastc) {
		    lastcount++;
		    newimage[p-1] += 16;
		} else {
		    switch (c) {
			case '<': newimage[p++] = 0 + 16; break;
			case '>': newimage[p++] = 1 + 16; break;
			case '+': newimage[p++] = 2 + 16; break;
			case '-': newimage[p++] = 3 + 16; break;
		    }
		    lastc = c;
		    lastcount=1;
		}
		if (lastcount == 15) lastc = 0;
		break;

	    case '.': case ',':
		lastc = c;
		lastcount=1;
		newimage[p++] = 4 + (c == ',');
		break;
	    case '[': case ']':
		lastc = c;
		lastcount=1;
#ifdef SUPINST
		if (image[i] == '[' && image[i+1] == '>' && image[i+2] == ']') {
		    newimage[p++] = 8;
		    i+=2;
		    break;
		}
		if (image[i] == '[' && image[i+1] == '<' && image[i+2] == ']') {
		    newimage[p++] = 9;
		    i+=2;
		    break;
		}
		if (image[i] == '[' && image[i+1] == '-' && image[i+2] == ']') {
		    newimage[p++] = 10;
		    i+=2;
		    break;
		}
#endif
		newimage[p] = 6 + (c == ']');

		if (c == '[') {
		    stack[depth] = p;
		    depth++;
		    if (depth > maxdepth) maxdepth = depth;
		} else if (depth>0) {
		    depth --;
		    *((int*)(newimage+(stack[depth]+1))) =
			    (p+5) - (stack[depth]+1);

		    *((int*)(newimage+(p+1))) = stack[depth] - (p+1);
		} else
		    newimage[p] = 0xFF;

		p += 5;
		break;
	}
    }
    newimage[p] = 0xFF;

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
    int i;
    for(i=0; i<imagelen; i++)
	hex_output(stdout, image[i]);

    hex_output(stdout, EOF);
}

void run_image(void)
{
    unsigned char *m, *p;
    int c;

    m = image + imagelen - memsize;
    p = image;
#if DEBUG
    fprintf(stderr, "p=0x%08x+%d, m=0x%08x\n", p, m-p, m);
#endif

    while((c = *p++) != 0xFF) 
    {
#if DEBUG
	fprintf(stderr, "p=%d, m=%d, *m=%d, c=0x%03o\n",
		p-image-1, m - (image + imagelen - memsize), *m, c);
#endif
	switch(c & 15)
	{
	    case 0: m -= (c >> 4); break;
	    case 1: m += (c >> 4); break;
	    case 2: *m += (c >> 4); break;
	    case 3: *m -= (c >> 4); break;
	    case 4: write(1,m,1); break;
	    case 5: read(0,m,1); break;
	    case 6:
		if (*m) p += sizeof(int);
		else
		    p = p + *((int*)p);
		break;
	    case 7:
		p = p + *((int*)p);
		break;
#ifdef SUPINST
	    case 8:
		while(*m) m++;
		break;
	    case 9:
		while(*m) m--;
		break;
	    case 10:
		*m = 0;
		break;
#endif
	}
#if DEBUG
	fprintf(stderr, "p=%d, m=%d, *m=%d\n",
		p-image, m - (image + imagelen - memsize), *m);
#endif
    }
}
