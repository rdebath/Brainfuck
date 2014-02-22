#include <stdio.h>
#include <string.h>
main(int argc, char **argv){
    static char pgm[BUFSIZ*1024];
    static int jmp[sizeof(pgm)];
    static unsigned char mem[65536];
    unsigned short m=0;
    int p=0, ch;
    FILE * f = fopen(argv[1],"r");
    while((ch=getc(f)) != EOF) if(strchr("+-<>[].,",ch)) pgm[p++] = ch;
    pgm[p++] = 0;
    fclose(f);
    for(p=0;pgm[p];p++) {
	switch(pgm[p]) {
	case '+': mem[m]++; break;
	case '-': mem[m]--; break;
	case '>': m++; break;
	case '<': m--; break;
	case '.': putchar(mem[m]); break;
	case ',': {int a=getchar(); if(a!=EOF) mem[m]=a;} break;
	case ']': if(mem[m]) p += jmp[p]; break;
	case '[':
	    if(jmp[p]) { if(!mem[m]) p += jmp[p]; break; }
	    else {
		int j = p, l=1;
		while(l){ j++;l-=(pgm[j]==']')-(pgm[j]=='['); }
		jmp[p] = j-p; jmp[j] = p-j;
		p--; continue;
	    }
	    break;
	}
    }
}
