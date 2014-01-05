#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Python translation from BF, runs at about 18,000,000 instructions per second.
 *
 * There is a limit on the number of nested loops of 20.
 */

/* #define USESYS // else USEOS */

int do_input = 0;
int ind = 0;
#define I printf("%*s", ind*4, "")
int lastcmd = 0;

int
check_arg(char * arg)
{
    return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	printf("#!/usr/bin/python\n");
#ifdef USESYS
	printf("import sys\n");
#else
	printf("import os\n");
#endif
	printf("m = [0] * 60000\n");
	printf("i = 0\n");
	break;
    case '+': I; printf("m[i] += %d\n", count); break;
    case '-': I; printf("m[i] -= %d\n", count); break;
    case '<': I; printf("i -= %d\n", count); break;
    case '>': I; printf("i += %d\n", count); break;
    case '[':
	if(bytecell) { I; printf("m[i] &= 255\n"); }
	I; printf("while m[i] :\n"); ind++; break;
    case ']':
	if(bytecell) {
	    I; printf("m[i] &= 255\n");
	} else if (lastcmd == '[') {
	    I; printf("pass\n");
	}
	ind--;
	break;

#ifdef USESYS
    case '.': I; printf("sys.stdout.write(chr(m[i]&255))\n"); break;
    case ',':
	I; printf("c = sys.stdin.read(1);\n");
	I; printf("if c != '' :\n");
	ind++; I; ind--; printf("m[i] = ord(c)\n");
	break;
#else
    case '.': I; printf("os.write(1, chr(m[i]&255))\n"); break;
    case ',':
	I; printf("c = os.read(0, 1);\n");
	I; printf("if c != '' :\n");
	ind++; I; ind--; printf("m[i] = ord(c)\n");
	break;
#endif
    }
    lastcmd = ch;
}
