#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * C Sharp translation from BF, runs at about 1,400,000,000 instructions per second.
 */

int ind = 0;
#define prv(s,v)        printf("%*s" s "\n", ind*4, "", (v))
#define pr(s)           printf("%*s" s "\n", ind*4, "")

int disable_savestring = 1;

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	pr("using System;");
	pr("class Program {");
	ind++;
	pr("public static void Main() {");
	ind++;
	prv("var M = new long[%d];", tapesz);
	prv("var P = %d;", tapeinit);
	pr("long V = 0;");
	break;

    case 'X': pr("throw new System.InvalidOperationException(\"Infinite Loop detected.\");"); break;

    case '=': prv("M[P] = %d;", count); break;
    case 'B':
	if(bytecell) pr("M[P] = (M[P] & 255);");
	pr("V = M[P];");
	break;
    case 'M': prv("M[P] += V*%d;", count); break;
    case 'N': prv("M[P] -= V*%d;", count); break;
    case 'S': pr("M[P] += V;"); break;
    case 'Q': prv("if(V!=0) M[P] = %d;", count); break;
    case 'm': prv("if(V!=0) M[P] += V*%d;", count); break;
    case 'n': prv("if(V!=0) M[P] -= V*%d;", count); break;
    case 's': pr("if(V!=0) M[P] += V;"); break;

    case '+': prv("M[P] += %d;", count); break;
    case '-': prv("M[P] -= %d;", count); break;
    case '>': prv("P += %d;", count); break;
    case '<': prv("P -= %d;", count); break;
    case '.': pr("Console.Write((char)M[P]);"); break;
    case ',': pr("M[P]=Console.Read();"); break;

    case '[':
	if(bytecell) pr("M[P] = (M[P] & 255);");
	pr("while(M[P]!=0) {");
	ind++;
	break;
    case ']':
	if(bytecell) pr("M[P] = (M[P] & 255);");
	ind--;
	pr("}");
	break;

    case '~':
	ind--; pr("}");
	ind--; pr("}");
	break;
    }
}
