#include <stdio.h>
void interpret(char * istring);

char t[65536]={};
unsigned short mp = 0;

int main (int argc, char *argv[]) {
    char *pgm = NULL, *p;
    FILE * fp = argc>1?fopen(argv[1], "r"):stdin;
    size_t bytes_read = 0;
    setbuf(stdout,0);
    if(!fp || getdelim(&pgm, &bytes_read, argc>1?'\0':'!', fp) < 0)
	perror(argv[1]);
    else if(pgm && bytes_read>0)
	interpret(pgm);
}

void interpret(char * pgm) {
    char * p;
    for(p=pgm;*p;p++) switch(*p) {
	case '>': mp++;break;
	case '<': mp--;break;
	case '+': t[mp]++;break;
	case '-': t[mp]--;break;
	case '.': putchar(t[mp]);break;
	case ',': t[mp]=getchar();break;
	case '[':
	    {
		char * l = p+1;
		while(*p!=']' && *p)p++;
		if (!*p) return;
		*p = 0;
		while(t[mp])
		    interpret(l);
		*p = ']';
	    }
	    break;
    }
}
