#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

void run(void);
char *pgm = 0;
int pgmlen = 0, on_eof = 1, debug = 0;

int main(int argc, char **argv)
{
    FILE * ifd;
    int ch, ar;
    int n = 0, j = 0, i = 0;
    char * fn = 0;

    for (ar=1;ar<argc;ar++) {
	if (!strcmp(argv[ar], "-e")) on_eof = -1;
	else if (!strcmp(argv[ar], "-z")) on_eof = 0;
	else if (!strcmp(argv[ar], "-n")) on_eof = 1;
	else if (!strcmp(argv[ar], "-d")) debug = 1;
	else if (argv[ar][0] == '-') {
	    fprintf(stderr, "Unknown option '%s'\n", argv[ar]);
	    exit(1);
	} else if (fn == 0) {
	    fn = argv[ar];
	} else {
	    fprintf(stderr, "Too many arguments '%s'\n", argv[ar]);
	    exit(1);
	}
    }

    if(!(ifd = fn && strcmp(fn, "-") ? fopen(fn, "r") : stdin) )
    {
	perror(fn);
	exit(1);
    }

    while((ch = getc(ifd)) != EOF &&
	    (ifd != stdin || ch != '!' || j > 0 || !i))
    {
	if (ch == '<' || ch == '>' || ch == '+' || ch == '-')
	    ;
	else if (ch == '.' || ch == ',' || ch == '[' || ch == ']')
	{
	    if (ch == '[') j++;
	    if (ch == ']' && j == 0) continue;
	    if (ch == ']') j--;
	    if (ch == ',') i = 1;
	}
	else if (ch == '#' && debug)
	    ;
	else
	    continue;

	if (n+2 > pgmlen)
	{
	    pgm = realloc(pgm, (pgmlen = n+99)*sizeof *pgm);
	    if (!pgm) { perror("realloc"); exit(1); }
	}
	pgm[n++] = ch;
	pgm[n] = 0;
    }

    if (ifd != stdin) fclose(ifd);

    if (j>0) {
	fprintf(stderr, "Unbalanced [] at end of file\n");
	exit(1);
    }

    setbuf(stdout, NULL);
    if (pgm) run();

    return 0;
}

void run(void)
{
    int jmpstk[400], sp = 0;
#if ULONG_MAX > UINT_MAX
    static unsigned char t[UINT_MAX+1L];
    unsigned int m = 0;
#else
    static unsigned char t[USHRT_MAX+1];
    unsigned short m = 0;
#endif
    int n, ch, j=0;
    for(n = 0;; n++) {
	if (j==0)
	{
	    switch(pgm[n])
	    {
	    case 0:   return;
	    case '>': m += 1; break;
	    case '<': m -= 1; break;
	    case '+': t[m]++; break;
	    case '-': t[m]--; break;
	    case '.': putchar(t[m]); break;
	    case ',':
		if((ch = getchar()) != EOF) t[m] = ch;
		else if (on_eof != 1) t[m] = on_eof;
		break;

	    case '[':
		jmpstk[sp++] = n;
		if (!t[m])
		    j++;
		break;
	    case ']':
		if (t[m]) n=jmpstk[sp-1]; else sp--;
		break;

	    case '#':
		fprintf(stderr,
			"\n%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d\n%*s\n",
			t[0], t[1], t[2], t[3], t[4], t[5],
			t[6], t[7], t[8], t[9], 4*m+3, "^");
		break;
	    }
	} else {
	    switch(pgm[n])
	    {
	    case 0:   return;
	    case '[': j++; break;
	    case ']': j--; break;
	    }
	}
    }
}
