#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * GDC "D" translation from BF, runs at about 1,600,000,000 instructions per second.
 */

static int ind = 0;
#define pr(s)           printf("%*s" s "\n", ind*4, "")
#define prv(s,v)        printf("%*s" s "\n", ind*4, "", (v))
#define prv2(s,v,v2)    printf("%*s" s "\n", ind*4, "", (v), (v2))

static void print_dstring(char * str);
static const char * dumbcast = "";

static gen_code_t gen_code;
/* Assume same machine, same int */
struct be_interface_s be_interface = {.gen_code=gen_code, .cells_are_ints=1};

static void
gen_code(int ch, int count, char * strn)
{
    switch(ch) {
    case '!':
	pr("module fuck;");	/* Required to prevent really stupid error messages. */

	pr("import std.stdio;");
	pr("void main(){");
	ind++;
	if (!count) {
	    if (bytecell) {
		prv("char[] mem; mem.length = %d; mem[] = 0;", tapesz);
		prv("int m = %d;", tapeinit);
		pr("int v;");
		dumbcast = "cast(char) ";
	    } else {
		prv("uint[] mem; mem.length = %d; mem[] = 0;", tapesz);
		pr("uint v;");
		prv("int m = %d;", tapeinit);
	    }
	    pr("stdout.setvbuf(0, _IONBF);");
	}
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
    case 'T': pr("mem[m] -= v;"); break;
    case '*': pr("mem[m] *= v;"); break;
    case '/': pr("mem[m] /= v;"); break;
    case '%': pr("mem[m] %%= v;"); break;

    case 'C': prv2("mem[m] = %s(v*%d);", dumbcast, count); break;
    case 'D': prv2("mem[m] = %s(-v*%d);", dumbcast, count); break;
    case 'V': prv("mem[m] = %sv;", dumbcast); break;
    case 'W': prv("mem[m] = %s(-v);", dumbcast); break;

    case '+': prv("mem[m] +=%d;", count); break;
    case '-': prv("mem[m] -=%d;", count); break;
    case '>': prv("m += %d;", count); break;
    case '<': prv("m -= %d;", count); break;

    case '.': pr("write(cast(char)mem[m]);"); break;
    case '"': print_dstring(strn); break;
    case ',': prv("v = getchar(); if(v!=EOF) mem[m] = %sv;", dumbcast); break;

    case '[': pr("while(mem[m]) {"); ind++; break;
    case ']': ind--; pr("}"); break;

    case 'I': pr("if(mem[m]) {"); ind++; break;
    case 'E': ind--; pr("}"); break;
    }
}

static void
print_dstring(char * str)
{
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
