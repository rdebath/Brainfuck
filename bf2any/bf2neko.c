#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Neko translation from BF, runs at about 170,000,000 instructions per second.
 *
 * For large source it generates the error
 *      "Uncaught exception - load.c(393) : Invalid module : ..."
 */

int ind = 0;
#define I printf("%*s", ind*4, "")

static void print_cstring(void);

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	printf( "%s%d%s%d%s",
	    "var m = $amake(",tapesz,");\n"
	    "var p = 0;\n"
	    "var s = $smake(1);\n"
	    "var file_read_char = $loader.loadprim(\"std@file_read_char\",1)\n"
	    "var file_stdin = $loader.loadprim(\"std@file_stdin\",0)\n"
	    "while p < $asize(m) {m[p] = 0; p+=1 } p = ",tapeinit,";\n"
	    );
	break;

    case '=': I; printf("m[p] = %d\n", count); break;
    case 'B':
	if(bytecell) { I; printf("m[p] &= 255\n"); }
	I; printf("v = m[p]\n");
	break;
    case 'M': I; printf("m[p] = m[p]+v*%d\n", count); break;
    case 'N': I; printf("m[p] = m[p]-v*%d\n", count); break;
    case 'S': I; printf("m[p] = m[p]+v\n"); break;
    case 'Q': I; printf("if v != 0 m[p] = %d\n", count); break;
    case 'm': I; printf("if v != 0 m[p] = m[p]+v*%d\n", count); break;
    case 'n': I; printf("if v != 0 m[p] = m[p]-v*%d\n", count); break;
    case 's': I; printf("if v != 0 m[p] = m[p]+v\n"); break;

    case 'X': I; printf("$throw(\"Abort Infinite Loop\");\n"); break;

    case '+': I; printf("m[p] += %d\n", count); break;
    case '-': I; printf("m[p] -= %d\n", count); break;
    case '<': I; printf("p -= %d\n", count); break;
    case '>': I; printf("p += %d\n", count); break;
    case '[':
	if(bytecell) { I; printf("m[p] &= 255\n"); }
	I; printf("while m[p] != 0 {\n");
	ind++;
	break;
    case ']':
	if(bytecell) { I; printf("m[p] &= 255\n"); }
	ind--; I; printf("}\n");
	break;
    case '.': I; printf("$sset(s,0,m[p]) $print(s)\n"); break;
    case '"': print_cstring(); break;
    case ',':
	I; printf("try m[p] = file_read_char(file_stdin()) catch e 0\n");
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
	    I; printf("$print(\"%s\")\n", buf);
	    gotnl = 0; outlen = 0;
	}
	if (!*str) break;

	if (*str == '\n') gotnl = 1;
	if (*str == '"' || *str == '\\') {
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
	    sprintf(buf2, "\\%03d", *str & 0xFF); /* <--- DECIMAL!! */
	    for(n=0; buf2[n]; n++)
		buf[outlen++] = buf2[n];
	}
    }
}
