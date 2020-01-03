#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int main (int argc, char *argv[]) {
    unsigned short m=0;
    int i=0, n=0, s=0, g=0, c;
    char *b = malloc(s=1024), *p;
    static char t[65536];
    FILE *fp=argc>1?fopen(argv[1], "r"):stdin;
    if(fp) while ((c=getc(fp)) != EOF && (c!='!' || argc>1 || !g)) {
	if (n+2>s && !(b = realloc(b, s += 1024))) break;
	g |= ((b[n++] = c) == ','); b[n] = 0;
    }
    if(!fp || !b) { perror(argv[1]); return 1;}
    if (isatty(fileno(stdout))) setbuf(stdout, 0);
    for(p=b;*p;p++) {
	t[m += (*p == '>') - (*p == '<')] += (*p == '+') - (*p == '-');
        if (*p == '.') putchar(t[m]);
        if (*p == ',' && ((c=getchar()) != EOF)) t[m]=c;
        if (*p == '[' && t[m]==0) while((i+=(*p=='[')-(*p==']'))&&p[1]) p++;
        if (*p == ']' && t[m]!=0) while((i+=(*p==']')-(*p=='['))&&p>=b) p--;
    }
    return 0;
}
