#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
 * Perl translation from BF, runs at about 14,000,000 instructions per second.
 */

extern int bytecell;

int do_input = 0;
int ind = 0;
#define I printf("%*s", ind*4, "")

int
check_arg(char * arg)
{
    return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!': printf("#!/usr/bin/perl\n$|++;\n$^W = 0;\n"); break;
    case '+': I; printf("$M[$P]+=%d;\n", count); break;
    case '-': I; printf("$M[$P]-=%d;\n", count); break;
    case '<': I; printf("$P-=%d;\n", count); break;
    case '>': I; printf("$P+=%d;\n", count); break;
    case '[':
	if(bytecell) { I; printf("$M[$P]&=255;\n"); }
	I; printf("while($M[$P]!=0){\n"); ind++; break;
    case ']':
	if(bytecell) { I; printf("$M[$P]&=255;\n"); }
	ind--; I; printf("}\n");
	break;
    case '.': I; printf("print chr($M[$P]&255);\n"); break;
    case ',': I; printf("$c=getc(STDIN); if(defined $c){ $M[$P] = ord($c); }\n"); break;
    }
}
