#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * C Sharp translation from BF, runs at about 1,400,000,000 instructions per second.
 */

static int ind = 0;
#define prv(s,v)        printf("%*s" s "\n", ind*4, "", (v))
#define pr(s)           printf("%*s" s "\n", ind*4, "")

static gen_code_t gen_code;
struct be_interface_s be_interface = {.gen_code=gen_code};

static void print_cstring(char * str);

static void
gen_code(int ch, int count, char * strn)
{
    switch(ch) {
    case '!':
	pr("using System;");
	pr("class Program {");
	ind++;
	pr("public static void Main() {");
	ind++;
	if (!count) {
	    prv("var M = new long[%d];", tapesz);
	    prv("var P = %d;", tapeinit);
	    pr("long V = 0;");
	}
	break;

    case 'X': pr("throw new System.InvalidOperationException(\"Infinite Loop detected.\");"); break;

    case '=': prv("M[P] = %d;", count); break;
    case 'B': pr("V = M[P];"); break;
    case 'M': prv("M[P] += V*%d;", count); break;
    case 'N': prv("M[P] -= V*%d;", count); break;
    case 'S': pr("M[P] += V;"); break;
    case 'T': pr("M[P] -= V;"); break;
    case '*': pr("M[P] *= V;"); break;

    case 'C': prv("M[P] = V*%d;", count); break;
    case 'D': prv("M[P] = -V*%d;", count); break;
    case 'V': pr("M[P] = V;"); break;
    case 'W': pr("M[P] = -V;"); break;

    case '+': prv("M[P] += %d;", count); break;
    case '-': prv("M[P] -= %d;", count); break;
    case '>': prv("P += %d;", count); break;
    case '<': prv("P -= %d;", count); break;
    case '.':
	prv("Console.Write(Char.ConvertFromUtf32(%s));",
	    bytecell?"(byte)M[P]":"(ulong)M[P]>0x10FFFF?0xFFFD:(int)M[P]");
	break;
    case ',': pr("M[P]=Console.Read();"); break;
    case '"': print_cstring(strn); break;

    case '[': prv("while(%sM[P]!=0) {", bytecell?"(byte)":""); ind++; break;
    case ']': ind--; pr("}"); break;

    case 'I': prv("if(%sM[P]!=0) {", bytecell?"(byte)":""); ind++; break;
    case 'E': ind--; pr("}"); break;

    case '~': ind--; pr("}"); ind--; pr("}"); break;
    }
}

static void
print_cstring(char * str)
{
    char buf[256];
    int gotnl = 0;
    int badchar = 0;
    size_t outlen = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || badchar || gotnl || outlen > sizeof(buf)-8))
	{
	    buf[outlen] = 0;
	    if (gotnl) {
		buf[outlen-2] = 0;
		prv("Console.Write(\"%s\\n\");", buf);
	    } else {
		prv("Console.Write(\"%s\");", buf);
	    }
	    gotnl = 0; outlen = 0;
	}
	if (badchar) {
	    prv("Console.Write((char)%d);", badchar);
	    badchar = 0;
	}
	if (!*str) break;

	if (*str == '"' || *str == '\\') {
	    buf[outlen++] = '\\'; buf[outlen++] = *str;
	} else if (*str >= ' ' && *str <= '~') {
	    buf[outlen++] = *str;
	} else if (*str == '\n') {
	    gotnl = 1; buf[outlen++] = '\\'; buf[outlen++] = 'n';
	} else if (*str == '\t') {
	    buf[outlen++] = '\\'; buf[outlen++] = 't';
	} else
	    badchar = *str;
    }
}
