#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Pascal(fpc) translation from BF, runs at about 2,400,000,000 instructions per second.
 *
 * Large pgms give: "Fatal: Procedure too complex, it requires too many registers"
 */

#define MAXWHILE 50

struct instruction {
    int ch;
    int count;
    int ino;
    int icount;
    struct instruction * next;
    struct instruction * loop;
} *pgm = 0, *last = 0, *jmpstack = 0;

void loutcmd(int ch, int count, struct instruction *n);

int do_input = 0;
int ind = 0;

#define prv(s,v)        printf("%*s" s "\n", ind*4, "", (v))
#define pr(s)           printf("%*s" s "\n", ind*4, "")

static int lblcount = 0;
static int icount = 0;

int disable_savestring = 1;

void
outcmd(int ch, int count)
{
    struct instruction * n = calloc(1, sizeof*n), *n2;
    if (!n) { perror("bf2pas"); exit(42); }

    icount ++;
    n->ch = ch;
    n->count = count;
    n->icount = icount;
    if (!last) {
	pgm = n;
    } else {
	last->next = n;
    }
    last = n;

    if (n->ch == '[') {
	n->ino = ++lblcount;
	n->loop = jmpstack; jmpstack = n;
    } else if (n->ch == ']') {
	n->loop = jmpstack; jmpstack = jmpstack->loop; n->loop->loop = n;
    }

    if (ch != '~') return;

    loutcmd(0, 0, 0);

    if (icount < MAXWHILE) {
	for(n=pgm; n; n=n->next)
	    loutcmd(n->ch, n->count, n);
    } else {
	for(n=pgm; n; n=n->next) {
	    if (n->ch != ']') continue;
	    if (n->icount-n->loop->icount <= MAXWHILE) continue;
	    loutcmd(1000, 1, n->loop);
	    for(n2 = n->loop->next; n != n2; n2=n2->next) {
		if (n2->ch == '[' && n2->loop->icount-n2->icount > MAXWHILE) {
		    loutcmd(n2->ch, n2->count, n);
		    prv("bf%d()", n2->ino);
		    n2 = n2->loop;
		    loutcmd(n2->ch, n2->count, n);
		} else
		    loutcmd(n2->ch, n2->count, n2);
	    }
	    loutcmd(1001, 1, n);
	}

	for(n=pgm; n; n=n->next) {
	    if (n->ch != '[' || n->loop->icount-n->icount <= MAXWHILE)
		loutcmd(n->ch, n->count, n);
	    else {
		loutcmd(n->ch, n->count, n);
		prv("bf%d()", n->ino);
		n=n->loop;
		loutcmd(n->ch, n->count, n);
	    }
	}
    }

    while(pgm) {
	n = pgm;
	pgm = pgm->next;
	memset(n, '\0', sizeof*n);
	free(n);
    }
}

void
loutcmd(int ch, int count, struct instruction *n)
{
    switch(ch) {
    case 1000:
	prv("procedure bf%d();", n->ino);
	pr("begin");
	ind++;
	break;

    case 1001:
	ind--;
	pr("end;");
	break;

    case 0:
	pr("var");
	ind++;
	if (bytecell)
	    prv("tape : packed array [0..%d] of byte;", tapesz);
	else
	    prv("tape : packed array [0..%d] of longint;", tapesz);
	pr("tapepos : longint;");
	pr("v : longint;");
	pr("inch : char;");
	ind --;
	pr("");
	break;

    case '!':
	pr("begin");
	ind ++;
	prv("tapepos := %d;", tapeinit);
	break;

    case '=': prv("tape[tapepos] := %d;", count); break;
    case 'B': pr("v := tape[tapepos];"); break;
    case 'M': prv("tape[tapepos] := tape[tapepos] + %d*v;", count); break;
    case 'N': prv("tape[tapepos] := tape[tapepos] - %d*v;", count); break;
    case 'S': pr("tape[tapepos] := tape[tapepos] + v;"); break;
    case 'Q': prv("if v <> 0 then tape[tapepos] := %d;", count); break;
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

