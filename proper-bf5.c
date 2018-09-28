/*
 *  This one isn't strictly "broken", but it is unusual enough that I need
 *  to specifically test against it.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
int main (int argc, char *argv[]) {
    char *b=0, *p, t[65536]={0};
    unsigned short m=0;
    int i=0;
    FILE *fp=argc>1?fopen(argv[1], "r"):stdin;
    size_t r=0;
    setbuf(stdout, 0);
    if(!fp || getdelim(&b,&r,argc>1?'\0':'!',fp)<0)
	perror(argv[1]);
    else if(b&&r>0)for(p=b;*p;p++)switch(*p) {
	case '>': if((unsigned short)(m+1)) m++; else return 1; break;
	case '<': if(m) m--; else return 1; break;
	case '+': if((char)(t[m]+1)) t[m]++;break;
	case '-': if(t[m]!=0) t[m]--;break;
	case '.': putchar(t[m]);break;
	case ',': {int c=getchar();if(c!=EOF)t[m]=c;}break;
	case '[': if(t[m]==0)while((i+=(*p=='[')-(*p==']'))&&p[1])p++;break;
	case ']': if(t[m]!=0)while((i+=(*p==']')-(*p=='['))&&p>b)p--;break;
    }
    return 0;
}
