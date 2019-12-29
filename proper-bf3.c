#include <stdio.h>
int main (int argc, char *argv[]) {
    char *b=0, *p;
    int t[65536]={0};
    unsigned short m=0;
    int i=0;
    FILE *fp=argc>1?fopen(argv[1], "r"):stdin;
    size_t r=0;
    setbuf(stdout, 0);
    if(!fp || getdelim(&b,&r,argc>1?'\0':'!',fp)<0)
	perror(argv[1]);
    else if(b&&r>0)for(p=b;*p;p++)switch(*p) {
	case '>': m++; if(!m) return fprintf(stderr, "Tape overflow\n"); break;
	case '<': if(!m) return fprintf(stderr, "Tape underflow\n"); m--; break;
	case '+': t[m]++;break;
	case '-': t[m]--;break;
	case '.': putchar(t[m]);break;
	case ',': {int c=getchar();if(c!=EOF)t[m]=c;}break;
	case '[': if(t[m]==0)while((i+=(*p=='[')-(*p==']'))&&p[1])p++;break;
	case ']': if(t[m]!=0)while((i+=(*p==']')-(*p=='['))&&p>b)p--;break;
    }
    return 0;
}
