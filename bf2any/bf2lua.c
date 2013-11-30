#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
 * Lua translation from BF, runs at about 39,000,000 instructions per second.
 *
 * There is a limit on the size of the final script imposed by the interpreter.
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
	printf("#!/usr/bin/lua\n");
	printf("local m = setmetatable({},{__index=function() return 0 end})\n");
	printf("local i = %d\n", BOFF);
	printf("local v = 0\n");
	printf("\n");
	printf("function brainfuck()\n");
	ind++;
	break;

    case '=': I; printf("m[i] = %d\n", count); break;
    case 'B': I; printf("v = m[i]\n"); break;
    case 'M': I; printf("m[i] = m[i]+v*%d\n", count); break;
    case 'N': I; printf("m[i] = m[i]-v*%d\n", count); break;
    case 'S': I; printf("m[i] = m[i]+v\n"); break;
    case 'Q': I; printf("if v ~= 0 then m[i] = %d end\n", count); break;
    case 'm': I; printf("if v ~= 0 then m[i] = m[i]+v*%d end\n", count); break;
    case 'n': I; printf("if v ~= 0 then m[i] = m[i]-v*%d end\n", count); break;
    case 's': I; printf("if v ~= 0 then m[i] = m[i]+v end\n"); break;

    case 'X': I; printf("io.write(\"Infinite Loop\\n\") os.exit(1)\n"); break;

    case '+': I; printf("m[i] = m[i]+%d\n", count); break;
    case '-': I; printf("m[i] = m[i]-%d\n", count); break;
    case '>': I; printf("i = i+%d\n", count); break;
    case '<': I; printf("i = i-%d\n", count); break;
    case '.': I; printf("putch()\n"); break;
    case ',': I; printf("getch()\n"); do_input++; break;

    case '[':
	if(bytecell) { I; printf("m[i] = m[i]%%256\n"); }
	I; printf("while m[i]~=0 do\n");
	ind++;
	break;
    case ']':
	if(bytecell) { I; printf("m[i] = m[i]%%256\n"); }
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
	    printf("        m[i] = string.byte(Char)\n");
	    printf("    end\n");
	    printf("end\n");
	}

	printf("\n");
	printf("function putch()\n");
	printf("    io.write(string.char(m[i]%%256))\n");
	printf("    io.flush()\n");
	printf("end\n");

	printf("\n");
	printf("brainfuck()\n");
	break;
    }
}
