/* This is the Hydrogen brainfuck interpreter.
 *
 * Robert de Bath (c) 2013 GPL v2 or later.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOKEN_LIST(Mac) \
    Mac(MOV) Mac(ADD) Mac(PRT) Mac(INP) Mac(WHL) Mac(END) \
    Mac(SET) Mac(ZFIND) Mac(MFIND) Mac(ADDWZ) \
    Mac(MUL) Mac(IFSAV) Mac(ENDIF) \
    Mac(NOP) Mac(STOP) Mac(ERR)

#define GEN_TOK_ENUM(NAME) T_ ## NAME,
enum token { TOKEN_LIST(GEN_TOK_ENUM) };

#define GEN_TOK_STRING(NAME) "T_" #NAME,
const char* tokennames[] = { TOKEN_LIST(GEN_TOK_STRING) };

void *
tcalloc(size_t nmemb, size_t size)
{
    void * m;
    m = calloc(nmemb, size);
    if (m) return m;

    fprintf(stderr, "Allocate of %d*%d bytes failed, ABORT\n", nmemb, size);
    exit(42);
}

char * program = "C";
struct bfi { int type; struct bfi *next, *prev, *jmp; int count; } *pgm;

void run(void);

int main(int argc, char **argv)
{
    FILE * ifd;
    int ch, dld = 0, lid = 0;
    struct bfi *p=0, *n=0, *j=0;
    setbuf(stdout, NULL);
    if (argc < 2 || strcmp(argv[1], "-") == 0) ifd = stdin;
    else if ((ifd = fopen(argv[1], "r")) == 0) 
	perror(argv[1]);
    if (ifd) {
	while((ch = getc(ifd)) != EOF && (ifd!=stdin || ch != '!')) {
	    int c = 0;
	    switch(ch) {
	    case '>': ch = T_MOV; c=  1; break;
	    case '<': ch = T_MOV; c= -1; break;
	    case '+': ch = T_ADD; c=  1; break;
	    case '-': ch = T_ADD; c= -1; break;
	    case '[': ch = T_WHL; break;
	    case ']': ch = T_END; break;
	    case '.': ch = T_PRT; break;
	    case ',': ch = T_INP; break;
	    default:  ch = T_NOP; break;
	    }
	    if (ch != T_NOP) {
		/* Comment loops, can never be run */
		if (dld || (ch == T_WHL && (!p || p->type == T_END))) {
		    if (ch == T_WHL) dld ++;
		    if (ch == T_END) dld --;
		    continue;
		}
#ifndef NO_RLE
		/* RLE compacting of instructions.
		 * NB: This is not strictly required, but it decreases maximum
		 * memory usage */
		if (p && ch == p->type && p->count ){
		    p->count += c;
		    continue;
		}
