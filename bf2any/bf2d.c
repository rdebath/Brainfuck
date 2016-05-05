#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * GDC "D" translation from BF, runs at about 1,600,000,000 instructions per second.
 */

int ind = 0;
FILE * ofd;
#define pr(s)           fprintf(ofd, "%*s" s "\n", ind*4, "")
#define prv(s,v)        fprintf(ofd, "%*s" s "\n", ind*4, "", (v))

static void print_dstring(void);

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    if (strcmp(arg, "-savestring") == 0) return 1;
    if (strcmp(arg, "-intcells") == 0) return 1;
    return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	ofd = stdout;

	pr("module fuck;");	/* Required to prevent really stupid error messages. */

	pr("import std.stdio;");
	pr("void main(){");
	ind++;
	if (bytecell) {
	    prv("char[] mem; mem.length = %d; mem[] = 0;", tapesz);
	    prv("int m = %d;", tapeinit);
	    pr("int v;");
	} else {
	    prv("int[] mem; mem.length = %d; mem[] = 0;", tapesz);
	    prv("int v, m = %d;", tapeinit);
	}
	pr("stdout.setvbuf(0, _IONBF);");
	break;

    case '~':
	ind--;
	pr("}");
	break;

    case 'X': pr("throw new Exception(\"Trivial Infinite Loop Hit.\");"); break;

    case '=': prv("mem[m] = %d;", count); break;
    case 'B': pr("v = mem[m];"); break;
    case 'M': prv("mem[m] += v*%d;", count); break;
    case 'N': prv("mem[m] -= v*%d;", count); break;
    case 'S': pr("mem[m] += v;"); break;
    case 'Q': prv("if(v) mem[m] = %d;", count); break;
    case 'm': prv("if(v) mem[m] += v*%d;", count); break;
    case 'n': prv("if(v) mem[m] -= v*%d;", count); break;
    case 's': pr("if(v) mem[m] += v;"); break;

    case '+': prv("mem[m] +=%d;", count); break;
    case '-': prv("mem[m] -=%d;", count); break;
    case '>': prv("m += %d;", count); break;
    case '<': prv("m -= %d;", count); break;

    case '.': pr("write(cast(char)mem[m]);"); break;
    case '"': print_dstring(); break;
    case ',': pr("v = getchar(); if(v!=EOF) mem[m] = v;"); break;

    case '[': pr("while(mem[m]) {"); ind++; break;
    case ']': ind--; pr("}"); break;
    }
}

static void
print_dstring(void)
{
    char * str = get_string();
    char buf[256];
    int gotnl = 0;
    size_t outlen = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || gotnl || outlen > sizeof(buf)-8))
	{
	    buf[outlen] = 0;
	    if (gotnl) {
		buf[outlen-2] = 0;
		prv("writeln(\"%s\");", buf);
	    } else
		prv("write(\"%s\");", buf);
	    gotnl = 0; outlen = 0;
	}
	if (!*str) break;

	if (*str == '\n') gotnl = 1;
	if (*str >= ' ' && *str <= '~' && *str != '"' && *str != '\\') {
	    buf[outlen++] = *str;
	} else if (*str == '"' || *str == '\\') {
	    buf[outlen++] = '\\'; buf[outlen++] = *str;
	} else if (*str == '\n') {
	    buf[outlen++] = '\\'; buf[outlen++] = 'n';
	} else if (*str == '\t') {
	    buf[outlen++] = '\\'; buf[outlen++] = 't';
	} else {
	    char buf2[16];
	    int n;
	    sprintf(buf2, "\\%03o", *str & 0xFF);
	    for(n=0; buf2[n]; n++)
		buf[outlen++] = buf2[n];
	}
    }
}
