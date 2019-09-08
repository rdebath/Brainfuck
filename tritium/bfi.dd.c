/*
 *  Convert tree to code for bf2any's "be-pipe"
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "bfi.tree.h"
#include "bfi.run.h"
#include "bfi.dd.h"

static void outcmd(int ch, int count);

static char * strbuf;
static unsigned int maxstrlen = 0, buflen = 0;

void
print_dd(void)
{
    struct bfi * n = bfprog;

    printf("{[ Code generated from %s ]}\n\n", bfname);

    puts("{[    This is not brainfuck, use bf2any's -be-pipe option     ]}");
    puts("{ ++++[>++++<-]>[>++>[++++++++>]++[<]>-]>>>>>>>++.<<<--.+.<<-- }");
    puts("{ -----.<.>>>.<<.<.>>----.+.<+.<.>>>>.<<<--.>>>-.<.<-.>---.<<+ }");
    puts("{ ++.>>---.<---.<<++++++++++++.------------.>.--.>>++.<<<.++++ }");
    puts("{ +++++++++.>>>>+.<.<<<.>---.>--.<.>>.[>]<<.                [] }");
    puts("\n");


    if (cell_size > 0)
	outcmd('%', cell_size);

    if (node_type_counts[T_MOV] != 0 && most_neg_maad_loop)
	outcmd('>', -most_neg_maad_loop);

    while(n)
    {
	switch(n->type)
	{
	case T_MOV:
	    outcmd('>', n->count);
	    break;

	case T_ADD:
	    outcmd('>', n->offset);
	    outcmd('+', n->count);
	    outcmd('<', n->offset);
	    break;

	case T_SET:
	    outcmd('>', n->offset);
	    outcmd('=', n->count);
	    outcmd('<', n->offset);
	    break;

	case T_CALC:
	    /* p[offset] = count + count2 * p[offset2] + count3 * p[offset3] */

	    if (n->count3 != 0 && n->offset == n->offset3) {
		fprintf(stderr, "Encoding error on code generation: "
			"%s\t"
			"%d,%d,%d,%d,%d,%d\n",
			tokennames[n->type],
			n->offset, n->count,
			n->offset2, n->count2,
			n->offset3, n->count3);
		exit(1);
	    }

	    if (n->offset == n->offset2 && n->count2 != 0) {
		int c = n->count2-1;
		if (c) {
		    outcmd('>', n->offset2);
		    outcmd('B', 0);
		    outcmd('<', n->offset2);
		    outcmd('>', n->offset);
		    outcmd('M', c);
		    outcmd('<', n->offset);
		}

		if (n->count != 0) {
		    outcmd('>', n->offset);
		    outcmd('+', n->count);
		    outcmd('<', n->offset);
		}
	    } else {
		outcmd('>', n->offset2);
		outcmd('B', 0);
		outcmd('<', n->offset2);

		outcmd('>', n->offset);
		if (n->count) {
		    outcmd('=', n->count);
		    outcmd('M', n->count2);
		} else {
		    outcmd('C', n->count2);
		}
		outcmd('<', n->offset);
	    }

	    if (n->count3) {
		outcmd('>', n->offset3);
		outcmd('B', 0);
		outcmd('<', n->offset3);

		outcmd('>', n->offset);
		outcmd('M', n->count3);
		outcmd('<', n->offset);
	    }
	    break;

	case T_CALCMULT:
	    if (n->count == 0 && n->count2 == 1 && n->count3 == 1) {
		if (n->offset != n->offset2) {
		    outcmd('>', n->offset2);
		    outcmd('B', 0);
		    outcmd('<', n->offset2);
		    outcmd('>', n->offset);
		    outcmd('C', 0);
		    outcmd('<', n->offset);
		}
		outcmd('>', n->offset3);
		outcmd('B', 0);
		outcmd('<', n->offset3);
		outcmd('>', n->offset);
		outcmd('*', 0);
		outcmd('<', n->offset);
		break;
	    }

	    fprintf(stderr, "Error on code generation:\n"
		   "Bad T_CALCMULT node with incorrect counts.\n");
	    exit(99);
	    break;

	case T_IF:
	    outcmd('>', n->offset);
	    outcmd('I', 0);
	    outcmd('<', n->offset);
	    break;

	case T_MULT: case T_CMULT: case T_WHL:
	    outcmd('>', n->offset);
	    outcmd('[', 0);
	    outcmd('<', n->offset);
	    break;

	case T_END:
	    outcmd('>', n->offset);
	    outcmd(']', 0);
	    outcmd('<', n->offset);
	    break;

	case T_ENDIF:
	    outcmd('>', n->offset);
	    outcmd('E', 0);
	    outcmd('<', n->offset);
	    break;

	case T_PRT:
	    outcmd('>', n->offset);
	    outcmd('.', 0);
	    outcmd('<', n->offset);
	    break;

	case T_PRTI:
	    outcmd('>', n->offset);
	    outcmd(':', 0);
	    outcmd('<', n->offset);
	    break;

	case T_CHR:
	    {
		unsigned i = 0;
		struct bfi * v = n;
		while(v && v->type == T_CHR) {

		    if (i+2 > maxstrlen) {
			if (maxstrlen) maxstrlen *= 2; else maxstrlen = 4096;
			strbuf = realloc(strbuf, maxstrlen);
			if (!strbuf) {
			    fprintf(stderr, "Reallocate of string buffer failed\n");
			    exit(42);
			}
		    }

		    strbuf[i++] = (char) /*GCC -Wconversion*/ v->count;
		    n = v;
		    v = v->next;
		}
		strbuf[i] = 0;
		buflen = i;
	    }
	    outcmd('"', 0);
	    break;

	case T_INP:
	    outcmd('>', n->offset);
	    if (eofcell == 3)
		outcmd('=', 0);
	    else if (eofcell == 2 || eofcell == 4)
		outcmd('=', -1);
	    outcmd(',', 0);
	    outcmd('<', n->offset);
	    break;

	case T_INPI:
	    outcmd('>', n->offset);
	    outcmd(';', 0);
	    outcmd('<', n->offset);
	    break;

	case T_STOP:
	    outcmd('X', 0);
	    break;

	case T_DUMP:
	    outcmd('#', 0);
	    break;

	case T_NOP:
	    fprintf(stderr, "Warning on code generation: "
		    "%s node: ptr+%d, cnt=%d, @(%d,%d).\n",
		    tokennames[n->type],
		    n->offset, n->count, n->line, n->col);
	    break;

	default:
	    fprintf(stderr, "Code gen error: "
		    "%s\t"
		    "%d,%d,%d,%d,%d,%d\n",
		    tokennames[n->type],
		    n->offset, n->count,
		    n->offset2, n->count2,
		    n->offset3, n->count3);
	    exit(1);
	}
	n=n->next;
    }
    outcmd('~', 0);
}

