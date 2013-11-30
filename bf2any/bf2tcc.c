#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libtcc.h>

/*
 * TCC translation from BF, runs at about 1,200,000,000 instructions per second.
 *
 * Much faster compiled with GCC, even with -O0
 *
 * BCC translation from BF, runs at about 3,000,000,000 instructions per second.
 * GCC-O0 translation from BF, runs at about 2,700,000,000 instructions per second.
 * GCC-O2 translation from BF, runs at about 4,300,000,000 instructions per second.
 */

extern int bytecell;

int ind = 0;
int runmode = 0;
FILE * ofd;
#define pr(s)           fprintf(ofd, "%*s" s "\n", ind*4, "")
#define prv(s,v)        fprintf(ofd, "%*s" s "\n", ind*4, "", (v))
#define prv2(s,v,v2)    fprintf(ofd, "%*s" s "\n", ind*4, "", (v), (v2))

char * ccode = 0;
size_t ccodesize = 0;
int imov = 0;

int
check_arg(char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    else if (strcmp(arg, "-r") == 0) {
	runmode=1; return 1;
    }
    return 0;
}

void
outcmd(int ch, int count)
{
    if (imov && ch != '>' && ch != '<') {
	int mov = imov;
	imov = 0;

	switch(ch) {
	case '=': prv2("*(m+=%d) = %d;", mov, count); return;
	case '+': prv2("*(m+=%d) +=%d;", mov, count); return;
	case '-': prv2("*(m+=%d) -=%d;", mov, count); return;
	case 'B': prv("v=*(m+=%d);", mov); return;
	case 'M': prv2("*(m+=%d) += v*%d;", mov, count); return;
	case 'N': prv2("*(m+=%d) -= v*%d;", mov, count); return;
	case 'S': prv("*(m+=%d) += v;", mov); return;
	}

	if (mov > 0)
	    prv("m += %d;", mov);
	else if (mov < 0)
	    prv("m -= %d;", -mov);
    }

    switch(ch) {
    case '!':
	if (runmode) {
	    ofd = open_memstream(&ccode, &ccodesize);
	} else
	    ofd = stdout;

	pr("#!/usr/bin/tcc -run");
	pr("#include <stdio.h>");
	pr("int main(int argc, char ** argv){");
	ind++;
	if (bytecell) {
	    pr("static char mem[30000];");
	    prv("register char *m = mem + %d;", BOFF);
	} else {
	    pr("static int mem[30000];");
	    prv("register int *m = mem + %d;", BOFF);
	}
	pr("register int v;");
	pr("setbuf(stdout,0);");
	break;

    case 'X': pr("fprintf(stderr, \"Infinite Loop\\n\"); exit(1);"); break;

    case '=': prv("*m = %d;", count); break;
    case 'B': pr("v = *m;"); break;
    case 'M': prv("*m += v*%d;", count); break;
    case 'N': prv("*m -= v*%d;", count); break;
    case 'S': pr("*m += v;"); break;
    case 'Q': prv("if(v) *m = %d;", count); break;
    case 'm': prv("if(v) *m += v*%d;", count); break;
    case 'n': prv("if(v) *m -= v*%d;", count); break;
    case 's': pr("if(v) *m += v;"); break;

    case '+': prv("*m +=%d;", count); break;
    case '-': prv("*m -=%d;", count); break;
    case '>': imov += count; break;
    case '<': imov -= count; break;
    case '.': pr("putchar(*m);"); break;
    case ',': pr("v = getchar(); if(v!=EOF) *m = v;"); break;

    case '[': pr("while(*m) {"); ind++; break;
    case ']': ind--; pr("}"); break;
    }

    if (ch != '~') return;
    pr("return 0;\n}");

    if (!runmode) return;

    putc('\0', ofd);
    fclose(ofd);

    {
	TCCState *s;
	int rv;

	s = tcc_new();
	if (s == NULL) { perror("tcc_new()"); exit(7); }
	tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
	tcc_compile_string(s, ccode);

	rv = tcc_run(s, 0, 0);
	if (rv) fprintf(stderr, "tcc_run returned %d\n", rv);
	tcc_delete(s);
	free(ccode);
    }
}
