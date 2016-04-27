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
int select_bash = 0;
int select_ksh = 0;

#define prv(s,v)        printf("%*s" s "\n", ind*4, "", (v))
#define pr(s)           printf("%*s" s "\n", ind*4, "")

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    if (strcmp(arg, "-ksh") == 0) { select_ksh = 1; return 1; }
    if (strcmp(arg, "-bash") == 0) { select_bash = 1; return 1; }
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
	if (select_bash)
	    pr("#!/bin/bash");
	else
	    pr("#!/bin/ksh");

	pr("if ( eval 'typeset -i M P && M[1]=3 && ((M[2]+=1)) &&");
	pr("     ((M[1]+=1)) &&  [[ ${M[1]} -eq 4 && ${M[2]} -eq 1 ]]' ) 2>/dev/null");
	pr("then eval 'typeset -i M P' 2>/dev/null");
	pr("elif ( eval 'M[1]=3 && ((M[2]+=1))");
	pr("     ((M[1]+=1)) &&  [[ ${M[1]} -eq 4 && ${M[2]} -eq 1 ]]' ) 2>/dev/null");
	pr("then :");
	pr("else");
	pr("    echo 'ERROR: The shell must be Ksh compatible' >&2");
	pr("    exit 1");
	pr("fi");

	if (select_bash) 
	    pr("eval 'set -f +B' 2>/dev/null");

	pr("export LC_ALL=C");
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
	if(bytecell) { pr("while (( (M[P]&=255) != 0)) ; do"); }
	else { pr("while ((M[P])) ; do"); }
	ind++;
	break;
    case ']': ind--; pr("done"); break;

    case '~':
	ind--;
	pr("}");

	if (!select_bash || select_ksh) {
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
	}

	pr("");
	if (select_bash && select_ksh) {
	    pr("o(){");
	    pr("    if printf -v C '\\\\%%04o' $((M[P]&=255)) 2>/dev/null");
	    pr("    then echo -n -e \"$C\"");
	    pr("    else");
	    pr("        o(){ echoe \"`printf '\\\\\\\\%%04o' $((M[P]&255))`\" ; }");
	    pr("        o");
	    pr("    fi");
	    pr("}");
	} else if (select_bash)
	    pr("o(){ printf -v C '\\\\%%04o' $((M[P]&=255)); echo -n -e \"$C\" ; }");
	else if (select_ksh)
	    pr("o(){ echoe \"`printf '\\\\\\\\%%04o' $((M[P]&255))`\" ; }");
	else {
	    int i;

	    printf("o() {\n");
	    printf("case $((M[P]+0)) in\n");
	    for(i=0; i<256; i++) {
		if (i >= ' ' && i <= '~' && i != '\'' && i != '\\')
		    printf("%d ) echon '%c' ;;\n", i, i);
		else if (i == 10 )
		    printf("%d ) echo ;;\n", i);
		else if (i < 64)
		    printf("%d ) echoe '\\%03o' ;;\n", i, i);
		else
		    printf("%d ) echoe '\\%04o' ;;\n", i, i);
	    }
	    printf("esac\n}\n");
	}

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
	    pr("        ((M[P]=10))");
	    pr("        return");
	    pr("    }");
	    if (select_bash) {
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