static int col = 0;
static int maxcol = 72;
static int pending_offset = 0;

static void ddump(int ch, int count);

void outcmd(int ch, int count)
{
    if (ch == '~') {
	if (col)
	    printf("\n");
	col = 0;
	return;
    }

    if (ch == '<') { ch = '>'; count = -count; }
    if (ch == '>') { pending_offset += count; return; }

    if (pending_offset > 0) { ddump('>', pending_offset); }
    if (pending_offset < 0) { ddump('<', -pending_offset); }
    pending_offset = 0;

    if (ch == '+' && count < 0) { ch = '-'; count = -count; }
    if (ch == '-' && count <= 0) { ch = '+'; count = -count; }
    if (ch == 'M' && count < 0) { ch = 'N'; count = -count; }
    if (ch == 'M' && count == 1) { ch = 'S'; count = 0; }
    if (ch == 'M' && count == -1) { ch = 'T'; count = 0; }

    if (ch == '>' && count == 0) return;
    if (ch == '+' && count == 0) return;
    if (ch == 'M' && count == 0) return;

    ddump(ch, count);
}

static void
ddump(int ch, int count)
{
    char ibuf[sizeof(int)*3+6];
    int l;

    if (ch == '"') {
	int sch; unsigned i = 0;
	if (buflen == 0) return;

	if (col+60>maxcol) {
	    printf("\n");
	    col = 0;
	}

	putchar('"'); col++;
	for(i=0; i<buflen; i++) {
	    sch = strbuf[i];
	    if (col > maxcol-(sch == '"')-2 + 80) {
		printf("\"\n\"");
		col = 1;
	    }
	    if (sch == '"') {
		putchar(sch);
		putchar(sch);
		col+=2;
	    } else if (sch == '\n') {
		putchar(sch);
		col = 0;
	    } else {
		putchar(sch);
		col++;
	    }
	}
	col++;
	putchar('"');
	return;

    } else if (ch == '=' && count < 0) {
	l = sprintf(ibuf, "%d~=", ~count);
    } else if (count == 0 || (ch != '=' && count == 1)) {
	l = sprintf(ibuf, "%c", ch);
    } else if (count > 0) {
	l = sprintf(ibuf, "%d%c", count, ch);
    } else {
	l = sprintf(ibuf, "%d~%c", ~count, ch);
    }

    if (col + l > maxcol) {
	printf("\n");
	col = 0;
    }
    printf("%s", ibuf);
    col += l;
}
