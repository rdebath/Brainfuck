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

static int ind = 0;
#define I printf("%*s", ind*4, "")

static gen_code_t gen_code;
struct be_interface_s be_interface = {.gen_code=gen_code};

static void print_cstring(char * str);

static void
gen_code(int ch, int count, char * strn)
{
    switch(ch) {
    case '!':
	if (!count)
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
    case 'T': I; printf("m[p] = m[p]-v\n"); break;
    case '*': I; printf("m[p] = m[p]*v\n"); break;
    case '/': I; printf("m[p] = $idiv(m[p], v)\n"); break;
    case '%': I; printf("m[p] = m[p] %% v\n"); break;

    case 'C': I; printf("m[p] = v*%d\n", count); break;
    case 'D': I; printf("m[p] = -v*%d\n", count); break;
    case 'V': I; printf("m[p] = v\n"); break;
    case 'W': I; printf("m[p] = -v\n"); break;

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
    case 'I':
	if(bytecell) { I; printf("m[p] &= 255\n"); }
	I; printf("if m[p] != 0 {\n");
	ind++;
	break;
    case 'E':
	ind--; I; printf("}\n");
	break;
    case '.': I; printf("$sset(s,0,m[p]) $print(s)\n"); break;
    case '"': print_cstring(strn); break;
    case ',':
	I; printf("try m[p] = file_read_char(file_stdin()) catch e 0\n");
	break;
    }
}

static void
print_cstring(char * str)
{
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
