#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Julia translation from BF, runs at about 2,000,000,000 instructions per second.
 * Julia translation from BF, runs at about 20,000,000 instructions per second.
 *
 * But is VERY variable presumably due to the dynamic JIT.
 * Without the function it only manages 20,000,000 instructions per second.
 * (Probably because it only JITs functions)
 */

int ind = 0;
#define I printf("%*s", ind*4, "")

static void print_cstring(void);
static int no_function = 0;

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    if (strcmp(arg, "-savestring") == 0) return 1;
    if (strcmp(arg, "-nofunc") == 0) {
	no_function = 1;
	return 1;
    }
    if (strcmp("-h", arg) ==0) {
	fprintf(stderr, "%s\n",
	"\t"    "-nofunc Do NOT include a function wrapper (SLOW)"
	);
	return 1;
    }

    return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	if (!no_function) puts("function brainfuck()");
	printf( "%s%d%s",
		"m = zeros(typeof(0), 65536)\n"
		"p = ", BOFF, "\n");
	break;
    case '~':
	if (!no_function) puts("end\nbrainfuck()");
	break;

    case '=': I; printf("m[p] = %d\n", count); break;
    case 'B':
	if(bytecell) { I; printf("m[p] &= 255\n"); }
	I; printf("v = m[p]\n");
	break;
    case 'M': I; printf("m[p] = m[p]+v*%d\n", count); break;
    case 'N': I; printf("m[p] = m[p]-v*%d\n", count); break;
    case 'S': I; printf("m[p] = m[p]+v\n"); break;
    case 'Q': I; printf("if v != 0 ; m[p] = %d; end\n", count); break;
    case 'm': I; printf("if v != 0 ; m[p] = m[p]+v*%d; end\n", count); break;
    case 'n': I; printf("if v != 0 ; m[p] = m[p]-v*%d; end\n", count); break;
    case 's': I; printf("if v != 0 ; m[p] = m[p]+v; end\n"); break;

    case 'X': I; printf("error(\"Aborting Infinite Loop.\")\n"); break;

    case '+': I; printf("m[p] += %d\n", count); break;
    case '-': I; printf("m[p] -= %d\n", count); break;
    case '<': I; printf("p -= %d\n", count); break;
    case '>': I; printf("p += %d\n", count); break;
    case '[':
	if(bytecell) { I; printf("m[p] &= 255\n"); }
	I; printf("while m[p] != 0\n");
	ind++;
	break;
    case ']':
	if(bytecell) { I; printf("m[p] &= 255\n"); }
	ind--; I; printf("end\n");
	break;
    case '.': I; printf("print(char(m[p]))\n"); break;
    case '"': print_cstring(); break;
    case ',': I; printf("m[p] = read(STDIN, Char)\n"); break;
    }
}

static void
print_cstring(void)
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
		I; printf("println(\"%s\")\n", buf);
	    } else {
		I; printf("print(\"%s\")\n", buf);
	    }
	    gotnl = 0; outlen = 0;
	}
	if (!*str) break;

	if (*str == '\n') gotnl = 1;
	if (*str == '"' || *str == '\\' || *str == '$') {
	    buf[outlen++] = '\\'; buf[outlen++] = *str;
	} else if (*str >= ' ' && *str <= '~') {
	    buf[outlen++] = *str;
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
