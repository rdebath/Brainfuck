#include <stdio.h>
#include <string.h>
int jmpstk[200], sp = 0;

int
main(int argc, char **argv){
    static char pgm[BUFSIZ*1024];
    static unsigned char mem[65536];
    int m = 0;
    int p=0, ch;
    FILE * f = fopen(argv[1],"r");
    while((ch=getc(f)) != EOF) if(strchr("+-<>[].,",ch)) pgm[p++] = ch;
    pgm[p++] = 0;
    fclose(f);
    setbuf(stdout,0);
    p=0;

    while(pgm[p]) {
	if (pgm[p] == '[') {
	    if(mem[m]) {
		jmpstk[sp++] = p+1;
	    } else {
		int l=1;
		p++;
		while(pgm[p] && l>0) {
		    if (pgm[p] == '[') l++;
		    else if (pgm[p] == ']') l--;
		    p++;
		}
		if (!pgm[p]) break;
	    }
	}

	if (pgm[p] == ']') {
	    if (mem[m]) {
		p = jmpstk[sp-1];
	    } else
		sp--;
	}

	if (pgm[p] == '+') mem[m]++;
	if (pgm[p] == '-') mem[m]--;
	if (pgm[p] == '<') m--;
	if (pgm[p] == '>') m++;
	if (pgm[p] == '.') putchar(mem[m]);
	if (pgm[p] == ',') {int a=getchar(); if(a!=EOF) mem[m]=a;}

	p++;
    }
}
