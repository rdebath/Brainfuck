#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
 * MS Power shell translation from BF, runs at about 1,200,000 instructions per second.
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
	I; printf("function brainfuck() {\n");
	ind ++;
	I; printf("$m = @([int]0) * 65536\n");
	I; printf("$p = %d\n", BOFF);
	break;

    // case 'X': I; printf("print \"Infinite Loop\"\nGOTO 200\n"); break;
    case '=': I; printf("$m[$p]=%d\n", count); break;
    case 'B': I; printf("$v=$m[$p]\n"); break;
    case 'M': I; printf("$m[$p]=$m[$p]+$v*%d\n", count); break;
    case 'N': I; printf("$m[$p]=$m[$p]-$v*%d\n", count); break;
    case 'S': I; printf("$m[$p]=$m[$p]+$v\n"); break;
    case 'Q': I; printf("if ($v -ne 0) { $m[$p] = %d; }\n", count); break;

    case 'm': I; printf("if ($v -ne 0) { $m[$p]=$m[$p]+$v*%d; }\n", count); break;
    case 'n': I; printf("if ($v -ne 0) { $m[$p]=$m[$p]-$v*%d; }\n", count); break;
    case 's': I; printf("if ($v -ne 0) { $m[$p]=$m[$p]+$v; }\n"); break;

    case '+': I; printf("$m[$p]+=%d;\n", count); break;
    case '-': I; printf("$m[$p]-=%d;\n", count); break;
    case '<': I; printf("$p-=%d;\n", count); break;
    case '>': I; printf("$p+=%d;\n", count); break;
    case '[':
	if(bytecell) { I; printf("$m[$p]=$m[$p] -band 255;\n"); }
	I; printf("while ($m[$p] -ne 0){\n"); ind++; break;
    case ']':
	if(bytecell) { I; printf("$m[$p]=$m[$p] -band 255;\n"); }
	ind--; I; printf("}\n");
	break;
    case '.': I; printf("[Console]::Write([string][char]$m[$p]);\n"); break;
    case ',':
	I; printf("inchar\n");
	do_input = 1;
	break;
    case '~':
	ind --;
	I; printf("}\n\n");
	if (do_input) {
	    /* Real fuckup here, if I don't prefix every single invocation of
	     * the variables with $script: it makes a local copy of the
	     * global variable. That is the automatically created local
	     * variable is init'd with the global variable's value.
	     * Talk about an obviously stupid idea!!
	     */
	    printf("function inchar() {\n");
	    printf("    if ($script:goteof -eq $true) { return; }\n");
	    printf("    if ($script:gotline -eq $false) {\n");
	    printf("        $script:line = Read-Host "";\n");
	    printf("        $script:gotline = $true;\n");
	    printf("        $script:goteof = $false; # Well crap.\n");
	    printf("    }\n");
	    printf("    if ($script:line -eq \"\") {\n");
	    printf("        $script:gotline = $false;\n");
	    printf("        $m[$p]=10;\n");
	    printf("        return;\n");
	    printf("    }\n");
	    printf("    $m[$p] = [int]($script:line.Substring(0,1).ToCharArray()[0]);\n");
	    printf("    $script:line = $script:line.Substring(1);\n");
	    printf("}\n");
	    printf("$script:line = \"\"\n");
	    printf("$script:goteof = $false\n");
	    printf("$script:gotline = $false\n");
	    printf("\n");
	}
	I; printf("brainfuck\n");
	break;
    }
}
