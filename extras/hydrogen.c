/* This is the Hydrogen brainfuck interpreter.
 *
 * It simply takes the original code, runlength encodes it and reencodes
 * some common and expensive sequences into calls of specific C fragments.
 *
 * Robert de Bath (c) 2013 GPL v2 or later.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Choose a cell size: 0, 1, 2, 4, 127, 255, 65535, others.
 */
#if MASK == 1
#define icell	unsigned char
#ifndef ALIGNED
#define dcell	unsigned short
#endif
#define M(x) x
#elif MASK == 2
#define icell	unsigned short
#define M(x) x
#elif MASK < 127
#define icell	int
#define M(x) x
#elif MASK == 0xFF || MASK == 0xFFFF || MASK == 0xFFFFFF
#define icell	int
#define M(x) ((x) & MASK)
#else
#define icell	int
#define M(x) ((x)%(MASK+1))
#endif
#ifndef MEMSIZE
#define MEMSIZE 60000
#endif

#define T fprintf(stderr, "Trace %d\n", __LINE__);

#define TOKEN_LIST(Mac) \
    Mac(NOP) \
    Mac(MOV) Mac(ADD) Mac(PRT) Mac(INP) Mac(WHL) Mac(END) \
    \
    Mac(SAVE) Mac(MUL) Mac(QSET) \
    Mac(SET) Mac(SET4) \
    Mac(ZFIND) Mac(MFIND) Mac(ADDWZ) \
    \
    Mac(SET2) Mac(ADD2) Mac(SUB2) Mac(WHL2) Mac(END2) Mac(ZTEMP2) \
    Mac(SET2c1) Mac(ADD2c1) Mac(SUB2c1) Mac(WHL2c1) Mac(END2c1) \
    Mac(SET2c2) Mac(ADD2c2) Mac(SUB2c2) Mac(WHL2c2) Mac(END2c2) \
    Mac(SAVE2) Mac(MUL2) Mac(QSET2) \
    \
    Mac(STOP) Mac(ERR) Mac(DATA) \
    Mac(RMOV) Mac(DEC)


#define GEN_TOK_ENUM(NAME) T_ ## NAME,
enum token { TOKEN_LIST(GEN_TOK_ENUM) };

#define GEN_TOK_STRING(NAME) "T_" #NAME,
const char* tokennames[] = { TOKEN_LIST(GEN_TOK_STRING) };

int nexttok(FILE * ifd);

