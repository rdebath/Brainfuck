#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Perl translation from BF, runs at about 14,000,000 instructions per second.
 */

int ind = 0;
#define I printf("%*s", ind*4, "")

static void print_cstring(void);

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	printf( "%s%d%s",
		"#!/usr/bin/perl\n"
		"$|++;\n" "$^W = 0;\n"
		"$p = ", tapeinit, ";\n");
	break;

    case '=': I; printf("$m[$p] = %d;\n", count); break;
    case 'B':
	if(bytecell) { I; printf("$m[$p] &= 255;\n"); }
	I; printf("$v = $m[$p];\n");
	break;
    case 'M': I; printf("$m[$p] = $m[$p]+$v*%d;\n", count); break;
    case 'N': I; printf("$m[$p] = $m[$p]-$v*%d;\n", count); break;
    case 'S': I; printf("$m[$p] = $m[$p]+$v;\n"); break;
    case 'Q': I; printf("$m[$p] = %d unless $v == 0;\n", count); break;
    case 'm': I; printf("$m[$p] = $m[$p]+$v*%d unless $v == 0;\n", count); break;
    case 'n': I; printf("$m[$p] = $m[$p]-$v*%d unless $v == 0;\n", count); break;
    case 's': I; printf("$m[$p] = $m[$p]+$v unless $v == 0;\n"); break;

    case 'X': I; printf("die(\"Abort: Infinite Loop.\\n\");\n"); break;

    case '+': I; printf("$m[$p] += %d;\n", count); break;
    case '-': I; printf("$m[$p] -= %d;\n", count); break;
    case '<': I; printf("$p -= %d;\n", count); break;
    case '>': I; printf("$p += %d;\n", count); break;
    case '[':
	if(bytecell) { I; printf("$m[$p] &= 255;\n"); }
	I; printf("while($m[$p] != 0){\n");
	ind++;
	break;
    case ']':
	if(bytecell) { I; printf("$m[$p] &= 255;\n"); }
	ind--; I; printf("}\n");
	break;
    case '.': I; printf("print chr($m[$p]&255);\n"); break;
    case ',': I; printf("$c = getc(STDIN); if(defined $c){ $m[$p] = ord($c); }\n"); break;
    case '"': print_cstring(); break;
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
	    I; printf("print \"%s\";\n", buf);
	    gotnl = 0; outlen = 0;
	}
	if (!*str) break;

	if (*str == '\n') gotnl = 1;
	if (*str == '"' || *str == '\\' || *str == '@' || *str == '$') {
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
