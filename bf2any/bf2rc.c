#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * AT&T Plan 9 shell (rc) translation from BF, runs at about 120,000 instructions per second.
 *
 * Large input files give:
 *      line 18551: yacc stack overflow near end of line
 * To workaround set a max_prle of 128
 *
 * This back end has no input function.
 * There is no built in function to read a line from the stdin and there is
 * no built in function to extract the first character from a string.
 *
 * I have used an external command pipeline to get the next character
 * from stdin.
 *
 * This does prove the rc shell is Turing complete as the 'left' and 'right'
 * command implementations don't have a defined right hand limit and I/O is
 * not required for TC.
 */

static int do_input = 0;
static int do_output = 0;
static int ind = 0;
static int max_prle = 128;
static int have_fn_e = 0;

static void print_string(char * str);

static gen_code_t gen_code;
struct be_interface_s be_interface = {.gen_code=gen_code,
	.bytesonly=1, .disable_be_optim=1, .enable_chrtok=1};

static void
gen_code(int ch, int count, char * strn)
{
    if (ch == '=') {
        count &= 255;
    } else {

	while (count>max_prle) { gen_code(ch, max_prle, 0); count -= max_prle; }

	if (count > 1) {
	    switch(ch) {
	    case '+': printf("u%d\n", count); break;
	    case '-': printf("d%d\n", count); break;
	    case '>': printf("r%d\n", count); break;
	    case '<': printf("l%d\n", count); break;
	    }
	    return;
	}
    }

    switch(ch) {
    case '"': print_string(strn); break;
    case '=': printf("s %d\n", count); break;
    case '+': printf("u\n"); break;
    case '-': printf("d\n"); break;
    case '>': printf("r\n"); break;
    case '<': printf("l\n"); break;
    case '[':
	printf("f\n");
	printf("while (! ~ $v 0){\n");
	ind++;
	break;
    case ']':
	printf("f\n");
	ind--; printf("}\n");
	break;
    case '.': printf("o\n"); do_output = 1; break;
    case ',': printf("i\n"); do_input = 1; break;
    }

    switch(ch) {
    case '!':
	if (count) break;

	have_fn_e = 1;
	printf("#!/usr/bin/rc\n");


	printf(
	    "ptr=(1)\n"
	    "\n"
	    "fn f {\n"
	    "    p=t$^ptr\n"
	    "    v=$$p\n"
	    "    if (~ $v ()) v=0\n"
	    "}\n"
	    "\n"
	    "fn w {\n"
	    "    $p=$v\n"
	    "}\n"
	    "\n"
	    "fn s {\n"
	    "    f\n"
	    "    v=$1\n"
	    "    w\n"
	    "}\n"
	    "\n"
	    "fn r {\n"
	    "    carry=1\n"
	    "    nptr=()\n"
	    "    for(v in $ptr) {\n"
	    "        if (~ $carry 1) {\n"
	    "            carry=0\n"
	    "            if (~ $v 0) {\n"
	    "                v=1\n"
	    "            } else if (~ $v 9) {\n"
	    "                v=0\n"
	    "                carry=1\n"
	    "            } else {\n"
	    "                n=(2 3 4 5 6 7 8 9)\n"
	    "                v=$n($v)\n"
	    "            }\n"
	    "        }\n"
	    "        nptr=($nptr $v)\n"
	    "    }\n"
	    "    if (~ $carry 1) nptr=($nptr 1)\n"
	    "    ptr=$nptr\n"
	    "}\n"
	    "\n"
	    "fn l {\n"
	    "    carry=1\n"
	    "    nptr=()\n"
	    "    pv=()\n"
	    "    for(v in $ptr) {\n"
	    "        nptr=($nptr $pv)\n"
	    "        if (~ $carry 1) {\n"
	    "            carry=0\n"
	    "            if (~ $v 0) {\n"
	    "                v=9\n"
	    "                carry=1\n"
	    "            } else {\n"
	    "                n=(0 1 2 3 4 5 6 7 8)\n"
	    "                v=$n($v)\n"
	    "            }\n"
	    "        }\n"
	    "        if (~ $v 0) {\n"
	    "            pv=$v\n"
	    "        } else {\n"
	    "            pv=()\n"
	    "            nptr=($nptr $v)\n"
	    "        }\n"
	    "    }\n"
	    "    ptr=$nptr\n"
	    "}\n\n");

	{
	    int i;
	    printf("inc=(\n");
	    for (i=0; i<256; i++) {
		printf(" %d", (i+2)&0xFF);
		if (i%16 == 15)
		    printf("\n");
	    }
	    printf(")\n\n");
	    printf("fn u {\n" "    f\n");
	    printf("    if (~ $v 0) {\n");
	    printf("        v=1\n");
	    printf("    } else {\n");
	    printf("        v=$inc($v)\n");
	    printf("    }\n");
	    printf("    w\n" "}\n" "\n");
	}

	{
	    int i;
	    printf("dec=(\n");
	    for (i=0; i<256; i++) {
		printf(" %d", (i)&0xFF);
		if (i%16 == 15)
		    printf("\n");
	    }
	    printf(")\n\n");
	    printf("fn d {\n" "    f\n");
	    printf("    if (~ $v 0) {\n");
	    printf("        v=255\n");
	    printf("    } else {\n");
	    printf("        v=$dec($v)\n");
	    printf("    }\n");
	    printf("    w\n" "}\n" "\n");
	}

	printf("fn e {\n" "    switch ($1) {\n");
	{
	    int i;
	    for (i=0; i<256; i++) {
		if (i == 10 )
		    printf("    case %d\n\techo\n", i);
		else if (i>= ' ' && i<= '~' && i != '\'')
		    printf("    case %d\n\techo -n '%c'\n", i, i);
		else if (i == '\'')
		    printf("    case %d\n\techo -n ''''\n", i);
	    }
	    printf("    # Use awk for the rest\n");
	    printf("    case *\n\tawk -v 'v='$1 'BEGIN{printf \"%%c\",v;}'\n");
	}
	printf("    }\n" "}\n" "\n");

	printf("fn o {\n" "    f\n" "    e $v\n" "}\n\n");

	printf("fn i {\n");
	printf("    f\n");
	printf("    v=`{dd 'bs=1' 'count=1' >[2]/dev/null | od -t d1 | cut -d ' ' -s -f 2- }\n");
	printf("    w\n");
	printf("}\n" "\n");

	if (max_prle>1) {
	    int i;
	    printf("fn u2 { u ; u; }\n");
	    printf("fn d2 { d ; d; }\n");
	    printf("fn l2 { l ; l; }\n");
	    printf("fn r2 { r ; r; }\n");
	    printf("fn u3 { u2 ; u; }\n");
	    printf("fn d3 { d2 ; d; }\n");
	    printf("fn l3 { l2 ; l; }\n");
	    printf("fn r3 { r2 ; r; }\n");

	    for (i=4; i<=max_prle; i++) {
		printf("fn u%d { u%d ; u%d; }\n", i, i-i/2, i/2);
		printf("fn d%d { d%d ; d%d; }\n", i, i-i/2, i/2);
		printf("fn l%d { l%d ; l%d; }\n", i, i-i/2, i/2);
		printf("fn r%d { r%d ; r%d; }\n", i, i-i/2, i/2);
	    }
	    printf("\n");
	}
	break;

    case '~':
	break;
    }
}