struct subst {
    int t[7];
    char * s;
} substrlist[] = {
#ifndef NO_XTRA
    { {T_SET,0},
		"[+]" },
    { {T_SET4, T_MOV, T_MOV, T_MOV, 0},
		"[-]>[-]>[-]>[-]" },

#if MASK == 1
    { {T_MOV, T_ZTEMP2, T_MOV, T_MOV, 0},
		"[-]>>>[-]" },
    { {T_SET2, T_MOV, 0},
		"[-]>[-]" },
    { {T_SET2c1, T_SET2,0},

		"[>>+>>>+<<<<<-]>>>>>[<<<<<+>>>>>-]<<<"
		    "[[-]<<<+>>>]<"
		    "[>+>>>+<<<<-]>>>>[<<<<+>>>>-]<<<"
		    "[[-]<<<+>>>]<<<"
		    "[[-]>"
		"[<+>>>+<<-]<[>+<-]+>>>[<<<->>>[-]]<<<[-"
		    ">>-<<"
		    "]>-"
		"[>>+>>>+<<<<<-]>>>>>[<<<<<+>>>>>-]<<<"
		    "[[-]<<<+>>>]<"
		    "[>+>>>+<<<<-]>>>>[<<<<+>>>>-]<<<"
		    "[[-]<<<+>>>]<<<"
		    "]>" },

    { {T_ADD2c1,T_ADD2,0},
		"+" "[<+>>>+<<-]<[>+<-]+>>>[<<<->>>[-]]<<<[-"
		    ">>+<<"
		    "]>" },
    { {T_SUB2c1,T_SUB2,0},
		"[<+>>>+<<-]<[>+<-]+>>>[<<<->>>[-]]<<<[-"
		    ">>-<<"
		    "]>-"},
    { {T_WHL2c1,T_WHL2,0},
		"[>>+>>>+<<<<<-]>>>>>[<<<<<+>>>>>-]<<<"
		    "[[-]<<<+>>>]<"
		    "[>+>>>+<<<<-]>>>>[<<<<+>>>>-]<<<"
		    "[[-]<<<+>>>]<<<"
		    "[[-]>" },
    { {T_END2c1,T_END2,0},
		"[>>+>>>+<<<<<-]>>>>>[<<<<<+>>>>>-]<<<"
		    "[[-]<<<+>>>]<"
		    "[>+>>>+<<<<-]>>>>[<<<<+>>>>-]<<<"
		    "[[-]<<<+>>>]<<<"
		    "]>" },
#endif
#if MASK == 1
    { {T_SET2c2, T_SET2,0},
        "[<+>[->>+<<]]>>[-<<+>>]<[<<+>>[->+<]]>[-<+>]<<<[[-]>"
	"<+>[<->[->>+<<]]>>[-<<+>>]<<<[->>-<<]>-"
	"[<+>[->>+<<]]>>[-<<+>>]<[<<+>>[->+<]]>[-<+>]<<<]>"
			},
    { {T_MOV, T_ADD2c2, T_ADD2, T_RMOV, 0},
	"+>+[<->[->>+<<]]>>[-<<+>>]<<<[->>+<<]"
			},
    { {T_MOV, T_SUB2c2, T_SUB2, 0},
	"+>[<->[->>+<<]]>>[-<<+>>]<<<[->>-<<]>-"
			},
    { {T_WHL2c2, T_WHL2, T_RMOV, 0},
	"[<+>[->>+<<]]>>[-<<+>>]<[<<+>>[->+<]]>[-<+>]<<<[[-]"
			},
    { {T_END2c2, T_END2, T_RMOV, 0},
	"[<+>[->>+<<]]>>[-<<+>>]<[<<+>>[->+<]]>[-<+>]<<<]"
			},
#endif

#endif

    { {T_MOV,0},    ">" },
    { {T_RMOV,0},   "<" },
    { {T_ADD,0},    "+" },
    { {T_DEC,0},    "-" },
    { {T_WHL,0},    "[" },
    { {T_END,0},    "]" },
    { {T_PRT,0},    "." },
    { {T_INP,0},    "," },
    { {T_DATA,0},   "!" },

    {{0},0}
};

char * program = "C";
struct bfi { int type; struct bfi *next, *prev, *jmp; int count; } *pgm;

void run(void);
void * tcalloc(size_t nmemb, size_t size);

int dump_prog = 0;

