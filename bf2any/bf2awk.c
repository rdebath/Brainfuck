#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * MAWK translation from BF, runs at about 23,000,000 instructions per second.
 * GAWK translation from BF, runs at about 13,000,000 instructions per second.
 *
 *  Getting character I/O is a little interesting here; but otherwise the
 *  language is very similar to C (without semicolons).
 *
 *  Generated code works in most awk implementations. For really old ones
 *  without functions the getch() function will have to be inlined.
 */

/*
#define INLINEGETCH
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
	printf("#!/usr/bin/awk -f\n");
	printf("BEGIN {\n"); ind++;
	I; printf("p=%d\n",BOFF); break;
	break;

    case '=': I; printf("m[p] = %d\n", count); break;
    case 'B':
	if(bytecell) { I; printf("m[p]=m[p]%%256\n"); }
	I; printf("v = m[p]\n");
	break;
    case 'M': I; printf("m[p] = m[p]+v*%d\n", count); break;
    case 'N': I; printf("m[p] = m[p]-v*%d\n", count); break;
    case 'S': I; printf("m[p] = m[p]+v\n"); break;
    case 'Q': I; printf("if (v != 0) m[p] = %d;\n", count); break;
    case 'm': I; printf("if (v != 0) m[p] = m[p]+v*%d;\n", count); break;
    case 'n': I; printf("if (v != 0) m[p] = m[p]-v*%d;\n", count); break;
    case 's': I; printf("if (v != 0) m[p] = m[p]+v;\n"); break;

    case 'X': I; printf("print \"Abort: Infinite Loop.\\n\" >\"/dev/stderr\"; exit;\n"); break;

    case '+': I; printf("m[p] += %d\n", count); break;
    case '-': I; printf("m[p] -= %d\n", count); break;
    case '<': I; printf("p -= %d\n", count); break;
    case '>': I; printf("p += %d\n", count); break;
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
	I; printf("exit 0\n");
	ind--;
	printf("}\n");

#ifndef INLINEGETCH
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
	    printf("    if (!genord) {\n");
	    printf("        for(i=1; i<256; i++)\n");
	    printf("            ord[sprintf(\"%%c\",i)] = i\n");
	    printf("        genord=1\n");
	    printf("    }\n");
	    printf("    c = substr(line, 1, 1)\n");
	    printf("    line=substr(line, 2)\n");
	    printf("    m[p] = ord[c]\n");
	    printf("}\n");
	}
#endif
	break;

    case '.':
	if(bytecell) { I; printf("m[p]=m[p]%%256\n"); }
	I; printf("printf \"%%c\",m[p]\n");
	break;
#ifndef INLINEGETCH
    case ',': I; printf("getch()\n"); do_input++; break;
#else
    case ',':
	I; printf("while(1) {\n");
	I; printf("    if (goteof) break\n");
	I; printf("    if (!gotline) {\n");
	I; printf("        gotline = getline\n");
	I; printf("        if(!gotline) goteof = 1\n");
	I; printf("        if (goteof) break\n");
	I; printf("        line = $0\n");
	I; printf("    }\n");
	I; printf("    if (line == \"\") {\n");
	I; printf("        gotline=0\n");
	I; printf("        m[p]=10\n");
	I; printf("        break\n");
	I; printf("    }\n");
	I; printf("    if (!genord) {\n");
	I; printf("        for(i=1; i<256; i++)\n");
	I; printf("            ord[sprintf(\"%%c\",i)] = i\n");
	I; printf("        genord=1\n");
	I; printf("    }\n");
	I; printf("    c = substr(line, 1, 1)\n");
	I; printf("    line=substr(line, 2)\n");
	I; printf("    m[p] = ord[c]\n");
	I; printf("    break\n");
	I; printf("}\n");
	break;
#endif
    }
}
