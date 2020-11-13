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
static unsigned int buflen = 0;
static int bfrle = 0;

int
checkarg_dd(char * opt, char * arg UNUSED)
{
    if (!strcmp(opt, "-bfrle")) { bfrle = 1; return 1; }
    return 0;
}

void
print_dd(void)
{
    struct bfi * n = bfprog;
    int is_bfrle = (
	(node_type_counts[T_CALC] == 0) &&
	(node_type_counts[T_CALCMULT] == 0) &&
	(node_type_counts[T_CALCMULT] == 0) &&
	(node_type_counts[T_IF] == 0) &&
	(node_type_counts[T_ENDIF] == 0) &&
	(node_type_counts[T_INPI] == 0) &&
	(node_type_counts[T_PRTI] == 0) &&
	(node_type_counts[T_CHR] == 0) &&
	(node_type_counts[T_STR] == 0) &&
	(node_type_counts[T_DIV] == 0) );

    if (bfrle && !is_bfrle)
	fprintf(stderr, "Warning: Additional be-pipe tokens generated.\n");

    if (!noheader && !bfrle) {
	printf("{[ Code generated from %s ]}\n\n", bfname);

	puts("{[    This is not brainfuck, use bf2any's -be-pipe option     ]}");
	puts("{ ++++[>++++<-]>[>++>[++++++++>]++[<]>-]>>>>>>>++.<<<--.+.<<-- }");
	puts("{ -----.<.>>>.<<.<.>>----.+.<+.<.>>>>.<<<--.>>>-.<.<-.>---.<<+ }");
	puts("{ ++.>>---.<---.<<++++++++++++.------------.>.--.>>++.<<<.++++ }");
	puts("{ +++++++++.>>>>+.<.<<<.>---.>--.<.>>.[>]<<.                [] }");
	puts("");

	if (cell_size > 0)
	    outcmd('!', cell_size);
	else if (cell_length > (int)sizeof(int)*CHAR_BIT)
	    outcmd('!', -(int)sizeof(int)*CHAR_BIT);

	if (node_type_counts[T_MOV] != 0 && most_neg_maad_loop)
	    outcmd('>', -most_neg_maad_loop);
    } else if (!noheader && bfrle) {
	printf("{[ Code generated from %s ]}\n\n", bfname);

	puts("{[ This is BF-RLE not brainfuck                               ]}");
	puts("{++2-[[-]                                                      }");
	puts("{ ++++[>++++<-]>[>++>[++++++++>]++[<]>-]>>>>>>>++.<<<--.+.<<-- }");
	puts("{ -----.<.>>>.<<.<.>>----.+.<+.<.>>>>.<<<--.>>>-.<.<-.>---.<<+ }");
	puts("{ ++.>>---.<---.>>>>>>>>>++.[<]>.>>--.<-.<.>>.<-.<.>>>>>>>>.++ }");
	puts("{ ++.>>>+.<<<<<--.>++.>-.[>]<<.                            []] }");
	puts("");
    }

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
	    if (bfrle) {
		outcmd('=', 0);
		outcmd('+', n->count);
	    } else
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

	case T_DIV:
	    // p[n->offset] = UM(p[n->offset]);
	    outcmd('>', n->offset);
	    outcmd('B', 0);
	    outcmd('<', n->offset);
	    // p[n->offset+2] = p[n->offset];
	    outcmd('>', n->offset+2);
	    outcmd('V', 0);
	    outcmd('<', n->offset+2);
	    // p[n->offset+3] = 0;
	    outcmd('>', n->offset+3);
	    outcmd('=', 0);
	    outcmd('<', n->offset+3);

	    // p[n->offset+1] = UM(p[n->offset+1]);
	    // if (p[n->offset+1] != 0) {
	    outcmd('>', n->offset+1);
	    outcmd('I', 0);
	    outcmd('<', n->offset+1);

	    //     p[n->offset+3] = p[n->offset] / p[n->offset+1];
	    outcmd('>', n->offset);
	    outcmd('B', 0);
	    outcmd('<', n->offset);
	    outcmd('>', n->offset+3);
	    outcmd('V', 0);
	    outcmd('<', n->offset+3);
	    outcmd('>', n->offset+1);
	    outcmd('B', 0);
	    outcmd('<', n->offset+1);
	    outcmd('>', n->offset+3);
	    outcmd('/', 0);
	    outcmd('<', n->offset+3);

	    //     p[n->offset+2] = p[n->offset] % p[n->offset+1];
	    outcmd('>', n->offset);
	    outcmd('B', 0);
	    outcmd('<', n->offset);
	    outcmd('>', n->offset+2);
	    outcmd('V', 0);
	    outcmd('<', n->offset+2);
	    outcmd('>', n->offset+1);
	    outcmd('B', 0);
	    outcmd('<', n->offset+1);
	    outcmd('>', n->offset+2);
	    outcmd('%', 0);
	    outcmd('<', n->offset+2);

	    // }
	    outcmd('>', n->offset+1);
	    outcmd('E', 0);
	    outcmd('<', n->offset+1);
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
	    outcmd('B', 0);
	    if (iostyle != 1)
		outcmd('=', n->count & 0xFF);
	    else
		outcmd('=', n->count);
	    outcmd('.', 0);
	    outcmd('V', 0);
	    break;

	case T_STR:
	    {
		buflen = n->str->length;
		strbuf = n->str->buf;
		outcmd('"', 0);
		strbuf = 0;
		if (n->next && n->next->type == T_STR)
		    outcmd(' ', 0); /* Oops, need NOP spacer. */
	    }
	    break;

	case T_INP:
	    outcmd('>', n->offset);
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
    if (ch == 'M' && count == 1) { ch = 'S'; count = 0; }
    if (ch == 'M' && count == -1) { ch = 'T'; count = 0; }
    if (ch == 'M' && count < 0) { ch = 'N'; count = -count; }

    if (ch == 'C' && count == 1) { ch = 'V'; count = 0; }
    if (ch == 'C' && count == -1) { ch = 'W'; count = 0; }
    if (ch == 'C' && count < 0) { ch = 'D'; count = -count; }

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
	int restore_cell = 0;
	int in_q = 0;
	if (buflen == 0) return;

	if (col+60>maxcol) {
	    printf("\n");
	    col = 0;
	}

	for(i=0; i<buflen; i++) {
	    sch = (signed char) strbuf[i];
	    if (iostyle != 1) sch &= 0xFF;
	    if ((sch<32||sch>127) && !(sch == '\n' && in_q)) {
		if (col > maxcol-8 + 80) {
		    if (in_q) putchar('"');
		    putchar('\n');
		    in_q = col = 0;
		}
		if (in_q) { col++; in_q=0; putchar('"'); }
		if (!restore_cell) { restore_cell=1; col++; putchar('B'); }
		if (sch < 0) {
		    col += printf("%d~=", ~sch);
		} else {
		    col += printf("%d=", sch);
		}
		putchar('.');
	    } else {
		if (col > maxcol-(sch == '"')-2-!in_q + 80) {
		    if (in_q) putchar('"');
		    printf("\n\"");
		    col = 1;
		    in_q = 1;
		}
		if (!in_q) { col++; putchar('"'); in_q = 1; }
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
	}
	if (in_q) { col++; putchar('"'); in_q =0; }
	if (restore_cell) { col++; putchar('V'); restore_cell=0; }
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
