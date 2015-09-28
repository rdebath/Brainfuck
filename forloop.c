#include <stdio.h>
#include <string.h>
int jmpstk[200], sp = 0;

int
main(int argc, char **argv){
    static char pgm[BUFSIZ*1024];
    static unsigned char mem[65536];
    unsigned short m=0;
    int p=0, ch;
    FILE * f = fopen(argv[1],"r");
    while((ch=getc(f)) != EOF) if(strchr("+-<>[].,#",ch)) pgm[p++] = ch;
    pgm[p++] = 0;
    fclose(f);
    setbuf(stdout,0);
    for(p=0;pgm[p];p++) {
	switch(pgm[p]) {
	case '+': mem[m]++; break;
	case '-': mem[m]--; break;
	case '>': m++; break;
	case '<': m--; break;
	case '.': putchar(mem[m]); break;
	case ',': {int a=getchar(); if(a!=EOF) mem[m]=a;} break;
	case ']': if (mem[m]) { if(sp) { sp--; p=jmpstk[sp]-1; } else p = -1; } break;
	case '[': jmpstk[sp++] = p; break;

	case '#':
            fprintf(stderr,
		"%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d "
		"%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d\n",
                    mem[0], mem[1], mem[2], mem[3], mem[4],
		    mem[5], mem[6], mem[7], mem[8], mem[9],
                    mem[10], mem[11], mem[12], mem[13], mem[14],
		    mem[15], mem[16], mem[17], mem[18], mem[19]
		    );
            if (m >= 0 && m < 20) fprintf(stderr, "%*s\n", 4*m+3, "^");
	    else fprintf(stderr, "offset: %d\n", (short)m);
	    break;
	}
    }
}
