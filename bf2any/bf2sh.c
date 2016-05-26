#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Bourne shell translation from BF, runs at about 950,000 instructions per second.
 *
 * This is a 'current version' translation to the common Bourne shell
 * language.
 *
 * It runs on ash, bash, dash, ksh, lksh, mksh, pdksh-5.2.12+, yash, zsh.
 *
 * It does NOT run on ksh88, pdksh-5.2.3, posh
 *
 * Ash/Dash looks like the fastest at the above speed, Bash is the
 * slowest at about 180,000.
 */

int do_input = 0;
int ind = 0;
#define prv(s,v)        printf("%*s" s "\n", ind*4, "", (v))
#define pr(s)           printf("%*s" s "\n", ind*4, "")

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	pr("#!/bin/sh");
	pr("if (eval 'P=1 && : $((M$P=3)) && Q=500 && : $((M$Q+=1)) &&");
	pr("   : $((M$P+=1)) && [ $((M$P)) = 4 -a $((M$Q)) = 1 ]' ) 2>/dev/null");
	pr("then :");
	pr("else");
	pr("    echo 'ERROR: The shell must be POSIX compatible' >&2");
	pr("    exit 1");
	pr("fi");
	pr("");

	pr("set -f");
	pr("");

	pr("brainfuck() {");
	ind++;
	prv("P=%d", tapeinit);
	break;

    case 'X': pr("echo Infinite Loop 2>&1 ; exit 1"); break;

    case '=': prv(": $((M$P=%d))", count); break;
    case 'B':
	if(bytecell) pr(": $((M$P&=255))");
	pr(": $((V=M$P))");
	break;
    case 'M': prv(": $((M$P+=V*%d))", count); break;
    case 'N': prv(": $((M$P-=V*%d))", count); break;
    case 'S': pr(": $((M$P+=V))"); break;
    case 'Q': prv("[ $V -ne 0 ] && : $((M$P=%d))", count); break;
    case 'm': prv("[ $V -ne 0 ] && : $((M$P+=V*%d))", count); break;
    case 'n': prv("[ $V -ne 0 ] && : $((M$P-=V*%d))", count); break;
    case 's': pr("[ $V -ne 0 ] && : $((M$P+=V))"); break;

    case '+': prv(": $((M$P+=%d))", count); break;
    case '-': prv(": $((M$P-=%d))", count); break;
    case '>': prv(": $((P+=%d))", count); break;
    case '<': prv(": $((P-=%d))", count); break;
    case '.': pr("o"); break;
    case ',': pr("getch"); do_input++; break;

    case '[':
	if(bytecell) { pr("while [ $((M$P&=255)) != 0 ] ; do"); }
	else { pr("while [ $((M$P)) != 0 ] ; do"); }
	ind++;
	break;
    case ']': ind--; pr("done"); break;

    case '~':
	ind--;
	pr("}");

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
	pr("if [ .`echoe '\\171'` = .y ]");
	pr("then o(){ echoe \"`printf '\\\\\\\\%%03o' $((M$P&255))`\" ; }");
	pr("else o(){ echoe \"`printf '\\\\\\\\%%04o' $((M$P&255))`\" ; }");
	pr("fi");
	pr("");

	if (do_input) {
	    pr("");
	    pr("getch() {");
	    pr("    [ \"$goteof\" = \"y\" ] && return;");
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
	    pr("        : $((M$P=10))");
	    pr("        return");
	    pr("    }");
	    pr("    A=\"$line\"");
	    pr("    while [ ${#A} -gt 1 ] ; do A=\"${A%%?}\"; done");
	    pr("    line=\"${line#?}\"");
	    pr("    A=`printf %%d \\'\"$A\"`");
	    pr("    : $((M$P=$A))");
	    pr("}");
	}

	pr("");
	pr("brainfuck ||:");
	break;
    }
}
