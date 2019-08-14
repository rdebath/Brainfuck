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
    char * cstr;
};
static struct instruction *pgm = 0, *last = 0, *jmpstack = 0;

static void loutcmd(int ch, int count, struct instruction *n);

static int ind = 0;

#define prv(s,v)        printf("%*s" s "\n", ind*4, "", (v))
#define pr(s)           printf("%*s" s "\n", ind*4, "")

static int lblcount = 0;
static int icount = 0;
static int has_inp = 0;

static gen_code_t gen_code;
struct be_interface_s be_interface = {.gen_code=gen_code};

static void print_string(char *);

static void
gen_code(int ch, int count, char * strn)
{
    struct instruction * n = calloc(1, sizeof*n), *n2;
    if (!n) { perror("bf2pas"); exit(42); }

    icount ++;
    n->ch = ch;
    n->count = count;
    n->icount = icount;
    if (ch == '"')
	n->cstr=strdup(strn);
    if (!last) {
	pgm = n;
    } else {
	last->next = n;
    }
    last = n;

    if (n->ch == '[' || n->ch == 'I') {
	n->ino = ++lblcount;
	n->loop = jmpstack; jmpstack = n;
    } else if (n->ch == ']' || n->ch == 'E') {
	n->loop = jmpstack; jmpstack = jmpstack->loop; n->loop->loop = n;
    } else if (n->ch == ',')
	has_inp = 1;

    if (ch != '~') return;

    loutcmd(0, 0, 0);

    if (icount < MAXWHILE) {
	for(n=pgm; n; n=n->next)
	    loutcmd(n->ch, n->count, n);
    } else {
	for(n=pgm; n; n=n->next) {
	    if (n->ch != ']' && n->ch != 'E') continue;
	    if (n->icount-n->loop->icount <= MAXWHILE) continue;
	    loutcmd(1000, 1, n->loop);
	    for(n2 = n->loop->next; n != n2; n2=n2->next) {
		if ((n2->ch == '[' || n2->ch == 'I')
			&& n2->loop->icount-n2->icount > MAXWHILE) {
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
	    if ((n->ch != '[' && n->ch != 'I')
		    || n->loop->icount-n->icount <= MAXWHILE)
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
	if (n->cstr)
            free(n->cstr);
	memset(n, '\0', sizeof*n);
	free(n);
    }
}

static void
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
	if (has_inp)
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
    case 'T': pr("tape[tapepos] := tape[tapepos] - v;"); break;
    case '*': pr("tape[tapepos] := tape[tapepos] * v;"); break;

    case 'C': prv("tape[tapepos] := %d*v;", count); break;
    case 'D': prv("tape[tapepos] := %d* -v;", count); break;
    case 'V': pr("tape[tapepos] := v;"); break;
    case 'W': pr("tape[tapepos] := -v;"); break;

    case '+': prv("tape[tapepos] := tape[tapepos] + %d;", count); break;
    case '-': prv("tape[tapepos] := tape[tapepos] - %d;", count); break;
    case '<': prv("tapepos := tapepos - %d;", count); break;
    case '>': prv("tapepos := tapepos + %d;", count); break;
    case 'X': pr("{Infinite Loop};"); break;
    case '[': pr("while tape[tapepos] <> 0 do begin"); ind++; break;
    case ']': ind--; pr("end;"); break;
    case 'I': pr("if tape[tapepos] <> 0 then begin"); ind++; break;
    case 'E': ind--; pr("end;"); break;
    case ',': pr("if not eof then begin read(inch); tape[tapepos] := ord(inch); end;"); break;
    case '.': pr("write(chr(tape[tapepos]));"); break;
    case '"': print_string(n->cstr); break;
    case '~': ind --; pr("end."); break;
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
	    if (badchar == '\n') {
		prv("writeln('%s');", buf);
		badchar = 0;
	    } else {
		prv("write('%s');", buf);
	    }
	    outlen = 0;
	}
	if (badchar) {
	    if (badchar == 10)
		pr("writeln('');");
	    else
		prv("write(chr(%d));", badchar);
	    badchar = 0;
	}
	if (!*str) break;

	if (*str >= ' ' && *str <= '~' && *str != '\\' && *str != '\'') {
	    buf[outlen++] = *str;
	} else if (*str == '\'') {
	    buf[outlen++] = *str;
	    buf[outlen++] = *str;
	} else {
	    badchar = (*str & 0xFF);
	}
    }
}
