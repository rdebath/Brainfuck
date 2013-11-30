#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/*
 * Pascal(fpc) translation from BF, runs at about 2,400,000,000 instructions per second.
 *
 * Large pgms give: "Fatal: Procedure too complex, it requires too many registers"
 */

extern int bytecell;

int do_input = 0;
int ind = 1;
#define prv(s,v)        printf("%*s" s "\n", ind*4, "", (v))
#define pr(s)           printf("%*s" s "\n", ind*4, "")

int
check_arg(char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	pr("var");
	ind++;
	if (bytecell)
	    pr("tape : packed array [0..65536] of byte;");
	else
	    pr("tape : packed array [0..65536] of longint;");
	pr("tapepos : longint;");
	pr("v : longint;");
	pr("inch : char;");
	ind --;
	pr("begin");
	ind ++;
	prv("tapepos := %d;", BOFF);
	break;

    case '=': prv("tape[tapepos] := %d;", count); break;
    case 'B': pr("v := tape[tapepos];"); break;
    case 'M': prv("tape[tapepos] := tape[tapepos] + %d*v;", count); break;
    case 'N': prv("tape[tapepos] := tape[tapepos] - %d*v;", count); break;
    case 'S': pr("tape[tapepos] := tape[tapepos] + v;"); break;
    case 'Q': prv("if v <> 0 then tape[tapepos] := %d*v;", count); break;
    case 'm': prv("if v <> 0 then tape[tapepos] := tape[tapepos] + %d*v;", count); break;
    case 'n': prv("if v <> 0 then tape[tapepos] := tape[tapepos] - %d*v;", count); break;
    case 's': pr("if v <> 0 then tape[tapepos] := tape[tapepos] + v;"); break;

    case '+': prv("tape[tapepos] := tape[tapepos] + %d;", count); break;
    case '-': prv("tape[tapepos] := tape[tapepos] - %d;", count); break;
    case '<': prv("tapepos := tapepos - %d;", count); break;
    case '>': prv("tapepos := tapepos + %d;", count); break;
    case '[': pr("while tape[tapepos] <> 0 do begin"); ind++; break;
    case 'X': pr("{Infinite Loop};"); break;
    case ']': ind--; pr("end;"); break;
    case ',': pr("if not eof then begin read(inch); tape[tapepos] := ord(inch); end;"); break;
    case '.': pr("write(chr(tape[tapepos]));"); break;
    case '~': ind --; pr("end."); break;
    }
}
