#include <stdio.h>
int main (int argc, char *argv[]) {
    char *pgm = NULL, *p;
    char t[65536]={};
    unsigned short mp = 0;
    int i;
    FILE * fp = argc>1?fopen(argv[1], "r"):stdin;
    size_t bytes_read = 0;
    setbuf(stdout,0);
    if(!fp || getdelim(&pgm, &bytes_read, argc>1?'\0':'!', fp) < 0)
	perror(argv[1]);
    else if(pgm && bytes_read>0) for(p=pgm;*p;p++) switch(*p) {
	case '>': mp++;break;
	case '<': mp--;break;
	case '+': t[mp]++;break;
	case '-': t[mp]--;break;
	case '.': putchar(t[mp]);break;
	case ',': i=getchar();if(i!=EOF)t[mp]=i;break;
	case '[': if(t[mp]==0)while(*p!=']')p++;break;
	case ']': if(t[mp]!=0)while(*p!='[')p--;break;
    }
    return 0;
}
