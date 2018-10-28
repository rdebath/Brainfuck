#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Ksh translation from BF, runs at about 3,400,000 instructions per second.
 * Bash translation from BF, runs at about 420,000 instructions per second.
 */

static int do_input = 0;
static int do_output = 0;
static int ind = 0;
static int in_arith = 0;
static int curr_offset = -1;

struct be_interface_s be_interface = {};

static void print_string(void);

#define prv(s,v)        printf("%*s" s "\n", ind*4, "", (v))
#define pr(s)           printf("%*s" s "\n", ind*4, "")

static void
shcode() {
    if (!in_arith) return;
    in_arith = 0;
    ind--;
    pr("1))");
}

static void
arith() {
    if (in_arith) return;
    in_arith = 1;
    pr("((");
    ind++;
}

void
outcmd(int ch, int count)
{
    if (curr_offset >= 0) {
	if (ch == '>') { curr_offset += count; return; }
	if (ch == '<' && curr_offset > count) { curr_offset -= count; return; }
	if (ch != '"' && ch != '~') {
	    pr("typeset -i M P V");
	    prv("P=%d", curr_offset);
	    curr_offset = -1;
	}
    }

    switch(ch) {
    case '!':
	pr("#!/bin/ksh");
	pr("(eval 'set -o sh +o sh') 2>/dev/null && set +o sh 2>/dev/null");
	pr("(eval 'set -o posix +o posix') 2>/dev/null && set +o posix 2>/dev/null");
	pr("if (eval 'typeset -i M P V && M[1]=3 && ((M[500]+=(1),1)) &&");
	pr("   ((M[1]+=1)) && [[ ${M[1]} -eq 4 && ${M[500]} -eq 1 ]]' ) 2>/dev/null");
	pr("then :");
	pr("else");
	pr("    echo 'ERROR: The shell must be Ksh93 compatible' >&2");
	pr("    exit 1");
	pr("fi");

	pr("");
	pr("(eval 'set -f +B') 2>/dev/null && set -f +B 2>/dev/null");
	pr("export LC_ALL=C");

	pr("");
	pr("brainfuck() {");
	ind++;
	curr_offset = tapeinit;
	break;

    case 'X': shcode(); pr("echo Infinite Loop 2>&1 ; exit 1"); break;

    case '=':
	if (in_arith)
	    prv("M[P]=%d,", count);
	else
	    prv("M[P]=%d", count);
	break;

    case 'B':
	arith();
	if(bytecell) pr("M[P]&=255,");
	pr("V=M[P],");
	break;
    case 'M': arith(); prv("M[P]+=V*%d,", count); break;
    case 'N': arith(); prv("M[P]-=V*%d,", count); break;
    case 'S': arith(); pr("M[P]+=V,"); break;
    case 'T': arith(); pr("M[P]-=V,"); break;
    case '*': arith(); pr("M[P]*=V,"); break;

    case 'C': arith(); prv("M[P]=V*%d,", count); break;
    case 'D': arith(); prv("M[P]=-V*%d,", count); break;
    case 'V': arith(); pr("M[P]=V,"); break;
    case 'W': arith(); pr("M[P]=-V,"); break;

    case '+': arith(); prv("M[P]+=%d,", count); break;
    case '-': arith(); prv("M[P]-=%d,", count); break;
    case '>': arith(); prv("P+=%d,", count); break;
    case '<': arith(); prv("P-=%d,", count); break;
    case '.': shcode(); pr("o $((M[P]&255))"); do_output++; break;
    case ',': shcode(); pr("getch"); do_input++; break;
    case '"': shcode(); print_string(); break;

    case '[':
	shcode();
	if(bytecell) { pr("while (( (M[P]&=255) != 0)) ; do"); }
	else { pr("while ((M[P])) ; do"); }
	ind++;
	break;
    case ']': shcode(); ind--; pr("done"); break;

    case 'I':
	shcode();
	if(bytecell) { pr("if (( (M[P]&=255) != 0)) ; then"); }
	else { pr("if ((M[P])) ; then"); }
	ind++;
	break;
    case 'E': shcode(); ind--; pr("fi"); break;

    case '~':
	shcode();
	ind--;
	pr("}");

	if (do_output) {
	    pr("echon() { printf \"%%s\" \"$*\" ;}");
	    pr("if ((BASH_VERSINFO[0]>0)) ; then");
	    pr("    # Bash works but so slooooow");
	    pr("    o(){ printf -v C '\\\\%%04o' $1; echo -n -e \"$C\" ; }");
	    pr("elif ( ( eval ': ${.sh.version}' ) 2>/dev/null >&2 ) ; then");
	    pr("    # A real Ksh93 has a fast printf");
	    pr("    o(){ printf \"\\\\x$(printf %%02x $1)\";}");
	    pr("else");
	    pr("    # Hopefully the ksh88 version is quick");
	    pr("    o() { typeset -i8 N ; N=$1; print -n - \\\\0$(print ${N#*#});}");
	    pr("    echon() { print -n - \"$*\" ;}");
	    pr("fi");

	    /* Safe fallback */
	    pr("[ \"`{ o 65;echon A;} 2>/dev/null`\" != AA ] && {");
	    pr("    o() { echo $1 | awk '{printf \"%%c\",$1;}';}");
	    pr("    echon() { awk -v S=\"$*\" 'BEGIN{printf \"%%s\",S;}';}");
	    pr("}");
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

	    ind++;
	    pr("if ((BASH_VERSINFO[0]>0)) ; then");
	    pr("    c=\"${line:0:1}\"");
	    pr("    line=\"${line:1}\"");
	    pr("    printf -v C %%d \"'$c\"");

	    pr("");
	    pr("elif ( line=nnynnn; j=2; [[ \"${line:j:1}\" == y ]] ) 2>/dev/null ; then");
	    pr("    c=\"${line:0:1}\"");
	    pr("    line=\"${line:1}\"");
	    pr("    C=$(printf %%d \"'$c\")");

	    pr("");
	    pr("else");
	    pr("    C=\"$line\"");
	    pr("    while [ ${#C} -gt 1 ] ; do C=\"${C%%?}\"; done");
	    pr("    line=\"${line#?}\"");
	    pr("    C=`echo \"$C\" |dd bs=1 count=1 2>/dev/null |od -d |awk 'NR==1{print 0+$2;}'`");
	    pr("fi");
	    pr("((M[P]=C))");

	    ind--;
	    pr("}");
	}

	pr("");
	pr("set -e");
	pr("brainfuck");
	break;
    }
}

static void
print_string(void)
{
    char * str = get_string();
    char buf[256];
    int badchar = 0;
    size_t outlen = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || badchar || outlen > sizeof(buf)-8))
	{
	    buf[outlen] = 0;
	    if (badchar == '\n') {
		prv("echo '%s'", buf);
		badchar = 0;
	    } else {
		do_output++;
		prv("echon '%s'", buf);
	    }
	    outlen = 0;
	}
	if (badchar) {
	    if (badchar == 10)
		pr("echo");
	    else {
		prv("o %d", badchar);
		do_output++;
	    }
	    badchar = 0;
	}
	if (!*str) break;

	if (*str == '-' && outlen == 0) {
	    badchar = (*str & 0xFF);
	} else if (*str >= ' ' && *str <= '~' && *str != '\'') {
	    buf[outlen++] = *str;
	} else if (*str == '\'') {
	    buf[outlen++] = '\'';
	    buf[outlen++] = '\\';
	    buf[outlen++] = *str;
	    buf[outlen++] = '\'';
	} else {
	    badchar = (*str & 0xFF);
	}
    }
}