#endif
		n = tcalloc(1, sizeof*n);
		if (p) { p->next = n; n->prev = p; } else pgm = n;
		n->type = ch; p = n;
		if (n->type == T_WHL) { n->jmp=j; j = n; }
		else if (n->type == T_END) {
		    if (j) { n->jmp = j; j = j->jmp; n->jmp->jmp = n;
		    } else n->type = T_ERR;
		} else
		    n->count = c;
	    }
	}
	if (ifd!=stdin) fclose(ifd);

	while (j) { n = j; j = j->jmp; n->type = T_ERR; n->jmp = 0; }

	for(n=pgm; n; n=n->next) {
	    switch(n->type)
	    {
		case T_WHL: n->count = ++lid; break;
		case T_ERR: /* Unbalanced bkts */
		    fprintf(stderr, "Warning: skipping unbalanced brackets\n");
		    n->type = T_NOP;
		    break;
	    }
#ifndef NO_RLE
	    /* Merge '<' with '>' and '+' with '-' */
	    if (n->prev && n->prev->type == n->type &&
		    (n->type == T_MOV || n->type == T_ADD)) {
		n->prev->next = n->next;
		if (n->next) n->next->prev = n->prev;
		n->prev->count += n->count;
		p = n->prev;
		free(n);
		n = p;
		if (n->count == 0)
		    n->type = T_NOP;
	    }

	    /* Merge '=' with '+' and '-' */
	    if (n->prev && n->prev->type == T_SET && n->type == T_ADD) {
		n->prev->next = n->next;
		if (n->next) n->next->prev = n->prev;
		n->prev->count += n->count;
		p = n->prev;
		free(n);
		n = p;
	    }

	    /* Remove '+' before '=' */
	    if (n->prev && n->prev->type == T_ADD && n->type == T_SET) {
		n->prev->next = n->next;
		if (n->next) n->next->prev = n->prev;
		n->prev->type = n->type;
		n->prev->count = n->count;
		n->prev->jmp = n->jmp;
		p = n->prev;
		free(n);
		n = p;
	    }

	    /* Identify [-] and replace */
	    if (n->type == T_WHL &&
		n->next->type == T_ADD && n->next->count == -1 &&
		n->next->next->type == T_END)
	    {
		// Make it the type we want.
		n->type = T_SET;
		n->count = 0;
		n->jmp = 0;
		// NOP the others.
		n = n->next;
		n->type = T_NOP;
		n = n->next;
		n->type = T_NOP;
	    }

	    /* Identify [>>>>] "zero search" and replace */
	    if (n->type == T_WHL &&
		n->next->type == T_MOV &&
		n->next->next->type == T_END)
	    {
		// Make it the type we want.
		n->type = T_ZFIND;
		n->count = n->next->count;
		n->jmp = 0;
		// NOP the others.
		n = n->next;
		n->type = T_NOP;
		n = n->next;
		n->type = T_NOP;
	    }

	    /* Identify [->>>>+] "minus one search" and replace */
	    if (n->type == T_WHL &&
		n->next->type == T_ADD && n->next->count == -1 &&
		n->next->next->type == T_MOV &&
		n->next->next->next->type == T_ADD &&
		    n->next->next->next->count == -n->next->count &&
		n->next->next->next->next->type == T_END)
	    {
		// Make it the type we want.
		n->type = T_MFIND;
		n->count = n->next->next->count;
		n->jmp = 0;
		// NOP the others.
		n = n->next;
		n->type = T_NOP;
		n = n->next;
		n->type = T_NOP;
		n = n->next;
		n->type = T_NOP;
		n = n->next;
		n->type = T_NOP;
	    }

	    /* [-<<] [->>]  Run along a rail clearing the flag column*/
	    if (n->type == T_WHL &&
		n->next->type == T_ADD &&
		n->next->count == -1 &&
		n->next->next->type == T_MOV &&
		n->next->next->next->type == T_END)
	    {
		// Make it the type we want.
		n->type = T_ADDWZ;
		n->count = n->next->next->count;
		n->jmp = 0;
		// NOP the others.
		n = n->next;
		n->type = T_NOP;
		n = n->next;
		n->type = T_NOP;
		n = n->next;
		n->type = T_NOP;
	    }

	    /* [->>>+>>>+<<<<<<]   Multiply or move loop. */
	    if (n->type == T_END) {
		struct bfi *v;
		int movsum = 0, incby = 0;
		for(v=n->prev; v->type == T_ADD || v->type == T_SET || v->type == T_MOV; v=v->prev) {
		    if (v->type == T_MOV)
			movsum += v->count;
		    else if (movsum == 0) {
			if (v->type == T_SET) break;
			incby += v->count;
		    }
		}

		if (movsum == 0 && incby == -1 && v->type == T_WHL) {
		    n = v;
		    n->type = T_IFSAV;
		    for(v=n->next; v->type != T_END; v=v->next) {
			if (v->type == T_MOV)
			    movsum += v->count;
			else if (movsum == 0) {
			    v->type = T_NOP;
			} else if (v->type == T_ADD) {
			    v->type = T_MUL;
			} else {
			    v->type = T_SET;
			}
		    }
		    v->type = T_ENDIF;
		}
	    }
#endif
	    /* Remove leftovers */
	    while (n->type == T_NOP && n->prev) {
		n->prev->next = n->next;
		if (n->next) n->next->prev = n->prev;
		p = n->prev;
		free(n);
		n = p;
	    }
	}