int main(int argc, char **argv)
{
    FILE * ifd;
    int ch, dld = 0, lid = 0;
    struct bfi *p=0, *n=0, *j=0;
    setbuf(stdout, NULL);
    for(;;) {
	if (argc>1 && strcmp(argv[1], "-d") == 0) {
	    dump_prog++; argc--; argv++;
	} else break;
    }
    if (argc < 2 || strcmp(argv[1], "-") == 0) ifd = stdin;
    else if ((ifd = fopen(argv[1], "r")) == 0)
	perror(argv[1]);
    if (ifd) {
	while((ch = nexttok(ifd)) != EOF &&
		(ifd!=stdin || ch != T_DATA || dld || j)) {
	    int c = 0;
	    switch(ch) {
	    case T_MOV:  ch = T_MOV; c=  1; break;
	    case T_RMOV: ch = T_MOV; c= -1; break;
	    case T_ADD:  ch = T_ADD; c=  1; break;
	    case T_DEC:  ch = T_ADD; c= -1; break;
	    case T_ADD2: ch = T_ADD2; c=  1; break;
	    case T_SUB2: ch = T_ADD2; c= -1; break;
	    case T_DATA: ch = T_NOP; break;
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
		if (n->type == T_WHL || n->type == T_WHL2) { n->jmp=j; j = n; }
		else if (n->type == T_END || n->type == T_END2) {
		    if (j) { n->jmp = j; j = j->jmp; n->jmp->jmp = n;
		    } else n->type = T_ERR;
		} else
		    n->count = c;
	    }
	}
	if (ifd!=stdin) fclose(ifd);

	while (j) { n = j; j = j->jmp; n->type = T_ERR; n->jmp = 0; }

#if 0
	for(n=pgm; n; n=n->next)
	    if (n->type != T_END)
		fprintf(stderr, "%s %d \t; %p %p %p\n", tokennames[n->type],
			n->count, n, n->prev, n->jmp);
	    else
		fprintf(stderr, "%s %d \t; %p %p %p\n", tokennames[n->type],
			n->jmp->count, n, n->prev, n->jmp);
#endif

#ifdef NO_XTRA
	for(n=pgm; n; n=n->next) {
	    switch(n->type)
	    {
		case T_WHL: n->count = ++lid; break;
		case T_WHL2: n->count = ++lid; break;
		case T_ERR: /* Unbalanced bkts */
		    fprintf(stderr, "Warning: skipping unbalanced brackets\n");
		    n->type = T_NOP;
		    break;
	    }
	    while (n->type == T_NOP && n->prev) {
		n->prev->next = n->next;
		if (n->next) n->next->prev = n->prev;
		p = n->prev;
		free(n);
		n = p;
	    }
	}
#else
	for(n=pgm; n; n=n->next) {
	    switch(n->type)
	    {
		case T_WHL: n->count = ++lid; break;
		case T_WHL2: n->count = ++lid; break;
		case T_ERR: /* Unbalanced bkts */
		    fprintf(stderr, "Warning: skipping unbalanced brackets\n");
		    n->type = T_NOP;
		    break;
	    }

	    /* Merge '<' with '>' and '+' with '-' */
	    if (n->prev && n->prev->type == n->type &&
		(n->type == T_MOV || n->type == T_ADD || n->type == T_ADD2)) {
		n->prev->next = n->next;
		if (n->next) n->next->prev = n->prev;
		n->prev->count += n->count;
		p = n->prev;
		free(n);
		n = p;
	    }
	    if (n->type == T_MOV || n->type == T_ADD || n->type == T_ADD2) {
		if (n->count == 0)
		    n->type = T_NOP;
	    }

	    /* Merge '[-]' with '+' and '-' */
	    if (n->prev && n->prev->type == T_SET && n->type == T_ADD) {
		n->prev->next = n->next;
		if (n->next) n->next->prev = n->prev;
		n->prev->count += n->count;
		p = n->prev;
		free(n);
		n = p;
	    }

	    /* Remove '+' before '[-]' */
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

#if MASK == 1
	    /* Remove unnecessary T_*c1 and T_*c2 tokens */
	    if (n->prev && (n->type == T_ADD2c1 || n->type == T_SUB2c1 ||
	                    n->type == T_ADD2c2 || n->type == T_SUB2c2 ||
	                    n->type == T_WHL2c2 || n->type == T_END2c2 ||
	                    n->type == T_SET2c2 || n->type == T_ZTEMP2)) {
		p = n->prev;
		if (p->type == T_ADD2 ||
		    p->type == T_SET2 ||
		    p->type == T_WHL2 ||
		    p->type == T_END2 ||
		    p->type == T_ZTEMP2)
		    n->type = T_NOP;
		if (p->type == T_PRT) {
		    p = p->prev;
		    if (p->type == T_ADD2 ||
			p->type == T_SET2 ||
			p->type == T_WHL2 ||
			p->type == T_END2 ||
			p->type == T_ZTEMP2)
			n->type = T_NOP;
		}
	    }

	    /* Identify double byte [-] and replace */
	    if (n->type == T_END2 &&
		n->prev->type == T_ADD2 && n->prev->count == -1 &&
		n->prev->prev->type == T_WHL2)
	    {
		// NOP the nodes.
		p = n;
		n->type = T_NOP;
		n = n->prev;
		n->type = T_NOP;
		n = n->prev;
		n->type = T_NOP;

		p->type = T_SET2;
		p->count = 0;
		p->jmp = 0;
		if (n->prev && n->prev->type == T_WHL2c1)
		    n->prev->type = T_SET2c1;
		if (n->prev && n->prev->type == T_WHL2c2)
		    n->prev->type = T_SET2c2;
	    }

	    /* Merge double byte '[-]' with '+' and '-' */
	    if (n->prev && n->prev->type == T_SET2 && n->type == T_ADD2) {
		n->prev->next = n->next;
		if (n->next) n->next->prev = n->prev;
		n->prev->count += n->count;
		p = n->prev;
		free(n);
		n = p;
	    }
#endif

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

	    /* [->>>+>>>+<<<<<<]   Multiple move or add loop. */
	    if (n->type == T_END) {
		struct bfi *v;
		int movsum = 0, incby = 0, idx_only = 1;
		for(v=n->prev; v->type == T_ADD || v->type == T_SET || v->type == T_MOV; v=v->prev) {
		    if (v->type == T_MOV)
			movsum += v->count;
		    else if (movsum == 0) {
			if (v->type == T_SET) break;
			incby += v->count;
		    } else
			idx_only = 0;
		}

		if (movsum == 0 && incby == -1 && v->type == T_WHL) {
		    n = v;
		    n->type = idx_only?T_NOP:T_SAVE;
		    n->count = 0;
		    n->jmp = 0;
		    for(v=n->next; v->type != T_END; v=v->next) {
			if (v->type == T_MOV) {
			    movsum += v->count;
			} else if (movsum == 0) {
			    v->type = idx_only?T_SET:T_NOP;
			    v->count = 0;
			} else if (v->type == T_ADD) {
			    v->type = T_MUL;
			} else {
			    v->type = T_QSET;
			}
		    }
		    v->type = T_NOP;
		}
	    }

#if MASK == 1
	    /* [->>>+>>>+<<<<<<]   Multiple move or add loop (double byte). */
	    if (n->type == T_END2) {
		struct bfi *v;
		int movsum = 0, incby = 0, idx_only = 1;
		for(v=n->prev;
			v->type == T_MOV
			|| v->type == T_ADD2
			|| v->type == T_SET2
			|| v->type == T_ADD2c2
//			|| v->type == T_SET2c2
			|| v->type == T_SUB2c2
			|| v->type == T_END2c2
			|| v->type == T_ZTEMP2 ; v=v->prev) {

		    if (v->type == T_MOV)
			movsum += v->count;
		    else if (movsum == 0) {
			if (v->type == T_SET2) break;
			if (v->type == T_ADD2 )
			    incby += v->count;
		    } else
			idx_only = 0; /*FIXME*/

		    if (movsum % 3 != 0)
			break; /* WTF! */
		}

		if (movsum == 0 && incby == -1 && v->type == T_WHL2 &&
		    (v->prev == 0 || v->prev->type != T_WHL2c2)) {
		    n = v;
		    n->type = idx_only?T_NOP:T_SAVE2;
		    n->count = 0;
		    n->jmp = 0;
		    for(v=n->next; v->type != T_END2; v=v->next) {
			if (v->type == T_MOV) {
			    movsum += v->count;
			} else if (movsum == 0) {
			    v->type = idx_only?T_SET2:T_NOP;
			    v->count = 0;
			} else if (v->type == T_ADD2) {
			    v->type = T_MUL2;
			} else if (v->type == T_SET2) {
			    v->type = T_QSET2;
			} else if (v->type == T_END2c2) {
			    v->type = T_NOP;
			}
		    }
		    v->type = T_NOP;
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
	while (pgm && pgm->type == T_NOP) {
	    n = pgm;
	    pgm = pgm->next;
	    if (pgm) pgm->prev = 0;
	    free(n);
	}
#endif

	if (dump_prog) {
	    for(n=pgm; n; n=n->next)
		if (n->type != T_END)
		    fprintf(stderr, "%s %d \t; %p %p %p\n", tokennames[n->type],
			    n->count, n, n->prev, n->jmp);
		else
		    fprintf(stderr, "%s %d \t; %p %p %p\n", tokennames[n->type],
			    n->jmp->count, n, n->prev, n->jmp);
	} else
	    run();
    }
    return !ifd;
}

char inputbuffer[BUFSIZ];
int inputline[BUFSIZ];
int inputcol[BUFSIZ];
int buflow = 0, bufhigh = 0, bufcount = 0;
int bufdrain = 0;
int * symbuffer = 0;

int inpline = 1, inpcol = 0;

int
nexttok(FILE * ifd)
{
    int cc;
    struct subst * subs;

    if (symbuffer && *symbuffer == 0) symbuffer = 0;
    if (symbuffer) return *symbuffer++;

    while (bufcount < sizeof(inputbuffer)/2 && !bufdrain) {
	if (buflow != 0) {
	    memmove(inputbuffer, inputbuffer+buflow, bufcount);
	    buflow = 0; bufhigh = buflow + bufcount;
	}
	cc = 0;
	while(bufhigh < sizeof(inputbuffer)-1 && !bufdrain) {
	    int ch = getc(ifd);
	    if (ch == '\n') {inpline++; inpcol=0;} else inpcol++;
	    if ((ch == '!' && ifd==stdin) || ch == EOF) {
		bufdrain = 1;
		if (ch == EOF)
		    break;
	    } else
		if (!strchr("+-<>[],.", ch))
		    continue;

	    bufcount++; cc++;
	    inputbuffer[bufhigh] = ch;
	    inputline[bufhigh] = inpline;
	    inputcol[bufhigh] = inpcol;
	    inputbuffer[++bufhigh] = 0;
	}
    }
    if (bufcount <= 0) { bufdrain=0; return EOF; }

    for(subs=substrlist; subs->s; subs++) {
	int l = strlen(subs->s);
	if (strncmp(inputbuffer+buflow, subs->s, l) == 0) {
	    if (inputbuffer[buflow] == '!')
		bufdrain = 0;
	    buflow += l;
	    bufcount -= l;
	    symbuffer = subs->t;
	    return *symbuffer++;
	}
    }

    fprintf(stderr, "String substitution %d failed\n", inputbuffer[buflow]);
    exit(1);
}

void *
tcalloc(size_t nmemb, size_t size)
{
    void * m;
    m = calloc(nmemb, size);
    if (m) return m;

    fprintf(stderr, "Allocate of %d*%d bytes failed, ABORT\n", nmemb, size);
    exit(42);
}






#if 0
/*
 * Tree interpreter with a linked list tape.
 * This is very slow!
 */

#ifndef NO_XTRA
#error "This interpreter does not support extended codes"
#endif

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

#ifndef NO_XTRA
#error "This interpreter does not support extended codes"
#endif

#ifdef __GNUC__
__attribute((optimize(3),hot,aligned(64)))
#endif
void run(void)
{
    struct bfi *n=pgm;
    //unsigned char * m;
    icell * m;
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
#ifndef NO_XTRA
    icell a = 0;
#if MASK == 1
    int a2 = 0;
#endif
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

	case T_INP: case T_PRT:
	case T_ADD:
	case T_WHL: case T_END:
	    arraylen += 3;
	    break;

#ifndef NO_XTRA

	case T_SAVE:
	case T_SET4:
	    arraylen += 2;
	    break;

	case T_SET: case T_QSET:
	case T_MUL:
	case T_ZFIND: case T_MFIND:
	case T_ADDWZ:
	    arraylen += 3;
	    break;

#if MASK == 1
	case T_ADD2: case T_SET2: case T_MUL2: case T_QSET2:
	    arraylen += 3;
	    break;

	case T_WHL2: case T_END2:
	case T_ADD2c1: case T_SUB2c1:
	case T_ADD2c2: case T_SUB2c2:
	    arraylen += 3;
	    break;

	case T_SAVE2:
	case T_ZTEMP2:
	    arraylen += 2;
	    break;

	case T_WHL2c1: case T_END2c1: case T_SET2c1:
	case T_WHL2c2: case T_END2c2: case T_SET2c2:
	    break;
#endif
#endif

	default:
	    fprintf(stderr, "Invalid node type found = %s\n",
		    tokennames[n->type]);
	    exit(1);
	}
	n = n->next;
    }
    arraylen+=2;

    p = progarray = calloc(arraylen, sizeof*progarray);
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

	case T_INP: case T_SAVE: case T_SET4:
	case T_ZTEMP2:
	case T_SAVE2:
	    break;

	case T_PRT:
	    *p++ = -1;
	    break;

	case T_ADD:
	    *p++ = n->count;
	    while (n->next && n->next->type == n->type) {
		n=n->next;
		p[-1] += n->count;
	    }
	    break;

	case T_ADD2: case T_SUB2: case T_SET2: case T_MUL2: case T_QSET2:
	case T_SET:
	case T_MUL: case T_QSET:
	case T_ZFIND: case T_MFIND:
	case T_ADDWZ:
	    *p++ = n->count;
	    break;

	case T_WHL2:
	case T_WHL:
	    /* Storing the location of the instruction in the T_END's count
	     * field; it's not normally used */
	    n->jmp->count = p-progarray;
	    *p++ = 0;
	    break;

	case T_END:
	case T_END2:
	    progarray[n->count] = (p-progarray) - n->count;
	    *p++ = -progarray[n->count];
	    break;

	case T_ADD2c1: case T_SUB2c1:
	case T_ADD2c2: case T_SUB2c2:
	    if (n->next->type != T_ADD2) {
		*p++ = 0;
	    } else {
		n=n->next;
		*p++ = n->count;
	    }
	    break;

	case T_SET2c1:
	case T_SET2c2:
	    if (n->next->type != T_SET2) {
		fprintf(stderr, "%s lost it's %s\n",
		    tokennames[n->type], tokennames[T_SET2]);
		exit(1);
	    }
	    n=n->next;
	    *p++ = n->count;
	    break;

	case T_WHL2c1:
	case T_WHL2c2:
	    if (n->next->type != T_WHL2) {
		fprintf(stderr, "%s lost it's %s\n",
		    tokennames[n->type], tokennames[T_WHL2]);
		exit(1);
	    }
	    n=n->next;
	    n->jmp->count = p-progarray;
	    *p++ = 0;
	    break;

	case T_END2c1:
	case T_END2c2:
	    if (n->next->type != T_END2) {
		fprintf(stderr, "%s lost it's %s\n",
		    tokennames[n->type], tokennames[T_END2]);
		exit(1);
	    }
	    n=n->next;
	    progarray[n->count] = (p-progarray) - n->count;
	    *p++ = -progarray[n->count];
	    break;

	default:
	    fprintf(stderr, "Invalid node type found in GEN2 = %s\n",
		    tokennames[n->type]);
	    exit(1);
	}
	n = n->next;
    }
    *p++ = 0;
    *p++ = T_STOP;

    if (p-progarray > arraylen) {
	fprintf(stderr, "Program array overflow; aborting\n");
	exit(1);
    }

    p = progarray;

    for(;;) {
	m += *p++;
#if 0
fprintf(stderr, "%d: %s,%d m[%d]=%d"
	    ", m[%d]=%d"
	    ", m[%d]=%d"
	    ", m[%d]=%d"
	    ", m[%d]=%d"
	    ", COND=%d"
	    "\n",
	    p-progarray, tokennames[*p], p[1],
	    m-(icell*)freep-1025, m[-1],
	    m-(icell*)freep-1024, *m,
	    m-(icell*)freep-1023, m[1],
	    m-(icell*)freep-1022, m[2],
	    m-(icell*)freep-1019, m[5],

	    m[-1] + !m[1] + !(m[2] + m[0])
	    );
#endif
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

#ifndef NO_XTRA
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

	case T_SAVE: a = *m; *m = 0; p += 1; break;
	case T_MUL: *m += p[1] * a; p += 2; break;
	case T_QSET: if(a) *m = p[1]; p += 2; break;

	case T_SET4: *m = 0; m[1] = 0; m[2] = 0; m[3] = 0; p += 1; break;

#if MASK == 1
	case T_SAVE2:
	    a = *m; a2 = (m[1]<<8) + a;
	    *m = 0; m[1] = 0;
	    p += 1;
	    break;

	case T_MUL2:
#ifdef dcell
	    *((dcell*)m) += p[1] * a2;
#else
	    {
		unsigned int t = m[0] + (m[1]<<8);
		t += p[1] * a2;
		m[0] = t; m[1] = (t>>8);
	    }
#endif
	    p += 2;
	    break;

	case T_QSET2:
	    if (a2)
#ifdef dcell
		*((dcell*)m) = p[1];
#else
	    {
		unsigned int t;
		t = p[1];
		m[0] = t; m[1] = (t>>8);
	    }
#endif
	    p += 2;
	    break;

	case T_ADD2:
#ifdef dcell
	    *((dcell*)m) += p[1];
#else
	    {
		unsigned int t = m[0] + (m[1]<<8);
		t += p[1];
		m[0] = t; m[1] = (t>>8);
	    }
#endif
	    p += 2;
	    break;

	case T_SET2:
#ifdef dcell
	    *((dcell*)m) = p[1];
#else
	    {
		unsigned int t;
		t = p[1];
		m[0] = t; m[1] = (t>>8);
	    }
#endif
	    p += 2;
	    break;

	case T_WHL2:
	    if (m[0] == 0 && m[1] == 0) p += p[1];
	    p += 2;
	    break;

	case T_END2:
	    if (m[0] != 0 || m[1] != 0) p += p[1];
	    p += 2;
	    break;

	case T_ZTEMP2:
	    m[-1] = m[2] = 0;
	    p += 1;
	    break;


	case T_ADD2c1:
	    if (m[-1] != 0 || m[2] != 0) {
		++*m;
		m[-1] += m[0];
		m[2] += m[0];
		m[0] = m[-1];
		m[1] += !m[2];
		m[1] -= !m[0];
		--*m;
		m[2] = 0;
		m[-1] = 0;
	    }
#ifdef dcell
	    *((dcell*)m) += p[1];
#else
	    {
		unsigned int t = m[0] + (m[1]<<8);
		t += p[1];
		m[0] = t; m[1] = (t>>8);
	    }
#endif
	    p += 2;
	    break;

	case T_SUB2c1:
	    if (m[-1] != 0 || m[2] != 0) {
		m[-1] += m[0];
		m[2] += m[0];
		m[0] = m[-1];
		m[-1] = 1;
		if(m[2]) {
		    m[-1] = 0;
		}
		m[1] -= m[-1];
		/* *m += 0; */
		m[2] = m[0];
		m[-1] = 1;
		if(m[2]) {
		    m[-1] = 0;
		}
		m[2] = 0;
		m[1] += m[-1];
		m[-1] = 0;
	    }
#ifdef dcell
	    *((dcell*)m) += p[1];
#else
	    {
		unsigned int t = m[0] + (m[1]<<8);
		t += p[1];
		m[0] = t; m[1] = (t>>8);
	    }
#endif
	    p += 2;
	    break;

	case T_SET2c1:
	    if (m[-1] != 0 || m[2] != 0 || m[5] != 0) {
		m[-1] += !!m[1] + !!(m[2] + m[0]);
		m[0] += m[5];
		m[2] = 0;
		m[5] = 0;
		if(m[-1]) { m[-1] = 0; m[0] = 0; m[1] = 0; }
	    } else
		m[0] = m[1] = 0;
#ifdef dcell
	    *((dcell*)m) += p[1];
#else
	    {
		unsigned int t = m[0] + (m[1]<<8);
		t += p[1];
		m[0] = t; m[1] = (t>>8);
	    }
#endif
	    p += 2;
	    break;

	case T_WHL2c1:
	    if (m[-1] != 0 || m[2] != 0 || m[5] != 0) {
		icell t;
		t = m[-1] + !!m[1] + !!(m[2] + m[0]);
		m[0] += m[5];
		m[5] = 0;
		m[2] = 0;
		m[-1] = 0;
		if(M(t) == 0) p += p[1];
		p += 2;
	    } else {
		if ((!!m[1] | !!m[0]) == 0) p += p[1];
		p += 2;
	    }
	    break;

	case T_END2c1:
	    if (m[-1] != 0 || m[2] != 0 || m[5] != 0) {
		icell t;
		t = m[-1] + !!m[1] + !!(m[2] + m[0]);
		m[0] += m[5];
		m[5] = 0;
		m[2] = 0;
		m[-1] = 0;

		if(M(t) != 0) p += p[1];
		p += 2;
	    } else {
		if(M(*m) != 0 || M(m[1]) != 0) p += p[1];
		p += 2;
	    }
	    break;


	case T_ADD2c2:
	    if (m[-1] != 0 || m[2] != 0) {
		++m[-1];
		++*m;
		if(m[0]) {
		    --m[-1];
		    m[2] += m[0];
		}
		m[0] = m[2];
		m[2] = 0;
		m[1] += m[-1];
		m[-1] = 1;
		if(m[0]) {
		    m[-1] = 0;
		}
		m[1] -= m[-1];
		m[-1] = 0;
		--*m;
	    }
#ifdef dcell
	    *((dcell*)m) += p[1];
#else
	    {
		unsigned int t = m[0] + (m[1]<<8);
		t += p[1];
		m[0] = t; m[1] = (t>>8);
	    }
#endif
	    p += 2;
	    break;


	case T_SUB2c2:
	    if (m[-1] != 0 || m[2] != 0) {
		++m[-1];
		if(m[0]) {
		    --m[-1];
		    m[2] += m[0];
		}
		m[0] = m[2];
		m[2] = 0;
		m[1] -= m[-1];
		m[-1] = 1;
		if(m[0]) {
		    m[-1] = 0;
		}
		m[1] += m[-1];
		m[-1] = 0;
	    }
#ifdef dcell
	    *((dcell*)m) += p[1];
#else
	    {
		unsigned int t = m[0] + (m[1]<<8);
		t += p[1];
		m[0] = t; m[1] = (t>>8);
	    }
#endif
	    p += 2;
	    break;

	case T_SET2c2:
	    if (m[-1] != 0 || m[2] != 0) {
		if(m[0]) {
		    ++m[-1];
		    m[2] += m[0];
		}
		m[0] = m[2];
		m[2] = 0;
		if(m[1]) {
		    ++m[-1];
		}
		if(m[-1]) {
		    m[-1] = 0;
		    m[0] = 0;
		    m[1] = 0;
		}
	    } else
		m[0] = m[1] = 0;
#ifdef dcell
	    *((dcell*)m) += p[1];
#else
	    {
		unsigned int t = m[0] + (m[1]<<8);
		t += p[1];
		m[0] = t; m[1] = (t>>8);
	    }
#endif
	    p += 2;
	    break;

	case T_WHL2c2:
	    if (m[-1] != 0 || m[2] != 0) {
		icell t = m[-1];
		if(m[0]) {
		    ++t;
		    m[2] += m[0];
		}
		m[0] = m[2];
		if(m[1]) {
		    ++t;
		}
		m[-1] = m[2] = 0;

		if(M(t) == 0) p += p[1];
	    } else {
		if ((!!m[1] | !!m[0]) == 0) p += p[1];
	    }
	    p += 2;
	    break;

	case T_END2c2:
	    if (m[-1] != 0 || m[2] != 0) {
		icell t = m[-1];
		if(m[0]) {
		    ++t;
		    m[2] += m[0];
		}
		m[0] = m[2];
		if(m[1]) {
		    ++t;
		}
		m[-1] = m[2] = 0;

		if(M(t) != 0) p += p[1];
	    } else {
		if(M(*m) != 0 || M(m[1]) != 0) p += p[1];
	    }
	    p += 2;
	    break;

#endif
#endif
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
