#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Ksh translation from BF, runs at about 750,000 instructions per second.
 * Bash translation from BF, runs at about 340,000 instructions per second.
 *
 * This has special optimisations for the BASH shell, it's still slower than
 * all the other modern shells I can find easily.
 */

int do_input = 0;
int ind = 0;
int bash_only = 0;

#define prv(s,v)        printf("%*s" s "\n", ind*4, "", (v))
#define pr(s)           printf("%*s" s "\n", ind*4, "")

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    if (strcmp(arg, "-ksh") == 0) { bash_only = 0; return 1; }
    if (strcmp(arg, "-bash") == 0) { bash_only = 1; return 1; }
    if (strcmp("-h", arg) ==0) {
	fprintf(stderr, "%s\n",
	"\t"    "-bash   Generate special bash code."
	"\n\t"  "-ksh    Allow for bash or ksh.");
	return 1;
    } else
    return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	if (bash_only) {
	    pr("#!/bin/bash");
	    pr("set -f +B");
	} else
	    pr("#!/bin/ksh");

	pr("brainfuck() {");
	ind++;
        prv("((P=%d))", tapeinit); break;
	break;

    case 'X': pr("echo Infinite Loop 2>&1 ; exit 1"); break;

    case '=': prv("((M[P]=%d))", count); break;
    case 'B':
	if(bytecell) pr("((M[P]&=255))");
	pr("((V=M[P]))");
	break;
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
	if(bytecell) { pr("while [[ $((M[P]&=255)) != 0 ]] ; do"); }
	else { pr("while [[ $((M[P])) != 0 ]] ; do"); }
	ind++;
	break;
    case ']': ind--; pr("done"); break;

    case '~':
	ind--;
	pr("}");

	pr("");
	if (bash_only)
	    pr("o(){ printf -v C '\\\\%%04o' $((M[P]&=255)); echo -n -e \"$C\" ; }");
	else {
	    pr("");
	    pr("if [ .`echo -n` = .-n ]");
	    pr("then");
	    pr("    echon() { echo \"$1\\c\"; }");
	    pr("    echoe() { echo \"$1\\c\"; }");
	    pr("else");
	    pr("    echon() { echo -n \"$1\"; }");
	    pr("    if [ .`echo -e` = .-e ]");
	    pr("    then echoe() { echo -n \"$1\"; }");
	    pr("    else if [ .`echo -e '\\070\\c'` = .8 ]");
	    pr("         then echoe() { echo -e \"$1\\c\"; }");
	    pr("         else echoe() { echo -n -e \"$1\"; }");
	    pr("         fi");
	    pr("    fi");
	    pr("fi");
	    pr("");
	    pr("o(){ echoe \"`printf '\\\\\\\\%%04o' $((M[P]&255))`\" ; }");
	}

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
	    if (bash_only) {
		pr("    c=\"${line:0:1}\"");
		pr("    line=\"${line:1}\"");
		pr("");
		pr("    printf -v C %%d \\'\"$c\"");
	    } else {
		pr("    C=\"$line\"");
		pr("    while [ ${#C} -gt 1 ] ; do C=\"${C%%?}\"; done");
		pr("    line=\"${line#?}\"");
		pr("    C=`echon \"$C\" |od -d |awk 'NR==1{print 0+$2;}'`");
	    }
	    pr("    ((M[P]=C))");
	    pr("}");
	}

	pr("");
	pr("brainfuck ||:");
	break;
    }
}