#if 0
	for(n=pgm; n; n=n->next)
	    if (n->type != T_END)
		fprintf(stderr, "%s %d \t; %p %p %p\n", tokennames[n->type],
			n->count, n, n->prev, n->jmp);
	    else
		fprintf(stderr, "%s %d \t; %p %p %p\n", tokennames[n->type],
			n->jmp->count, n, n->prev, n->jmp);
	if (0)
#endif
	run();
    }
    return !ifd;
}

#if 0
/*
 * Tree interpreter with a linked list tape.
 * This is very slow!
 */

struct mem { unsigned char val; struct mem *next, *prev; };

#ifdef __GNUC__
__attribute((optimize(3),hot,aligned(64)))
#endif
void run(void) 
{
    struct bfi *n=pgm;
    struct mem *m = tcalloc(1,sizeof*m);
    int ch;

    for(; n; n=n->next)
	switch(n->type)
	{
	    case T_ADD: m->val += n->count; break;
	    case T_SET: m->val = n->count; break;
	    case T_WHL: if (m->val == 0) n=n->jmp; break;
	    case T_END: if (m->val != 0) n=n->jmp; break;
	    case T_PRT: putchar(m->val); break;
	    case T_INP: if((ch=getchar())!=EOF) m->val=ch; break;
	    case T_MOV:
		if (n->count < 0) {
		    for(ch=0; ch<-n->count; ch++)
			if (!(m=m->prev)) {
			    fprintf(stderr, "Error: Hit start of tape\n");
			    exit(1);
			}
		} else {
		    for(ch=0; ch<n->count; ch++) {
			if (m->next == 0) {
			    m->next = tcalloc(1,sizeof*m);
			    m->next->prev = m;
			}
			m=m->next;
		    }
		}
		break;
	}
}
#endif

#if 0
/*
 * Tree interpreter with an int array tape.
 * This is rather slow.
 * Note it is, fractionally, quicker to use int cells and a mask when needed.
 */

#ifndef MEMSIZE
#define MEMSIZE 60000
#endif
#define M(x) ((x) & 0xFF)
#ifdef __GNUC__
__attribute((optimize(3),hot,aligned(64)))
#endif
void run(void) 
{
    struct bfi *n=pgm;
    //unsigned char * m;
    int * m;
    int ch;
    void * freep;
    m = freep = tcalloc(MEMSIZE, sizeof*m);

    for(; n; n=n->next)
	switch(n->type)
	{
	    case T_ADD: *m += n->count; break;
	    case T_SET: *m = n->count; break;
	    case T_WHL: if (M(*m) == 0) n=n->jmp; break;
	    case T_END: if (M(*m) != 0) n=n->jmp; break;
	    case T_PRT: putchar(M(*m)); break;
	    case T_INP: if((ch=getchar())!=EOF) *m=ch; break;
	    case T_MOV: m += n->count; break;
	}

    free(freep);
}
#endif

#if 1
/*
 * An interpreter that uses an arry of ints to store the instructions
 * and another array for the tape. Using ints for the tape and masking 
 * when needed is marginally faster than using a char cell.
 *
 * This seems to be the fastest interpretation method on my Intel I7.
 */

