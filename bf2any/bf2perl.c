#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Perl translation from BF, runs at about 14,000,000 instructions per second.
 */

int ind = 0;
#define I printf("%*s", ind*4, "")

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    return 0;
}

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
    }
}
