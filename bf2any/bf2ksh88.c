#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Ksh88 translation from BF, runs at about 2,600,000 instructions per second.
 */

static int do_input = 0;
static int ind = 0;

#define prv(s,v)        printf("%*s" s "\n", ind*4, "", (v))
#define pr(s)           printf("%*s" s "\n", ind*4, "")

static void print_string(void);

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	pr("#!/bin/ksh");

	pr("if ( eval 'typeset -i M P V ; M[1]=3 && : $((M[500]+=1)) &&");
	pr("   ((M[1]+=1)) && (( M[1] == 4 && M[500] == 1 ))' ) 2>/dev/null");
	pr("then :");
	pr("else");
	pr("    echo 'ERROR: The shell must be Ksh88 compatible' >&2");
	pr("    exit 1");
	pr("fi");

	pr("");
	pr("o() { typeset -i8 N ; N=$1; print -n - \\\\0\"$(print \"${N#8?}\")\";}");
	pr("if [ \"$({ o 65;o 65;} 2>&1 </dev/null)\" != AA ]");
	pr("then");
	pr("    echo 'ERROR: The print command must be Ksh88 compatible' >&2");
	pr("    exit 1");
	pr("fi");

	pr("");
	pr("set -e");
	pr("(typeset -i V) 2>/dev/null && typeset -i M P V");
	pr("brainfuck() {");
	ind++;
	prv(": $((P=%d))", tapeinit); break;
	break;

    case 'X': pr("echo Infinite Loop 2>&1 ; exit 1"); break;

    case '=': prv(": $((M[P]=%d))", count); break;
    case 'B':
	if(bytecell) pr(": $((M[P]&=255))");
	pr(": $((V=M[P]))");
	break;
    case 'M': prv(": $((M[P]+=V*%d))", count); break;
    case 'N': prv(": $((M[P]-=V*%d))", count); break;
    case 'S': pr(": $((M[P]+=V))"); break;

    case 'Q': prv("((V)) && : $((M[P]=%d))", count); break;
    case 'm': prv("((V)) && : $((M[P]+=V*%d))", count); break;
    case 'n': prv("((V)) && : $((M[P]-=V*%d))", count); break;
    case 's': pr("((V)) && : $((M[P]+=V))"); break;

    case '+': prv(": $((M[P]+=%d))", count); break;
    case '-': prv(": $((M[P]-=%d))", count); break;
    case '>': prv(": $((P+=%d))", count); break;
    case '<': prv(": $((P-=%d))", count); break;
    case '.': pr("o $((M[P]&255))"); break;
    case ',': pr("getch"); do_input++; break;
    case '"': print_string(); break;

    case '[':
	if(bytecell) { pr("while (( (M[P]&=255) != 0)) ; do"); }
	else { pr("while ((M[P])) ; do"); }
	ind++;
	break;
    case ']': ind--; pr("done"); break;

    case '~':
	ind--;
	pr("}");

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
	    pr("        : $((M[P]=10))");
	    pr("        return");
	    pr("    }");

	    pr("    C=\"$line\"");
	    pr("    while [ ${#C} -gt 1 ] ; do C=\"${C%%?}\"; done");
	    pr("    line=\"${line#?}\"");
	    pr("    C=`print -n - \"$C\" |od -d |awk 'NR==1{print 0+$2;}'`");

	    pr("    : $((M[P]=C))");
	    pr("}");
	}

	pr("");
	pr("brainfuck");
	break;
    }
}

static void
print_string(void)
{
    char * str = get_string();
    char buf[256];
    int badchar = 0, badecho = 0;
    size_t outlen = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || badchar || outlen > sizeof(buf)-8))
	{
	    buf[outlen] = 0;
	    if (badchar == '\n' && !badecho) {
		prv("echo '%s'", buf);
		badchar = 0;
	    } else {
		if (badchar == '\n') {
		    badchar = 0;
		    if (buf[0] == '-') {
			prv("print - '%s'", buf);
		    } else {
			prv("print '%s'", buf);
		    }
		} else {
		    if (buf[0] == '-') {
			prv("print -n - '%s'", buf);
		    } else {
			prv("print -n '%s'", buf);
		    }
		}
	    }
	    outlen = 0;
	}
	if (badchar) {
	    prv("o %d", badchar);
	    badchar = 0;
	}
	if (!*str) break;

	if (*str == '-' && outlen == 0) {
	    badecho = 1;
	    buf[outlen++] = *str;
	} else if (*str >= ' ' && *str <= '~' && *str != '\\' && *str != '\'') {
	    buf[outlen++] = *str;
	} else if (*str == 27 || *str == '\'') {
	    badecho = 1;
	    buf[outlen++] = '\\';
	    sprintf(buf+outlen, "%03o", *str);
	    outlen += 3;
	} else if (*str == '\\') {
	    badecho = 1;
	    buf[outlen++] = '\\';
	    buf[outlen++] = '\\';
	} else {
	    badchar = (*str & 0xFF);
	}
    }
}
