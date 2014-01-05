#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * PHP translation from BF, runs at about 41,000,000 instructions per second.
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
	printf( "%s%d%s",
		"<?php\n"
		"$m=array_fill(0, 65535, 0);\n"
		"$p=",BOFF,";\n");
	break;

    case '=': I; printf("$m[$p] = %d;\n", count); break;
    case 'B':
	if(bytecell) { I; printf("$m[$p] &= 255;\n"); }
	I; printf("$v = $m[$p];\n");
	break;
    case 'M': I; printf("$m[$p] = $m[$p]+$v*%d;\n", count); break;
    case 'N': I; printf("$m[$p] = $m[$p]-$v*%d;\n", count); break;
    case 'S': I; printf("$m[$p] = $m[$p]+$v;\n"); break;
    case 'Q': I; printf("if ($v != 0) $m[$p] = %d;\n", count); break;
    case 'm': I; printf("if ($v != 0) $m[$p] = $m[$p]+$v*%d;\n", count); break;
    case 'n': I; printf("if ($v != 0) $m[$p] = $m[$p]-$v*%d;\n", count); break;
    case 's': I; printf("if ($v != 0) $m[$p] = $m[$p]+$v;\n"); break;

    case 'X': I; printf("fwrite(STDERR, \"Abort: Infinite Loop.\\n\"); exit;\n"); break;

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
    case ',': I; printf("$s = fread(STDIN, 1); if(strlen($s)) $m[$p] = ord($s);\n"); break;
    }
}
