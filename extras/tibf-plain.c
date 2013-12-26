#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
static unsigned char mem[65536];
static char bf[] = "><+-][.,";
    char *pgm = 0, *c;
    int *jmp;
    unsigned short m=0;
    int maxp=0, p=0, ch;
    FILE * f = argc>1?fopen(argv[1],"r"):stdin;
    if(!f) perror(argv[1]); else
    while((ch=getc(f)) != EOF && (ch!='!'||f!=stdin))
	if((c=strchr(bf,ch))) {
	    while (p+2 > maxp) {
		pgm = realloc(pgm, maxp += 1024);
		if(!pgm) { perror(argv[0]); exit(1); }
	    }
	    pgm[p++] = 1+c-bf;
	}
    if(pgm) {
	pgm[p++] = 0;
	if (f!=stdin) fclose(f);
	jmp = calloc(maxp, sizeof*jmp);
	if(!jmp) { perror(argv[0]); exit(1); }
	setbuf(stdout, 0);
	for(p=0;pgm[p];p++) {
	    switch(pgm[p]) {
	    case 1: m++; break;
	    case 2: m--; break;
	    case 3: mem[m]++; break;
	    case 4: mem[m]--; break;
	    case 5: if(mem[m]) p += jmp[p]; break;
	    case 6:
		if(jmp[p]) { if(!mem[m]) p += jmp[p]; break; }
		else {
		    int j = p, l=1;
		    while(pgm[j] && l){ j++;l+=(pgm[j]==6)-(pgm[j]==5); }
		    if(!pgm[j]) break;
		    jmp[p] = j-p; jmp[j] = p-j;
		    p--; continue;
		}
		break;
	    case 7: putchar(mem[m]); break;
	    case 8: {int a=getchar(); if(a!=EOF) mem[m]=a;} break;
	    }
	}
    }
    return 0;
}
