/* From dietlib Copyright GPL V2
 * Felix von Leitner <felix-dietlibc@fefe.de>
 * Bugfix by RDB and altered for "gcc -include this.h"
 */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

#ifdef SSIZE_MAX
#define t_usize size_t
#define t_ssize ssize_t
#else
#define t_usize int
#define t_ssize int
#endif
t_ssize
getdelim(char **lineptr, t_usize *n, int delim, FILE *stream) {
  t_usize i;
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
