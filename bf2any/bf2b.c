#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * B translation from BF, runs at about 640,000,000 instructions per second.
 *
 * Compiled using ABC -- https://github.com/aap/abc
 */

static int ind = 1;
#define prv(s,v)        printf("%*s" s "\n", ind*4, "", (v))
#define pr(s)           printf("%*s" s "\n", ind*4, "")

static gen_code_t gen_code;
struct be_interface_s be_interface = {.gen_code=gen_code};

static void print_string(char * str);

static void
gen_code(int ch, int count, char * strn)
{
    switch(ch) {
    case '!':
	printf( "%s%d%s",
	    "t[",tapesz,"];\n"
	    "main()\n{\n"
	    "    extrn t;\n"
	    "    auto v;\n"
	    );
	if (tapeinit)
	    printf( "%s%d%s", "    t =+ ",tapeinit,";\n");
	break;
    case '~':
	pr("return(0);");
	printf("}\n");
	break;

    case 'X':
	/* Note: FD2 is Unix specific (works with ABC's brt.s ) */
	pr("write(2, \"Infinite Loop*n\", 14);");
	pr("return(1);");
	break;

    case '=': prv("*t = %d;", count); break;
    case 'B':
	if(bytecell) pr("*t =& 0377;");
	pr("v = *t;");
	break;
    case 'M': prv("*t =+ v * %d;", count); break;
    case 'N': prv("*t =- v * %d;", count); break;
    case 'S': pr("*t =+ v;"); break;
    case 'T': pr("*t =- v;"); break;
    case '*': pr("*t =* v;"); break;
    case '/': pr("*t =/ v;"); break;
    case '%': pr("*t =%% v;"); break;

    case 'C': prv("*t = v * %d;", count); break;
    case 'D': prv("*t = -v * %d;", count); break;
    case 'V': pr("*t = v;"); break;
    case 'W': pr("*t = -v;"); break;

    case '+': prv("*t =+ %d;", count); break;
    case '-': prv("*t =- %d;", count); break;
    case '>': prv("t =+ %d;", count); break;
    case '<': prv("t =- %d;", count); break;
    case '.': pr("putchar(*t & 0377);"); break;
    case ',':
	    pr("*t = getchar();");
	    break;
    case '"': print_string(strn); break;

    case '[':
	if(bytecell) pr("*t =& 0377;");
	pr("while( *t != 0 ) {");
	ind++;
	break;
    case ']':
	if(bytecell) pr("*t =& 0377;");
	ind--;
	pr("}");
	break;

    case 'I':
	if(bytecell) pr("*t =& 0377;");
	pr("if( *t != 0 ) {");
	ind++;
	break;
    case 'E':
	ind--;
	pr("}");
	break;
    }
}

static void
print_string(char * str)
{
    char buf[256];
    int badchar = 0;
    size_t outlen = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || badchar || outlen > sizeof(buf)-8))
	{
	    buf[outlen] = 0;
	    prv("putstr(\"%s\");", buf);
	    outlen = 0;
	}
	if (badchar) {
	    prv("putchar(%d);", badchar);
	    badchar = 0;
	}
	if (!*str) break;

	if (*str >= ' ' && *str <= '~' && *str != '\'' && *str != '\"' && *str != '*') {
	    buf[outlen++] = *str;
	} else if (*str == '\'' || *str == '\"' || *str == '*') {
	    buf[outlen++] = '*';
	    buf[outlen++] = *str;
	} else if (*str == '\n') {
	    buf[outlen++] = '*';
	    buf[outlen++] = 'n';
	} else if (*str == '\t') {
	    buf[outlen++] = '*';
	    buf[outlen++] = 't';
	} else {
	    badchar = (*str & 0xFF);
	}
    }
}
