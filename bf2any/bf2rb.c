#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Ruby translation from BF, runs at about 31,000,000 instructions per second.
 */

int ind = 0;
#define I printf("%*s", ind*4, "")
int safetapeoff = 0, curtapeoff = 0;

static void print_cstring(void);

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    if (strcmp(arg, "-savestring") == 0) return 1;
    if (strcmp(arg, "-M") == 0) {
	tapelen = 0;
	return 1;
    }
    return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	puts("#!/usr/bin/ruby");
	/* Using push is about 20% slower, using a Hash is a LOT slower! */
	if (tapelen > 0)
	    printf("m = Array.new(%d, 0)\n",tapesz);
	else {
	    puts("m = Array.new");
	}
	printf("%s%d%s", "p = ", tapeinit, "\n");
	if (tapelen <= 0)
	    puts("m.push(0) while p>=m.length");
	break;

    case '=': I; printf("m[p] = %d\n", count); break;
    case 'B':
	if(bytecell) { I; printf("m[p] &= 255\n"); }
	I; printf("v = m[p]\n");
	break;
    case 'M': I; printf("m[p] = m[p]+v*%d\n", count); break;
    case 'N': I; printf("m[p] = m[p]-v*%d\n", count); break;
    case 'S': I; printf("m[p] = m[p]+v\n"); break;
    case 'Q': I; printf("m[p] = %d unless v == 0\n", count); break;
    case 'm': I; printf("m[p] = m[p]+v*%d unless v == 0\n", count); break;
    case 'n': I; printf("m[p] = m[p]-v*%d unless v == 0\n", count); break;
    case 's': I; printf("m[p] = m[p]+v unless v == 0\n"); break;

    case 'X': I; printf("raise 'Aborting Infinite Loop.'\n"); break;

    case '+': I; printf("m[p] += %d\n", count); break;
    case '-': I; printf("m[p] -= %d\n", count); break;
    case '<':
	I; printf("p -= %d\n", count);
	curtapeoff -= count;
	break;
    case '>':
	I; printf("p += %d\n", count);
	curtapeoff += count;
	if (tapelen <= 0 && curtapeoff > safetapeoff) {
	    safetapeoff = curtapeoff;
	    I; puts("m.push(0) while p>=m.length");
	}
	break;
    case '[':
	if(bytecell) { I; printf("m[p] &= 255\n"); }
	I; printf("while m[p] != 0\n");
	ind++;
	curtapeoff = safetapeoff = 0;
	break;
    case ']':
	if(bytecell) { I; printf("m[p] &= 255\n"); }
	ind--; I; printf("end\n");
	curtapeoff = safetapeoff = 0;
	break;
    case '"': print_cstring(); break;
    case '.':
	I;
	if(bytecell) printf("putc m[p]\n");
	else         printf("begin print ''<<m[p] rescue putc m[p] end\n");
	break;
    case ',':
	I;
	if (bytecell) printf("m[p] = $stdin.getbyte if !$stdin.eof\n");
	else          printf("m[p] = $stdin.getc.ord if !$stdin.eof\n");
	break;
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
		I; printf("puts \"%s\"\n", buf);
	    } else {
		I; printf("print \"%s\"\n", buf);
	    }
	    gotnl = 0; outlen = 0;
	}
	if (!*str) break;

	if (*str == '\n') gotnl = 1;
	if (*str == '"' || *str == '\\' || *str == '#') {
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
