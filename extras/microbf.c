#include <stdio.h>
#include <stdlib.h>
int main (int argc, char *argv[]) {
    unsigned short m=0; int i=0, n=0, s=0, g=0, c;
    char *b = calloc(s=1024,1), *p, t[65536]={0};
    FILE *fp=argc>1?fopen(argv[1], "r"):stdin;
    while (fp && (c=getc(fp)) != EOF && (c!='!' || argc>1 || !g)) {
	if (n+2>s && !(b = realloc(b, s += 1024))) break;
	if (c) g |= ((b[n++] = c) == ','); b[n] = 0;
    }
    if(!fp || !b) { perror(argv[1]); return 1;}
    for(p=b;*p;p++)switch(*p) {
	case '>': m++;break;
	case '<': m--;break;
	case '+': t[m]++;break;
	case '-': t[m]--;break;
	case '.': putchar(t[m]);break;
	case ',': if((c=getchar())!=EOF)t[m]=c;break;
	case '[': if(t[m]==0)while((i+=(*p=='[')-(*p==']'))&&p[1])p++;break;
	case ']': if(t[m]!=0)while((i+=(*p==']')-(*p=='['))&&p>b)p--;break;
    }
    return 0;
}
