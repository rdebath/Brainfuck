#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
static unsigned char mem[65536];
static char bf[] = "><+-][.,#";
    char *pgm = 0;
    int *jmp;
    unsigned short m=0;
    int maxp=0, p=0, ch;
    int on_eof = 1, debug = 0;
    FILE * f;
    for (;;) {
	if (argc < 2 || argv[1][0] != '-' || argv[1][1] == '\0') {
	    break;
	} else if (!strcmp(argv[1], "-e")) {
	    on_eof = -1; argc--; argv++;
	} else if (!strcmp(argv[1], "-z")) {
	    on_eof = 0; argc--; argv++;
	} else if (!strcmp(argv[1], "-n")) {
	    on_eof = 1; argc--; argv++;
	} else if (!strcmp(argv[1], "-d")) {
	    debug = 1; argc--; argv++;
	} else if (argv[1][0] == '-') {
	    fprintf(stderr, "Unknown option '%s'\n", argv[1]);
	    exit(1);
	} else break;
    }
    f = argc>1&&strcmp(argv[1],"-")?fopen(argv[1],"r"):stdin;
    if(!f) perror(argv[1]); else
    while((ch=getc(f)) != EOF && (ch!='!'||f!=stdin))
	if(strchr(bf,ch) && (debug || ch != '#')) {
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
	    case '>': m++; break;
	    case '<': m--; break;
	    case '+': mem[m]++; break;
	    case '-': mem[m]--; break;
	    case ']': if(mem[m]) p += jmp[p]; break;
	    case '[':
		if(jmp[p]) { if(!mem[m]) p += jmp[p]; break; }
		else {
		    int j = p, l=1;
		    while(pgm[j] && l){ j++;l+=(pgm[j]=='[')-(pgm[j]==']'); }
		    if(!pgm[j]) break;
		    jmp[p] = j-p; jmp[j] = p-j;
		    p--; continue;
		}
		break;
	    case '.': putchar(mem[m]); break;
	    case ',': {	int a=getchar();
			if(a!=EOF) mem[m]=a;
			else if (on_eof != 1) mem[m] = on_eof;
		    }
		    break;
	    case '#':
		{
		    printf("\n%2d %2d %2d %2d %2d %2d %2d %2d %2d %2d\n%*s\n",
		       *mem,mem[1],mem[2],mem[3],mem[4],mem[5],
		       mem[6],mem[7],mem[8],mem[9],3*m+2,"^");
		}
	    }
	}
    }
    return 0;
}
