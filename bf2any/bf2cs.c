#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * C Sharp translation from BF, runs at about 1,400,000,000 instructions per second.
 */

static int ind = 0, utf_32 = 0;
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
	pr(" public static void Main() {");
	pr("  unchecked {");
	ind++;
	if (!count) {
	    prv("var M = new ulong[%d];", tapesz);
	    prv("var P = %d;", tapeinit);
	    pr("ulong V = 0;");
	    pr("int InpCh = 0;");
	    pr("V = V|0;");
	    pr("InpCh = InpCh|0;");
	}
	break;

    case 'X': pr("throw new System.InvalidOperationException(\"Infinite Loop detected.\");"); break;

    case '=': prv("M[P] = (ulong)%d;", count); break;
    case 'B': pr("V = M[P];"); break;
    case 'M': prv("M[P] += V*(ulong)%d;", count); break;
    case 'N': prv("M[P] -= V*(ulong)%d;", count); break;
    case 'S': pr("M[P] += V;"); break;
    case 'T': pr("M[P] -= V;"); break;
    case '*': pr("M[P] *= V;"); break;
    case '/': pr("M[P] /= V;"); break;
    case '%': pr("M[P] %%= V;"); break;

    case 'C': prv("M[P] = V*(ulong)%d;", count); break;
    case 'D': prv("M[P] = 0 - V*(ulong)%d;", count); break;
    case 'V': pr("M[P] = V;"); break;
    case 'W': pr("M[P] = 0 - V;"); break;

    case '+': prv("M[P] += (ulong)%d;", count); break;
    case '-': prv("M[P] -= (ulong)%d;", count); break;
    case '>': prv("P += %d;", count); break;
    case '<': prv("P -= %d;", count); break;
    case '.':
	if (bytecell)
	    pr("Console.Write((Char)(byte)M[P]);\n");
	else if (utf_32)
	    prv("Console.Write(Char.ConvertFromUtf32(%s));",
		"(ulong)M[P]>0x10FFFF?0xFFFD:(int)M[P]");
	else
	    pr("Console.Write((Char)((ulong)M[P]>0xFFFF?0xFFFD:M[P]));\n");
	break;
    case ',':
	pr("InpCh = Console.Read();");
	pr("if(InpCh != -1) { M[P] = (ulong) InpCh; }");
	break;
    case '"': print_cstring(strn); break;

    case '[': prv("while(%sM[P]!=0) {", bytecell?"(byte)":""); ind++; break;
    case ']': ind--; pr("}"); break;

    case 'I': prv("if(%sM[P]!=0) {", bytecell?"(byte)":""); ind++; break;
    case 'E': ind--; pr("}"); break;

    case '~': ind--; pr("  }") ; pr(" }"); pr("}"); break;
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
	    prv("Console.Write(\"%s\");", buf);
	    gotnl = 0; outlen = 0;
	}
	if (badchar) {
	    prv("Console.Write((Char)%d);", badchar);
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
