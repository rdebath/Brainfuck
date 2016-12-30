#include <stdio.h>
#include <string.h>

int
main(int argc, char **argv){
    static char pgm[BUFSIZ*1024];
    static unsigned char mem[65536];
    short m = 0, b = 0;
    int p=0, ch;
    FILE * f = fopen(argv[1],"r");
    while((ch=getc(f)) != EOF) if(strchr("+-<>[].,",ch)) pgm[p++] = ch;
    pgm[p++] = 0;
    fclose(f);
    setbuf(stdout,0);
    p=0;

    for(;pgm[p];p++) {
	switch(pgm[p]) {
	case '+': mem[m]++; break;
	case '-': mem[m]--; break;
	case '>': m++; break;
	case '<': m--; break;
	case '.': putchar(mem[m]); break;
	case ',': {int a=getchar(); if(a!=EOF) mem[m]=a;} break;
	case '[': b=m; break;
	case ']': if(mem[b]) for(;pgm[p] != '['; p--); break;
	}
    }
}
