
#ifdef __STRICT_ANSI__
#define _POSIX_C_SOURCE 200809UL
#if __STDC_VERSION__ < 199901L
#error This program needs at least the C99 standard.
#endif
#else
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "bfi.tree.h"
#include "bfi.run.h"

#if MASK == 0xFF
#define icell	unsigned char
#define M(x) x
#elif MASK == 0xFFFF
#define icell	unsigned short
#define M(x) x
#else
#define icell	int
#endif
void run_progarray(int * progarray);

void
convert_tree_to_runarray(void)
{
    struct bfi * n = bfprog;
    int arraylen = 0;
    int * progarray = 0;
    int * p;
    int last_offset = 0;
#ifdef M
    if (sizeof(*m)*8 != cell_size && cell_size>0) {
	fprintf(stderr, "Sorry, the interpreter has been configured with a fixed cell size of %d bits\n", sizeof(icell)*8);
	exit(1);
    }
#else
#define M(x) UM(x)
#endif

    while(n)
    {
	switch(n->type)
	{
	case T_MOV:
	    break;

	case T_CALC:
	    arraylen += 7;
	    break;

	default:
	    arraylen += 3;
	    break;
	}
	n = n->next;
    }

    p = progarray = tcalloc(arraylen+2, sizeof*progarray);
    n = bfprog;

    last_offset = 0;
    while(n)
    {
	if (n->type != T_MOV) {
	    *p++ = (n->offset - last_offset);
	    last_offset = n->offset;
	}
	else {
	    last_offset -= n->count;
	    n = n->next;
	    continue;
	}

	*p++ = n->type;
	switch(n->type)
	{
	case T_INP:
	    break;

	case T_PRT: case T_ADD: case T_SET:
	    *p++ = n->count;
	    break;

	case T_ADDWZ:
	    *p++ = n->next->offset - last_offset;
	    *p++ = n->next->count;
	    *p++ = n->next->next->count;
	    n = n->jmp;
	    break;

	case T_ZFIND:
	    *p++ = n->next->count;
	    n = n->jmp;
	    break;

	case T_MFIND:
	    *p++ = n->next->next->next->count;
	    n = n->jmp;
	    break;

	case T_MULT: case T_CMULT:
	    /* Note: for these I could generate multiply code for then enclosed
	     * T_ADD instructions, but that will only happen if the optimiser
	     * section is turned off.
	     */
	case T_FOR: case T_IF:
	    p[-1] = T_WHL;
	    /*FALLTHROUGH*/

	case T_WHL:
	    /* Storing the location of the instruction in the T_END's count
	     * field; it's not normally used */
	    n->jmp->count = p-progarray;
	    *p++ = 0;
	    break;

	case T_ENDIF:
	    progarray[n->count] = (p-progarray) - n->count -1;
	    break;

	case T_END:
	    progarray[n->count] = (p-progarray) - n->count;
	    *p++ = -progarray[n->count];
	    break;

	case T_CALC:
	    if (n->count3 == 0) {
		if (n->count == 0 && n->count2 == 1) {
		    /*  m[off] = m[off2] */
		    p[-1] = T_CALC4;
		    *p++ = n->offset2 - last_offset;
		} else {
		    /*  m[off] = m[off2]*count2 + count2 */
		    p[-1] = T_CALC2;
		    *p++ = n->count;
		    *p++ = n->offset2 - last_offset;
		    *p++ = n->count2;
		}
	    } else if ( n->count == 0 && n->count2 == 1 && n->offset == n->offset2 ) {
		if (n->count3 == 1) {
		    /*  m[off] += m[off3] */
		    p[-1] = T_CALC5;
		    *p++ = n->offset3 - last_offset;
		} else {
		    /*  m[off] += m[off3]*count3 */
		    p[-1] = T_CALC3;
		    *p++ = n->offset3 - last_offset;
		    *p++ = n->count3;
		}
	    } else {
		*p++ = n->count;
		*p++ = n->offset2 - last_offset;
		*p++ = n->count2;
		*p++ = n->offset3 - last_offset;
		*p++ = n->count3;
	    }
	    break;

	case T_STOP:
	    break;

	default:
	    fprintf(stderr, "Invalid node type found = %s\n", tokennames[n->type]);
	    exit(1);
	}
	n = n->next;
    }
    *p++ = 0;
    *p++ = T_STOP;

    delete_tree();
    run_progarray(progarray);
    free(progarray);
}

#ifdef __GNUC__
__attribute((optimize(3),hot,aligned(64)))
#endif
void
run_progarray(int * p)
{
    icell * m;
    m = map_hugeram();

    for(;;) {
	m += *p++;
	switch(*p)
	{
	case T_ADD: *m += p[1]; p += 2; break;
	case T_SET: *m = p[1]; p += 2; break;

	case T_END:
	    if(M(*m) != 0) p += p[1];
	    p += 2;
	    break;

	case T_WHL:
	    if(M(*m) == 0) p += p[1];
	    p += 2;
	    break;

	case T_ENDIF:
	    p += 1;
	    break;

	case T_ADDWZ:
	    /* This is normally a running dec, it cleans up a rail */
	    while(M(*m)) {
		m[p[1]] += p[2];
		m += p[3];
	    }
	    p += 4;
	    break;

	case T_ZFIND:
	    /* Search along a rail til you find the end of it. */
	    while(M(*m)) {
		m += p[1];
	    }
	    p += 2;
	    break;

	case T_MFIND:
	    /* Search along a rail for a minus 1 */
	    while(M(*m)) {
		*m -= 1;
		m += p[1];
		*m += 1;
	    }
	    p += 2;
	    break;

	case T_CALC:
	    *m = p[1] + m[p[2]] * p[3] + m[p[4]] * p[5];
	    p += 6;
	    break;

	case T_CALC2:
	    *m = p[1] + m[p[2]] * p[3];
	    p += 4;
	    break;

	case T_CALC3:
	    *m += m[p[1]] * p[2];
	    p += 3;
	    break;

	case T_CALC4:
	    *m = m[p[1]];
	    p += 2;
	    break;

	case T_CALC5:
	    *m += m[p[1]];
	    p += 2;
	    break;

	case T_INP:
	    *m = getch(*m);
	    p += 1;
	    break;

	case T_PRT:
	    {
		int ch = M(p[1] == -1?*m:p[1]);
		putch(ch);
	    }
	    p += 2;
	    break;

	case T_STOP:
	    goto break_break;
	}
    }
break_break:;
#undef icell
#undef M
}
