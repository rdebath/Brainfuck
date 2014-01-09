#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
static unsigned char mem[65536];
static char bf[] = "><+-][.,#";
    int sp = 0, stk[999], jc[256] = {-1}, jd[256];
    char *pgm = 0, *c;
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
	if((c=strchr(bf,ch)) && (debug || ch != '#')) {
	    while (p+2 > maxp) {
		pgm = realloc(pgm, maxp += 1024);
		if(!pgm) { perror(argv[0]); exit(1); }
	    }
	    pgm[p++] = 1+c-bf;
	}
    if(pgm) {
	pgm[p++] = 0;
	if (f!=stdin) fclose(f);
	setbuf(stdout, 0);
	for(p=0;pgm[p];p++) {
	    switch(pgm[p]) {
	    case 1: m++; break;
	    case 2: m--; break;
	    case 3: mem[m]++; break;
	    case 4: mem[m]--; break;
	    case 5: if(mem[m]) p = stk[sp-1]; else sp--; break;
	    case 6:
		if(!mem[m]) {
		    if (jc[p&0xFF] == p) {
			p = jd[p&0xFF];
		    } else {
			int j = p, l=1;
			while(pgm[j] && l){ j++;l+=(pgm[j]==6)-(pgm[j]==5); }
			if(!pgm[j]) break;
			jc[p&0xFF] = p; jd[p&0xFF] = j;
			p = j;
		    }
		} else
		    stk[sp++] = p;
		break;
	    case 7: putchar(mem[m]); break;
	    case 8: {	int a=getchar();
			if(a!=EOF) mem[m]=a;
			else if (on_eof != 1) mem[m] = on_eof;
		    }
		    break;
	    case 9:
		{
		    printf("%2d %2d %2d %2d %2d %2d %2d %2d %2d %2d\n%*s\n",
		       *mem,mem[1],mem[2],mem[3],mem[4],mem[5],
		       mem[6],mem[7],mem[8],mem[9],3*m+2,"^");
		}
	    }
	}
    }
    return 0;
}