static void
print_string(char * str)
{
    char buf[256];
    int gotnl = 0, badchar = 0;
    size_t outlen = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || gotnl || badchar || outlen > sizeof(buf)-8))
	{
	    if (gotnl) {
		buf[--outlen] = 0;
		if (outlen == 0) {
		    printf("echo\n");
		} else {
		    printf("echo '%s'\n", buf);
		}
	    } else {
		buf[outlen] = 0;
		printf("echo -n '%s'\n", buf);
	    }
	    gotnl = 0; outlen = 0;
	}
	if (badchar) {
	    if (have_fn_e)
		printf("e %d\n", badchar);
	    else if (badchar == '-')
		printf("echo -n '%c'\n", badchar);
	    else
		printf("awk 'BEGIN{printf \"%%c\",%d;}'\n", badchar);
	    badchar = 0;
	    do_output = 1;
	}
	if (!*str) break;

	if (*str == '\n') {
	    gotnl = 1;
	    buf[outlen++] = *str;
	} else if (*str == '-' && outlen == 0) {
	    badchar = (*str & 0xFF);
	} else if (*str >= ' ' && *str <= '~' && *str != '\'') {
	    buf[outlen++] = *str;
	} else if (*str == '\'') {
	    buf[outlen++] = *str;
	    buf[outlen++] = *str;
	} else {
	    badchar = (*str & 0xFF);
	}
    }
}
