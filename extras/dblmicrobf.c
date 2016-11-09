/*
 * This file contains two copies of a very small but still uncrashable BF
 * interpreter. They do this by having a 65536 cell wrapping tape.
 *
 * The unmodified code is just 22 lines.
 * The second copy is a modified version that can be compiled with any cell
 * size (upto a complete integer). The working code is the same size, but
 * all the #define's take a bit of space.
 */
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#if _POSIX_VERSION < 200809L
#define getdelim my_getdelim
int getdelim(char **lineptr, size_t *n, int delim, FILE *stream);
#endif

#ifndef CELLSIZE

int main (int argc, char *argv[]) {
    char *b=0, *p, t[65536]={0};
    unsigned short m=0;
    int i=0;
    FILE *fp=argc>1?fopen(argv[1], "r"):stdin;
    size_t r=0;
    if(!fp || getdelim(&b,&r,argc>1?'\0':'!',fp)<0)
	perror(argv[1]);
    else if(b&&r>0)for(p=b;*p;p++)switch(*p) {
	case '>': m++;break;
	case '<': m--;break;
	case '+': t[m]++;break;
	case '-': t[m]--;break;
	case '.': putchar(t[m]);break;
	case ',': {int c=getchar();if(c!=EOF)t[m]=c;}break;
	case '[': if(t[m]==0)while((i+=(*p=='[')-(*p==']'))&&p[1])p++;break;
	case ']': if(t[m]!=0)while((i+=(*p==']')-(*p=='['))&&p>b)p--;break;
    }
    return 0;
}

#else
#if (CELLSIZE > 256) || (CELLSIZE <= 0)
#define CELLTYPE int
#else
#define CELLTYPE unsigned char
#endif

#if (CELLSIZE == 256) || (CELLSIZE <= 0)
#define CELLMOD(x) x
#elif (CELLSIZE == 128) || (CELLSIZE == 4096) || (CELLSIZE == 65536)
#define CELLMOD(x) ((x) & (CELLSIZE-1))
#else
#define CELLMOD(x) (((x) + CELLSIZE) % CELLSIZE)
#endif

int main (int argc, char *argv[]) {
    char *b=0, *p; CELLTYPE t[65536]={0};
    unsigned short m=0;
    int i=0;
    FILE *fp=argc>1?fopen(argv[1], "r"):stdin;
    size_t r=0;
    setbuf(stdout,0);
    if(!fp || getdelim(&b,&r,argc>1?'\0':'!',fp)<0)
	perror(argv[1]);
    else if(b&&r>0)for(p=b;*p;p++)switch(*p) {
	case '>': m++;break;
	case '<': m--;break;
	case '+': t[m] = CELLMOD(t[m] + 1);break;
	case '-': t[m] = CELLMOD(t[m] - 1);break;
	case '.': putchar(t[m]);break;
	case ',': {int c=getchar();if(c!=EOF)t[m]=c;}break;
	case '[': if(t[m]==0)while((i+=(*p=='[')-(*p==']'))&&p[1])p++;break;
	case ']': if(t[m]!=0)while((i+=(*p==']')-(*p=='['))&&p>b)p--;break;
    }
    return 0;
}
#endif

#if _POSIX_VERSION < 200809L
int
getdelim(char **lineptr, size_t *n, int delim, FILE *stream) {
  size_t i;
  if (!lineptr || !n) {
    errno=EINVAL;
    return -1;
  }
  if (!*lineptr) *n=0;
  for (i=0; ; ) {
    int x;
    if (i>=*n) {
      int tmp=*n+100;
      char* new=realloc(*lineptr,tmp);
      if (!new) return -1;
      *lineptr=new; *n=tmp;
    }
    x=fgetc(stream);
    if (x==EOF) { if (!i) return -1; (*lineptr)[i]=0; return i; }
    (*lineptr)[i]=x;
    ++i;
    if (x==delim) break;
  }
  (*lineptr)[i]=0;
  return i;
}
#endif
