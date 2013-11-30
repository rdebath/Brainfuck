#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
 * Bash translation from BF, runs at about 340,000 instructions per second.
 */

extern int bytecell;

int do_input = 0;
int ind = 0;
#define prv(s,v)        printf("%*s" s "\n", ind*4, "", (v))
#define pr(s)           printf("%*s" s "\n", ind*4, "")

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
	pr("#!/bin/bash");
	pr("set -f +B");
	pr("brainfuck() {");
	ind++;
	break;

    case 'X': pr("echo Infinite Loop 2>&1 ; exit 1"); break;

    case '=': prv("((M[P]=%d))", count); break;
    case 'B': pr("((V=M[P]))"); break;
    case 'M': prv("((M[P]+=V*%d))", count); break;
    case 'N': prv("((M[P]-=V*%d))", count); break;
    case 'S': pr("((M[P]+=V))"); break;
    case 'Q': prv("[[ $V -ne 0 ]] && ((M[P]=%d))", count); break;
    case 'm': prv("[[ $V -ne 0 ]] && ((M[P]+=V*%d))", count); break;
    case 'n': prv("[[ $V -ne 0 ]] && ((M[P]-=V*%d))", count); break;
    case 's': pr("[[ $V -ne 0 ]] && ((M[P]+=V))"); break;

    case '+': prv("((M[P]+=%d))", count); break;
    case '-': prv("((M[P]-=%d))", count); break;
    case '>': prv("((P+=%d))", count); break;
    case '<': prv("((P-=%d))", count); break;
    case '.': pr("o"); break;
    case ',': pr("getch"); do_input++; break;

    case '[':
	if(bytecell) { pr("while [[ $((M[P]&=255)) != 0 ]] ; do :"); }
	else { pr("while [[ $((M[P])) != 0 ]] ; do :"); }
	ind++;
	break;
    case ']': ind--; pr("done"); break;

    case '~':
	ind--;
	pr("}");

	pr("");
	pr("o(){ printf -v C '\\%%04o' $((M[P]&=255)); echo -n -e \"$C\" ; }");

	if (do_input) {
	    pr("");
	    pr("getch() {");
	    pr("    [ \"$goteof\" == \"y\" ] && return;");
	    pr("    [ \"$gotline\" != \"y\" ] && {");
	    pr("        if read -r line");
	    pr("        then");
	    pr("            gotline=y");
	    pr("        else");
	    pr("            goteof=y");
	    pr("            return");
	    pr("        fi");
	    pr("    }");
	    pr("    [ \"$line\" = \"\" ] && {");
	    pr("        gotline=n");
	    pr("        ((M[P]=10))");
	    pr("        return");
	    pr("    }");
	    pr("    c=\"${line:0:1}\"");
	    pr("    line=\"${line:1}\"");
	    pr("");
	    pr("    printf -v C %%d \\'\"$c\"");
	    pr("    M[$P]=$C");
	    pr("}");
	}

	pr("");
	pr("brainfuck ||:");
	break;
    }
}