#if MASK == 1
#define icell	unsigned char
#define M(x) x
#elif MASK == 2
#define icell	unsigned short
#define M(x) x
#elif MASK <= 128
#define icell	int
#define M(x) x
#elif MASK == 0xFF || MASK == 0xFFFF
#define icell	int
#define M(x) ((x) & MASK)
#else
#define icell	int
#define M(x) ((x)%(MASK+1))
#endif
#ifndef MEMSIZE
#define MEMSIZE 60000
#endif
void
#ifdef __GNUC__
__attribute((optimize(3),hot,aligned(64)))
#endif
run(void)
{
    struct bfi * n = pgm;
    int arraylen = 0;
    int * progarray = 0;
    int * p;
    int last_offset = 0;
    icell * m;
#ifndef NO_RLE
    icell a = 0;
#endif
#ifndef USEHUGERAM
    void * freep;
#endif

#ifdef USEHUGERAM
    m = map_hugeram();
#else
    m = freep = tcalloc(MEMSIZE+1024, sizeof*p);
    m += 1024;
#endif

    while(n)
    {
	switch(n->type)
	{
	case T_MOV:
	    break;

#ifndef NO_RLE
	case T_ENDIF:
	    arraylen += 2;
	    break;

	case T_SET:
	case T_MUL:
	case T_ZFIND: case T_MFIND:
	case T_ADDWZ:
	case T_IFSAV:
#endif
	case T_INP: case T_PRT:
	case T_ADD:
	case T_WHL: case T_END:
	    arraylen += 3;
	    break;

	default:
	    fprintf(stderr, "Invalid node type found = %d\n", n->type);
	    exit(1);
	}
	n = n->next;
    }

    p = progarray = calloc(arraylen+2, sizeof*progarray);
    if (!progarray) { perror(program); exit(1); }
    n = pgm;

    last_offset = 0;
    while(n)
    {
	if (n->type != T_MOV) {
	    *p++ = (0 - last_offset);
	    last_offset = 0;
	}
	else {
	    last_offset -= n->count;
	    n = n->next;
	    continue;
	}

	*p++ = n->type;
	switch(n->type)
	{
	case T_MOV: 
	    p--;
	    break;

	case T_INP:
	    break;

	case T_PRT:
	    *p++ = -1;
	    break;

	case T_ADD: case T_SET:
	case T_MUL:
	case T_ZFIND: case T_MFIND:
	case T_ADDWZ:
	    *p++ = n->count;
	    break;

	case T_WHL: case T_IFSAV:
	    /* Storing the location of the instruction in the T_END's count
	     * field; it's not normally used */
	    n->jmp->count = p-progarray;
	    *p++ = 0;
	    break;

	case T_END:
	    progarray[n->count] = (p-progarray) - n->count;
	    *p++ = -progarray[n->count];
	    break;

	case T_ENDIF:
	    progarray[n->count] = (p-progarray) - n->count -1;
	    break;
	}
	n = n->next;
    }
    *p++ = 0;
    *p++ = T_STOP;

    p = progarray;

    for(;;) {
	m += *p++;
	switch(*p)
	{
	case T_ADD: *m += p[1]; p += 2; break;
#ifndef NO_RLE
	case T_SET: *m = p[1]; p += 2; break;
#endif

	case T_END: if(M(*m) != 0) p += p[1];
	    p += 2;
	    break;

	case T_WHL:
	    if(M(*m) == 0) p += p[1];
	    p += 2;
	    break;

#ifndef NO_RLE
	case T_ZFIND:
	    /* Search along a rail til you find the end of it. */
	    while(M(*m)) m += p[1];
	    p += 2;
	    break;

	case T_MFIND:
	    (*m)--;
	    while(M(*m) != M((icell)-1)) m += p[1];
	    (*m)++;
	    p += 2;
	    break;

	case T_ADDWZ:
	    while(M(*m)) { *m -= 1; m += p[1]; }
	    p += 2;
	    break;

	case T_MUL: *m += p[1] * a; p += 2; break;

	case T_IFSAV:
	    if(M(*m) == 0) p += p[1];
	    a = *m; *m = 0;
	    p += 2;
	break;

	case T_ENDIF:
	    p += 1;
	    break;
#endif

	case T_INP:
	    {
		int c = getchar();
		if (c != EOF) *m = c;
	    }
	    p += 1;
	    break;

	case T_PRT:
	    {
		int ch = M(p[1] == -1?*m:p[1]);
		putchar(ch);
	    }
	    p += 2;
	    break;

	case T_STOP:
	    goto break_break;
	}
    }
break_break:;
    free(progarray);
#ifndef USEHUGERAM
    free(freep);
#endif
#undef icell
#undef M
}

#endif
