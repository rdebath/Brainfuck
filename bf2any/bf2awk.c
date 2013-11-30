#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
 * MAWK translation from BF, runs at about 23,000,000 instructions per second.
 * GAWK translation from BF, runs at about 13,000,000 instructions per second.
 *
 *  Getting character I/O is a little interesting here; but otherwise the
 *  language is very similar to C (without semicolons).
 *
 *  Generated code works in most (if not all) awk implementations.
 */

extern int bytecell;

int do_input = 0;
int ind = 1;
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
	printf("#!/usr/bin/awk -f\n");
	printf("function brainfuck() {\n");
	I; printf("p=0\n"); break;
	break;

    case '=': I; printf("m[p] = %d\n", count); break;
    case 'B': I; printf("v = m[p]\n"); break;
    case 'M': I; printf("m[p] = m[p]+v*%d\n", count); break;
    case 'N': I; printf("m[p] = m[p]-v*%d\n", count); break;
    case 'S': I; printf("m[p] = m[p]+v\n"); break;
    case 'Q': I; printf("if (v != 0) m[p] = %d;\n", count); break;
    case 'm': I; printf("if (v != 0) m[p] = m[p]+v*%d;\n", count); break;
    case 'n': I; printf("if (v != 0) m[p] = m[p]-v*%d;\n", count); break;
    case 's': I; printf("if (v != 0) m[p] = m[p]+v;\n"); break;

    case 'X': I; printf("print \"Abort: Infinite Loop.\\n\" >\"/dev/stderr\"; exit;\n"); break;

    case '+': I; printf("m[p]=m[p]+%d\n", count); break;
    case '-': I; printf("m[p]=m[p]-%d\n", count); break;
    case '<': I; printf("p=p-%d\n", count); break;
    case '>': I; printf("p=p+%d\n", count); break;
    case '.': I; printf("printf \"%%c\",m[p]\n"); break;
    case ',': I; printf("getch()\n"); do_input++; break;
    case '[':
	if(bytecell) { I; printf("m[p]=m[p]%%256\n"); }
	I; printf("while(m[p]!=0){\n");
	ind++;
	break;
    case ']':
	if(bytecell) { I; printf("m[p]=m[p]%%256\n"); }
	ind--;
	I; printf("}\n");
	break;

    case '~':
	printf("}\n");

	if (do_input) {
	    printf("\n");
	    printf("function getch() {\n");
	    printf("    if (goteof) return;\n");
	    printf("    if (!gotline) {\n");
	    printf("        gotline = getline line\n");
	    printf("        goteof = !gotline\n");
	    printf("        if (goteof) return\n");
	    printf("    }\n");
	    printf("    if (line == \"\") {\n");
	    printf("        gotline=0\n");
	    printf("        m[p]=10\n");
	    printf("        return\n");
	    printf("    }\n");
	    printf("    c = substr(line, 1, 1)\n");
	    printf("    line=substr(line, 2)\n");
	    printf("    m[p] = ord[c]\n");
	    printf("}\n");
	    printf("BEGIN{\n");
	    printf("    for(i=1; i<256; i++) ord[sprintf(\"%%c\",i)] = i\n");
	    printf("    brainfuck();\n");
	    printf("}\n");
	}
	else
	    printf("BEGIN{ brainfuck(); }\n");
	break;
    }
}
