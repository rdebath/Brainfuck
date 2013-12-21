#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
 * Bourne Shell (dash) translation from BF, runs at about 170,000 instructions per second.
 *
 * The pseudo RLE is required so that dash doesn't run out of memory on the
 * Lost Kingdom program. (Yes it does work ... but slowly! )
 *
 * NB: Dash needs an MAXPRLE of 2, bash needs 22.
 *
 * It seems that shells older than dash don't implement ${var#?} this is a
 * significant problem for '>' and '<' and probably an insoluble problem
 * for ',' without using external programs.
 *
 * The 'printf' command in the ',' command could be replaced by a "case".
 *
 * If external programs are allowed it becomes a lot simpler.
 */

#define MAXPRLE	2

int do_input = 0;
int do_output = 0;
int ind = 0;

int
check_arg(char * arg)
{
    return 0;
}

void
outcmd(int ch, int count)
{
    int i;
    while (count>MAXPRLE) { outcmd(ch, MAXPRLE); count -= MAXPRLE; }

    if (count > 1) {
	switch(ch) {
	case '+': printf("u%d\n", count); break;
	case '-': printf("d%d\n", count); break;
	case '>': printf("r%d\n", count); break;
	case '<': printf("l%d\n", count); break;
	}
	return;
    }

    switch(ch) {
    case '=': printf("z\n"); break;
    case '+': printf("u\n"); break;
    case '-': printf("d\n"); break;
    case '>': printf("r\n"); break;
    case '<': printf("l\n"); break;
    case '[':
	printf("f\n");
	printf("while [ \"$A\" != 0 ] ; do\n");
	ind++;
	break;
    case ']':
	printf("f\n");
	ind--; printf("done\n");
	break;
    case '.': printf("o\n"); do_output = 1; break;
    case ',': printf("i\n"); do_input = 1; break;
    }

    switch(ch) {
    case '!': printf("#!/bin/sh\nbf(){\n"); break;
    case '~':
	printf("}\n");

	if (MAXPRLE>1) {
	    for (i=3; i<=MAXPRLE; i++) {
		printf("u%d() { u ; u%d; }\n", i, i-1);
		printf("d%d() { d ; d%d; }\n", i, i-1);
		printf("l%d() { l ; l%d; }\n", i, i-1);
		printf("r%d() { r ; r%d; }\n", i, i-1);
	    }

	    printf("u2() { u ; u; }\n");
	    printf("d2() { d ; d; }\n");
	    printf("l2() { l ; l; }\n");
	    printf("r2() { r ; r; }\n");
	}

	printf("u() { eval \"A=\\$M$P\"; inc; eval \"M$P=$A\" ; }\n");
	printf("d() { eval \"A=\\$M$P\"; dec; eval \"M$P=$A\" ; }\n");
	printf("f() { eval \"A=\\$M$P\"; [ .$A = . ] && A=0; }\n");
	printf("z() { eval \"M$P=0\" ; }\n");

	printf("inc() {\n");
	printf("case \"$A\" in\n");
	for(i=0; i<256; i++)
	    printf("%d ) A=%d ;;\n", i, ((i+1) & 0xFF));
	printf("* ) A=1 ;;\n");
	printf("esac\n");
	printf("}\n");

	printf("dec() {\n");
	printf("case \"$A\" in\n");
	for(i=0; i<256; i++)
	    printf("%d ) A=%d ;;\n", i, ((i-1) & 0xFF));
	printf("* ) A=255 ;;\n");
	printf("esac\n");
	printf("}\n");

	printf("%s\n",

"\n"	    "r(){"
"\n"	    "B="
"\n"	    "while [ ${#P} -gt 0 ]"
"\n"	    "do"
"\n"	    "    C=$P"
"\n"	    "    P=${P#?}"
"\n"	    "    case \"$C\" in"
"\n"	    "    9* ) B=${B}0 ;;"
"\n"	    "    1* ) B=${B}2 ; break ;;"
"\n"	    "    2* ) B=${B}3 ; break ;;"
"\n"	    "    3* ) B=${B}4 ; break ;;"
"\n"	    "    4* ) B=${B}5 ; break ;;"
"\n"	    "    5* ) B=${B}6 ; break ;;"
"\n"	    "    6* ) B=${B}7 ; break ;;"
"\n"	    "    7* ) B=${B}8 ; break ;;"
"\n"	    "    8* ) B=${B}9 ; break ;;"
"\n"	    "    * ) B=${B}1 ; break ;;"
"\n"	    "    esac"
"\n"	    "done"
"\n"	    "P=$B$P"
"\n"	    "}"
"\n"	    ""
"\n"	    "l(){"
"\n"	    "B="
"\n"	    "while [ ${#P} -gt 0 ]"
"\n"	    "do"
"\n"	    "    C=$P"
"\n"	    "    P=${P#?}"
"\n"	    "    case \"$C\" in"
"\n"	    "    0* ) B=${B}9 ;;"
"\n"	    "    1* ) B=${B}0 ; break ;;"
"\n"	    "    2* ) B=${B}1 ; break ;;"
"\n"	    "    3* ) B=${B}2 ; break ;;"
"\n"	    "    4* ) B=${B}3 ; break ;;"
"\n"	    "    5* ) B=${B}4 ; break ;;"
"\n"	    "    6* ) B=${B}5 ; break ;;"
"\n"	    "    7* ) B=${B}6 ; break ;;"
"\n"	    "    8* ) B=${B}7 ; break ;;"
"\n"	    "    9* ) B=${B}8 ; break ;;"
"\n"	    "    esac"
"\n"	    "done"
"\n"	    "P=$B$P"
"\n"	    "}"
	    );

	if(do_output) {
	    printf("\n");
	    printf("o() {\n");
	    printf("eval \"A=\\$M$P\"\n");
	    printf("case \"$A\" in\n");
	    for(i=0; i<256; i++) {
		if (i >= ' ' && i <= '~' && i != '\'' && i != '\\')
		    printf("%d ) echo -n '%c' ;;\n", i, i);
		else if (i == 10 )
		    printf("%d ) echo ;;\n", i);
		else
		    printf("%d ) echoe '\\%03o' ;;\n", i, i);
	    }
	    printf("esac\n}\n");
	    printf("%s\n",
"\n"		"if [ .`echo -n` = .-n ]"
"\n"		"then"
"\n"		"    echon() { echo \"$1\\c\"; }"
"\n"		"    echoe() { echo \"$1\\c\"; }"
"\n"		"else"
"\n"		"    echon() { echo -n \"$1\"; }"
"\n"		"    if [ .`echo -e` = .-e ]"
"\n"		"    then echoe() { echo -n \"$1\"; }"
"\n"		"    else echoe() { echo -n -e \"$1\"; }"
"\n"		"    fi"
"\n"		"fi"
		);
	}

	if (do_input) {
	    printf("%s\n",
		"\n"	"i() {"
		"\n"	"    [ \"$goteof\" = \"y\" ] && return;"
		"\n"	"    [ \"$gotline\" != \"y\" ] && {"
		"\n"	"        if read -r line"
		"\n"	"        then"
		"\n"	"            gotline=y"
		"\n"	"        else"
		"\n"	"            goteof=y"
		"\n"	"            return"
		"\n"	"        fi"
		"\n"	"    }"
		"\n"	"    [ \"$line\" = \"\" ] && {"
		"\n"	"        gotline=n"
		"\n"	"        eval \"M$P=10\""
		"\n"	"        return"
		"\n"	"    }"
		"\n"	"    A=\"$line\""
		"\n"	"    while [ ${#A} -gt 1 ] ; do A=\"${A%?}\"; done"
		"\n"	"    line=\"${line#?}\""
		"\n"	"# This printf command is probably not portable"
		"\n"	"    A=`printf %d \\'\"$A\"`"
		"\n"	"    eval \"M$P=$A\""
		"\n"	"}"
	    );
	}

	printf("\nP=00000\nbf\n");
	break;
    }
}

#if 0
/* Simple filter to find [-] sequences */
void
outcmd(int ch, int count)
{
static int zstate = 0;

    switch(zstate)
    {
    case 1:
	if (count == 1 && ch == '-') { zstate++; return; }
	outcmd2('[', 1);
	break;
    case 2:
	if (count == 1 && ch == ']') { zstate=0; outcmd2('=', 1); return; }
	outcmd2('[', 1);
	outcmd2('-', 1);
	break;
    }
    zstate=0;
    if (count == 1 && ch == '[') { zstate++; return; }
    outcmd2(ch, count);
}
#endif
