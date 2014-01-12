#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Lua translation from BF, runs at about 39,000,000 instructions per second.
 *
 * There is a limit on the size of the final script imposed by the interpreter.
 */

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
	printf("#!/usr/bin/lua\n");
	printf("local m = setmetatable({},{__index=function() return 0 end})\n");
	printf("local p = %d\n", BOFF);
	printf("local v = 0\n");
	printf("\n");
	printf("function brainfuck()\n");
	ind++;
	break;

    case '=': I; printf("m[p] = %d\n", count); break;
    case 'B':
	if(bytecell) { I; printf("m[p] = m[p]%%256\n"); }
	I; printf("v = m[p]\n");
	break;
    case 'M': I; printf("m[p] = m[p]+v*%d\n", count); break;
    case 'N': I; printf("m[p] = m[p]-v*%d\n", count); break;
    case 'S': I; printf("m[p] = m[p]+v\n"); break;
    case 'Q': I; printf("if v ~= 0 then m[p] = %d end\n", count); break;
    case 'm': I; printf("if v ~= 0 then m[p] = m[p]+v*%d end\n", count); break;
    case 'n': I; printf("if v ~= 0 then m[p] = m[p]-v*%d end\n", count); break;
    case 's': I; printf("if v ~= 0 then m[p] = m[p]+v end\n"); break;

    case 'X': I; printf("error('Aborting Infinite Loop')\n"); break;

    case '+': I; printf("m[p] = m[p]+%d\n", count); break;
    case '-': I; printf("m[p] = m[p]-%d\n", count); break;
    case '>': I; printf("p = p+%d\n", count); break;
    case '<': I; printf("p = p-%d\n", count); break;
    case '.': I; printf("putch()\n"); break;
    case ',': I; printf("getch()\n"); do_input++; break;

    case '[':
	if(bytecell) { I; printf("m[p] = m[p]%%256\n"); }
	I; printf("while m[p]~=0 do\n");
	ind++;
	break;
    case ']':
	if(bytecell) { I; printf("m[p] = m[p]%%256\n"); }
	ind--; I; printf("end\n");
	break;

    case '~':
	ind--;
	printf("end\n");

	if (do_input) {
	    printf("\n");
	    printf("function getch()\n");
	    printf("    local Char = io.read(1)\n");
	    printf("    if Char then\n");
	    printf("        m[p] = string.byte(Char)\n");
	    printf("    end\n");
	    printf("end\n");
	}

	printf("\n");
	printf("function putch()\n");
	printf("    io.write(string.char(m[p]%%256))\n");
	printf("    io.flush()\n");
	printf("end\n");

	printf("\n");
	printf("brainfuck()\n");
	break;
    }
}
