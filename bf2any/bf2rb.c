#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
 * Ruby translation from BF, runs at about 31,000,000 instructions per second.
 */

extern int bytecell;

int do_input = 0;
int ind = 0;
#define I printf("%*s", ind*4, "")

int
check_arg(char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	printf( "%s",
		"#!/usr/bin/ruby\n"
		"m = Array.new(32768, 0)\n"
		"p = 0\n"
		);
	break;

    case '=': I; printf("m[p] = %d\n", count); break;
    case 'B': I; printf("v = m[p]\n"); break;
    case 'M': I; printf("m[p] = m[p]+v*%d\n", count); break;
    case 'N': I; printf("m[p] = m[p]-v*%d\n", count); break;
    case 'S': I; printf("m[p] = m[p]+v\n"); break;
    case 'Q': I; printf("m[p] = %d unless v == 0\n", count); break;
    case 'm': I; printf("m[p] = m[p]+v*%d unless v == 0\n", count); break;
    case 'n': I; printf("m[p] = m[p]-v*%d unless v == 0\n", count); break;
    case 's': I; printf("m[p] = m[p]+v unless v == 0\n"); break;

    case 'X': I; printf("print \"Abort: Infinite Loop.\\n\" ; exit\n"); break;

    case '+': I; printf("m[p] += %d\n", count); break;
    case '-': I; printf("m[p] -= %d\n", count); break;
    case '<': I; printf("p -= %d\n", count); break;
    case '>': I; printf("p += %d\n", count); break;
    case '[':
	if(bytecell) { I; printf("m[p] &= 255\n"); }
	I; printf("while m[p] != 0\n"); ind++; break;
    case ']':
	if(bytecell) { I; printf("m[p] &= 255\n"); }
	ind--; I; printf("end\n");
	break;
    case '.': I; printf("print m[p].chr\n"); break;
    case ',':
	I; printf("c = $stdin.getc\n");
	I; printf("m[p] = c.ord unless c == nil\n");
	break;
    }
}
