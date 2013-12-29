#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
 * AT&T Plan 9 shell (rc) translation from BF, runs really slowly.
 *
 * Large input files give:
 *      line 18551: yacc stack overflow near end of line
 * To workaround set a MAXPRLE of 128
 *
 * This back end has no input function.
 * There is no built in function to read a line from the stdin and there is
 * no built in function to extract the first character from a string.
 *
 * I would have to use something like "dd bs=1 count=1 >[2]/dev/null | od"
 * and so I've not included it.
 */

#define MAXPRLE	1

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
    case '+': printf("u\n"); break;
    case '-': printf("d\n"); break;
    case '>': printf("r\n"); break;
    case '<': printf("l\n"); break;
    case '[':
	printf("f\n");
	printf("while (! ~ $v 0 ) {\n");
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
	printf("#!/usr/bin/rc\n");


	printf(
	    "ptr=(1)\n"
	    "\n"
	    "fn lflat {\n"
	    "  lflat=$*; *=$$1\n"
	    "  while () {\n"
	    "    echo -n $1; shift\n"
	    "    ~ $#* 0 && break\n"
	    "    echo -n $lflat(2)\n"
	    "  }\n"
	    "}\n"
	    "\n"
	    "fn f {\n"
	    "    p=t`{lflat ptr _}\n"
	    "    v=$$p\n"
	    "    if (~ $v ()) v=0\n"
	    "}\n"
	    "\n"
	    "fn s {\n"
	    "    $p=$v\n"
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

	printf("fn u {\n" "    f\n" "    switch ($v) {\n");
	{
	    int i;
	    for (i=0; i<256; i++) {
		printf("    case %d; v=%d\n", i, ((i+1)& 0xFF));
	    }
	    printf("    case *; v=1\n");
	}
	printf("    }\n" "    s\n" "}\n" "\n");

	printf("fn d {\n" "    f\n" "    switch ($v) {\n");
	{
	    int i;
	    for (i=0; i<256; i++) {
		printf("    case %d; v=%d\n", i, ((i-1)& 0xFF));
	    }
	    printf("    case *; v=255\n");
	}
	printf("    }\n" "    s\n" "}\n" "\n");

	printf("fn o {\n" "    f\n" "    switch ($v) {\n");
	{
	    int i;
	    for (i=0; i<256; i++) {
		if (i == 10 )
		    printf("    case %d; echo\n", i);
		else if (i>= ' ' && i<= '~' && i != '\'')
		    printf("    case %d; echo -n '%c'\n", i, i);
		else if (i == '\'')
		    printf("    case %d; echo -n ''''\n", i);
	    }
	}
	printf("    }\n" "}\n" "\n");

	printf("fn i {\n");
	printf("echo STOP command executed >[1=2]\n" "exit\n");
	printf("}\n" "\n");

	if (MAXPRLE>1) {
	    for (i=3; i<=MAXPRLE; i++) {
		printf("fn u%d { u ; u%d; }\n", i, i-1);
		printf("fn d%d { d ; d%d; }\n", i, i-1);
		printf("fn l%d { l ; l%d; }\n", i, i-1);
		printf("fn r%d { r ; r%d; }\n", i, i-1);
	    }

	    printf("fn u2 { u ; u; }\n");
	    printf("fn d2 { d ; d; }\n");
	    printf("fn l2 { l ; l; }\n");
	    printf("fn r2 { r ; r; }\n");
	}
	break;

    case '~':
	break;
    }
}
