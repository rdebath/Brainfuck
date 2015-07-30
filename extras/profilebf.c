#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <limits.h>
#if _POSIX_VERSION >= 199506L || defined(LLONG_MAX)
#include <inttypes.h>
#else
#define intmax_t long
#define PRIdMAX  "ld"
#endif

#define TAPELEN		(2<<16)
#define TM(x)		((x)&(TAPELEN-1))

int main(int argc, char **argv) {
static int mem[TAPELEN];
static char bf[] = "><+-][.,#";
static intmax_t profile[sizeof(bf)*3+3];
static intmax_t overflows, underflows;
    int tape_min = 0, tape_max = 0;
    int program_len = 0;
    int nonl = 0;
    int neg_array = 0;

    char *pgm = 0, *c;
    int *jmp;
    int m=0;
    int maxp=0, p=0, ch;
    int on_eof = 1, debug = 0;
    int physical_overflow = 0;
    int quick_summary = 0;
    int quick_with_counts = 0;
    int hard_wrap = 0;
    int cell_mask = 255;

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
	} else if (!strcmp(argv[1], "-w")) {
	    cell_mask=65535; argc--; argv++;
	} else if (!strcmp(argv[1], "-p")) {
	    physical_overflow++; argc--; argv++;
	} else if (!strcmp(argv[1], "-P")) {
	    physical_overflow=2; argc--; argv++;
	} else if (!strcmp(argv[1], "-q")) {
	    quick_summary=1; argc--; argv++;
	} else if (!strcmp(argv[1], "-Q")) {
	    quick_summary=1; quick_with_counts=1; argc--; argv++;
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

    if(!pgm) return 1;

    program_len = p;
    pgm[p++] = 0;

    if (f!=stdin) fclose(f);
    jmp = calloc(maxp, sizeof*jmp);
    if(!jmp) { perror(argv[0]); exit(1); }
    setbuf(stdout, 0);

    for(p=0;pgm[p];p++) {
	profile[pgm[p]*3 + !!mem[TM(m)]]++;
	switch(pgm[p]) {
	case 1: m++; if(tape_max<m) tape_max = m; break;
	case 2: m--; if(tape_min>m) tape_min = m; break;
	case 3:
	    if (mem[TM(m)] == cell_mask) hard_wrap = 1;
	    mem[TM(m)]++;
	    if(physical_overflow) {
		if (mem[TM(m)] > cell_mask) { overflows++; mem[TM(m)] &= cell_mask; profile[pgm[p]*3 + 2]++; }
	    } else {
		/* Even if we're checking on '[' it's possible for our "int" cell to overflow; trap that here. */
		if(mem[TM(m)] > 0x3FFFFFFF)
		    { overflows++; mem[TM(m)] &= cell_mask; profile[pgm[p]*3 + 2]++; }
	    }
	    if(m<0) neg_array = 1;
	    break;
	case 4:
	    if (mem[TM(m)] == 0) hard_wrap = 1;
	    mem[TM(m)]--;
	    if(physical_overflow) {
		if (mem[TM(m)] < -cell_mask/2 || (physical_overflow>1 && mem[TM(m)] < 0))
		    { underflows++; mem[TM(m)] &= cell_mask; profile[pgm[p]*3 + 2]++; }
	    } else {
		if(mem[TM(m)] < -0x3FFFFFFF)
		    { underflows++; mem[TM(m)] &= cell_mask; profile[pgm[p]*3 + 2]++; }
	    }
	    if(m<0) neg_array = 1;
	    break;
	case 5:
	    if ((mem[TM(m)] & cell_mask) == 0 && (mem[TM(m)] & ~cell_mask) != 0) {
		if (mem[TM(m)] > 0) overflows++; else underflows++;
		mem[TM(m)] = 0;
		profile[pgm[p]*3 + 2]++;
	    }

	    if(mem[TM(m)]) p += jmp[p];
	    if(m<0) neg_array = 1;
	    break;
	case 6:
	    if ((mem[TM(m)] & cell_mask) == 0 && (mem[TM(m)] & ~cell_mask) != 0) {
		if (mem[TM(m)] > 0) overflows++; else underflows++;
		mem[TM(m)] = 0;
		profile[pgm[p]*3 + 2]++;
	    }

	    if(!jmp[p]) {
		int j = p, l=1;
		while(pgm[j] && l){ j++;l+=(pgm[j]==6)-(pgm[j]==5); }
		if(!pgm[j]) break;
		jmp[p] = j-p; jmp[j] = p-j;
	    }
	    if(!mem[TM(m)]) p += jmp[p];
	    if(m<0) neg_array = 1;
	    break;
	case 7:
	    if (mem[TM(m)] > cell_mask || mem[TM(m)] < 0) {
		if (mem[TM(m)] > 0) overflows++; else underflows++;
		mem[TM(m)] &= cell_mask;
		profile[pgm[p]*3 + 2]++;
	    }

	    {	int a=(mem[TM(m)] & 0xFF);
		putchar(a);
		if (a != 13) nonl=(a != '\n');
	    }
	    if(m<0) neg_array = 1;
	    break;
	case 8:
	    if (mem[TM(m)] > cell_mask || mem[TM(m)] < 0) {
		if (mem[TM(m)] > 0) overflows++; else underflows++;
		mem[TM(m)] &= cell_mask;
		profile[pgm[p]*3 + 2]++;
	    }

	    {   int a=getchar();
		if(a!=EOF) mem[TM(m)]=a;
		else if (on_eof != 1) mem[TM(m)] = on_eof;
	    }
	    if(m<0) neg_array = 1;
	    break;
	case 9:
	    {
		printf("%2d %2d %2d %2d %2d %2d %2d %2d %2d %2d\n",
		   mem[0],mem[1],mem[2],mem[3],mem[4],mem[5],
		   mem[6],mem[7],mem[8],mem[9]);

		if (m>=0 && m<10) printf("%*s\n", 3*m+2,"^");
	    }
	}
    }

    if (!quick_summary)
    {
	if (nonl) fprintf(stderr, "\n\\ no newline at end of output.\n");

	fprintf(stderr, "Program length %d\n", program_len);
	fprintf(stderr, "Final tape contents:\n");

	{
	    int cc = 0, pc = 0;
	    if (tape_min<0) cc+=fprintf(stderr, " !");
	    for(ch=0; ch<16 && ch<=tape_max-tape_min; ch++) {
		if (ch+tape_min == 0) cc+=fprintf(stderr, " :");
		cc += fprintf(stderr, " %3d", mem[TM(ch+tape_min)] & cell_mask);
		if (m == ch+tape_min)
		    pc = cc;
	    }
	    if (tape_max-tape_min>=16) fprintf(stderr, " ...");
	    if (pc) fprintf(stderr, "\n%*s\n", pc, "^");
	    else fprintf(stderr, "\nPointer at: %d\n", m);
	}

	if (tape_min<0) {
	    if (neg_array)
		fprintf(stderr, "WARNING: Tape pointer minimum %d, segfault.\n", tape_min);
	    else
		fprintf(stderr, "WARNING: Tape pointer minimum %d, pointer only.\n", tape_min);
	}
	fprintf(stderr, "Tape pointer maximum %d\n", tape_max);
	if (tape_max>= TAPELEN)
	    fprintf(stderr, "WARNING: Invalid result, pointer has wrapped at %d\n", TAPELEN);

	if (overflows || underflows) {
	    fprintf(stderr, "Range error: ");
	    if (physical_overflow>1)
		fprintf(stderr, "range 0..%d", cell_mask);
	    else if (physical_overflow)
		fprintf(stderr, "range %d..%d", -cell_mask/2, cell_mask);
	    else
		fprintf(stderr, "value check");
	    if (overflows)
		fprintf(stderr, ", overflows: %"PRIdMAX"", overflows);
	    if (underflows)
		fprintf(stderr, ", underflows: %"PRIdMAX"", underflows);
	    fprintf(stderr, "\n");
	} else if (hard_wrap)
	    fprintf(stderr, "Hard wrapping would occur.\n");

	fprintf(stderr, "%-5s %9s %12s %12s %12s\n",
		"Token","Total","Zero cell","Non-zero","Range Err");

	for(ch = 0; ch < 8; ch++) {
	    if (profile[(ch+1)*3]+ profile[(ch+1)*3+1] == 0)
		;
	    else {
		fprintf(stderr, "%c: %12"PRIdMAX"", bf[ch],
			profile[(ch+1)*3]+ profile[(ch+1)*3+1]);

		if (bf[ch] == '>' || bf[ch] == '<' || bf[ch] == '.') {
		    if (profile[(ch+1)*3+2] != 0) {
			fprintf(stderr, " %12s %12s", "","");
			fprintf(stderr, " %12"PRIdMAX"", profile[(ch+1)*3+2]);
		    }
		} else {
		    fprintf(stderr, " %12"PRIdMAX" %12"PRIdMAX"",
			    profile[(ch+1)*3], profile[(ch+1)*3+1]);
		    fprintf(stderr, " %12"PRIdMAX"", profile[(ch+1)*3+2]);
		}
		fprintf(stderr, "\n");
	    }

	    profile[0] += profile[(ch+1)*3]+ profile[(ch+1)*3+1];
	}
	fprintf(stderr, "   %12"PRIdMAX"\n", profile[0]);
    }
    else
    {
	if (tape_min < -42 || tape_max > 127) {
	    fprintf(stderr, "ERROR ");
	    for(p=0;pgm[p];p++)
		fprintf(stderr, "%c", pgm[p][" ><+-][.,#"]);
	    fprintf(stderr, "\n");
	} else {
	    int nonwrap =
		(overflows==0 && underflows==0 && (mem[TM(m)] & ~cell_mask) == 0);
	    for(ch = 0; ch < 8; ch++) {
		profile[0] += profile[(ch+1)*3]+ profile[(ch+1)*3+1];
	    }
	    fprintf(stderr, "%d ", (mem[TM(m)] & cell_mask) );
	    fprintf(stderr, "%d ", program_len-tape_min);
	    fprintf(stderr, "%d ", tape_max-tape_min+1);
	    fprintf(stderr, "%"PRIdMAX" ", profile[0]);
	    for(p=tape_min; p<0; p++)
		fprintf(stderr, ">");
	    for(p=0;pgm[p];p++)
		fprintf(stderr, "%c", pgm[p][" ><+-][.,#"]);

	    if (quick_with_counts)
		fprintf(stderr, " (%d, %d, %"PRIdMAX") %swrapping%s\n",
			program_len-tape_min, tape_max-tape_min+1,
			profile[0]-tape_min,
			nonwrap?"non-":"",
			nonwrap && hard_wrap?" (soft)":"");
	    else
		fprintf(stderr, " (%d, %d) %swrapping%s\n",
			program_len-tape_min, tape_max-tape_min+1,
			nonwrap?"non-":"",
			nonwrap && hard_wrap?" (soft)":"");
	}
    }
    return 0;
}
