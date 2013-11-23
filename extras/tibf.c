#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Byte array with jitjump translation from BF, runs at about 210,000,000 instructions per second.
 */

static unsigned char mem[65536];

int main(int argc, char **argv) {
    char *pgm = 0;
    int *jmp;
    unsigned short m=0;
    int maxp=0, p=0, ch;
    FILE * f = argc>1?fopen(argv[1],"r"):stdin;
    if(!f) perror(argv[1]); else
    while((ch=getc(f)) != EOF && (ch!='!'||f!=stdin))
	if(strchr("+-<>[].,",ch)) {
	    while (p+2 > maxp) {
		pgm = realloc(pgm, maxp += 1024);
		if(!pgm) { perror(argv[0]); exit(1); }
	    }
	    pgm[p++] = ch;
	}
    if(pgm) {
	pgm[p++] = 0;
	if (f!=stdin) fclose(f);
	jmp = calloc(maxp, sizeof*jmp);
	if(!jmp) { perror(argv[0]); exit(1); }
	setbuf(stdout, 0);
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
		    while(pgm[j] && l){ j++;l-=(pgm[j]==']')-(pgm[j]=='['); }
		    if(!pgm[j]) break;
		    jmp[p] = j-p; jmp[j] = p-j;
		    p--; continue;
		}
		break;
	    }
	}
    }
    return 0;
}
