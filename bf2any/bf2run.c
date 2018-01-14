#ifndef PART2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Integer translation from BF, runs at about 1,000,000,000 instructions per second.
 *
 */

static int do_dump = 0;

#ifndef icell
typedef int icell;
#endif

static int *mem = 0, *mptr = 0;
static size_t memlen = 0;
static icell * tape = 0;

static size_t tapealloc;

#define TOKEN_LIST(Mac) \
    Mac(STOP) Mac(ADD) Mac(PRT) Mac(INP) Mac(WHL) Mac(END) \
    Mac(SET) Mac(BEG) Mac(MUL) Mac(MUL1) Mac(QSET) Mac(QMUL) Mac(QMUL1) \
    Mac(ZFIND) Mac(RAILC) Mac(RAILZ) Mac(DUMP)

#define GEN_TOK_ENUM(NAME) T_ ## NAME,
enum token { TOKEN_LIST(GEN_TOK_ENUM) TCOUNT};
#define GEN_TOK_STRING(NAME) #NAME,
static const char* tokennames[] = { TOKEN_LIST(GEN_TOK_STRING) };

static struct stkdat { struct stkdat * up; int id; } *sp = 0;
static int imov = 0;
static int prevtk = 0;
static int checklimits = 0;

static void runprog(int * p, int *ep);
static void debugprog(int * p, int *ep);
static void dumpprog(int * p, int *ep);
static void dumpmem(int *tp);

static check_arg_t fn_check_arg;
struct be_interface_s be_interface = { .check_arg=fn_check_arg, .cells_are_ints=1 };

static int
fn_check_arg(const char * arg)
{
    if (strcmp(arg, "-#") == 0) return 1;
    if (strcmp(arg, "-D") == 0) {
	checklimits = 1;
	return 1;
    } else
    if (strcmp(arg, "-d") == 0) {
	do_dump = 1;
	return 1;
    } else
    if (strcmp(arg, "-r") == 0) {
	do_dump = 0;
	return 1;
    } else
    if (strcmp("-h", arg) ==0) {
	fprintf(stderr, "%s\n",
	"\t"    "-d      Dump code"
	"\n\t"  "-r      Run program");
	return 1;
    } else
    return 0;
}

void
outcmd(int ch, int count)
{
    int t = -1;

    if (memlen < mptr-mem + 8) {
	size_t s = memlen + 1024;
	int * p;
	p = realloc(mem, s * sizeof(int));

	memset(p+memlen, '\0', (s-memlen) * sizeof(int));
	mptr = (mptr-mem + p);
	mem = p;
	memlen = s;
    }

    if (ch != '>' && ch != '<') {
	*mptr++ = imov;
	imov = 0;
    }

    switch(ch) {
    default: mptr--; imov = *mptr; break; /* Hmm; oops */

    case '=': *mptr++ = t = T_SET; *mptr++ = count; break;
    case 'B': *mptr++ = t = T_BEG; break;
    case 'M': *mptr++ = t = T_MUL; *mptr++ = count; break;
    case 'N': *mptr++ = t = T_MUL; *mptr++ = -count; break;
    case 'S': *mptr++ = t = T_MUL1; break;

    case 'Q': *mptr++ = t = T_QSET; *mptr++ = count; break;
    case 'm': *mptr++ = t = T_QMUL; *mptr++ = count; break;
    case 'n': *mptr++ = t = T_QMUL; *mptr++ = -count; break;
    case 's': *mptr++ = t = T_QMUL1; break;

    case '+': *mptr++ = t = T_ADD; *mptr++ = count; break;
    case '-': *mptr++ = t = T_ADD; *mptr++ = -count; break;
    case '<': imov -= count; break;
    case '>': imov += count; break;
    case 'X': *mptr++ = t = T_STOP; break;
    case ',': *mptr++ = t = T_INP; break;
    case '.': *mptr++ = t = T_PRT; break;
    case '#': *mptr++ = t = T_DUMP; break;
    case '~':
	*mptr++ = t = T_STOP;
	tapealloc = tapelen + BOFF;
	if (do_dump) {
	    dumpprog(mem, mptr);
	    return;
	}
	setbuf(stdout, 0);

	tape = calloc(tapealloc, sizeof(icell));

	if(!enable_debug && !checklimits)
	    runprog(mem, tape+BOFF);
	else
	    debugprog(mem, tape+BOFF);
	break;

    case '[':
	{
	    struct stkdat * n = malloc(sizeof*n);
	    n->up = sp;
	    sp = n;
	    *mptr++ = t = T_WHL;
	    n->id = mptr-mem;
	    *mptr++ = 0;	/* Default to NOP */
	}
	break;

    case ']':
	*mptr++ = t = T_END;
	if (sp) {
	    struct stkdat * n = sp;
	    sp = n->up;
	    mem[n->id] = (mptr-mem) - n->id;
	    *mptr++ = -mem[n->id];
	    free(n);
	} else
	    *mptr++ = 0;	/* On error NOP */

	if ((prevtk & 0xFF) == T_WHL && mptr[-3] != 0) {
	    /* [<<<] loop */
	    mptr[-5] = T_ZFIND;
	    mptr[-4] = mptr[-3];
	    mptr -= 3;
	} else
	if ((prevtk & 0xFFFF) == ((T_WHL<<8) + T_ADD) && mptr[-4] == -1
	    && mptr[-6] == 0 && mptr[-3] != 0) {
	    /* [-<<<] loop */
	    mptr[-8] = T_RAILC;
	    mptr[-7] = mptr[-3];
	    mptr -= 6;
	} else
	if ((prevtk & 0xFFFF) == ((T_WHL<<8) + T_SET) && mptr[-4] == 0
	    && mptr[-6] == 0 && mptr[-3] != 0) {
	    /* [[-]<<<] loop */
	    mptr[-8] = T_RAILZ;
	    mptr[-7] = mptr[-3];
	    mptr -= 6;
	}
	/* TODO: Add special for [-<<<+] */
	break;
    }

    if (t>=0 && enable_optim) {
	prevtk = (prevtk << 8) + t;
    }
}

static void
dumpmem(int *mp)
{
    size_t i, j = 0;
    const icell msk = (bytecell)?0xFF:-1;
    for (i = 0; i < tapealloc; i++) if (tape[i]&msk) j = i + 1;
    fprintf(stderr, "Ptr: %3d, mem:", (int)(mp-tape-BOFF));
    for (i = BOFF; i < j; i++)
	fprintf(stderr, "%s%d", tape + i == mp ? ">" : " ", tape[i]&msk);
    fprintf(stderr, "\n");
}

static void
dumpprog(int * p, int * ep)
{
    for(;p<ep;){
	printf("%06d:", (int)(p-mem));
	if (*p)
	    printf("MOV %d\n%06d:", *p, (int)(p-mem + 1));
	p++;
	printf("%s", tokennames[*p]);
	switch(*p++) {
	case T_STOP:
	case T_PRT: case T_INP:
	case T_BEG: case T_MUL1: case T_QMUL1:
	case T_DUMP:
	    break;
	case T_WHL: case T_END:
	    printf(" %d (%06d)", *p, (int)(p-mem + *p+1));
	    p++;
	    break;
	case T_SET: case T_ADD:
	case T_MUL: case T_QSET: case T_QMUL:
	case T_ZFIND: case T_RAILC: case T_RAILZ:
	    printf(" %d", *p++);
	    break;
	default:
	    printf(" DECODE ERROR\n");
	    return;
	}
	printf("\n");
    }
}
#endif

#ifdef PART2
static void
debugprog(register int * p, register icell *mp)
#else
#if defined(__GNUC__) && ((__GNUC__>4) || (__GNUC__==4 && __GNUC_MINOR__>=4))
/* Tell GNU C to think really hard about this function! */
__attribute__((optimize(3),hot,aligned(64)))
#endif
static void
runprog(register int * p, register icell *mp)
#endif
{
    register icell a = 1;
    const icell msk = (bytecell)?0xFF:-1;
    for(;;){
	mp += p[0];
#ifdef PART2
	if (mp>=tape+tapealloc || (mp<tape+BOFF && (a || mp<tape)))
	{
	    fprintf(stderr,
		    "Error: Tape pointer has moved to position %d\n",
		    (int)(mp-tape-BOFF));
	    exit(42);
	}
#endif
	switch(p[1]) {
	case T_ADD: *mp += p[2]; p+=3; break;
	case T_SET: *mp = p[2]; p+=3; break;
	case T_END: if ((*mp&=msk)!=0) p+= p[2]; p+=3; break;
	case T_WHL: if ((*mp&=msk)==0) p+= p[2]; p+=3; break;
	case T_BEG: a = (*mp&=msk); p+=2; break;
	case T_MUL1: *mp += a; p+=2; break;
	case T_ZFIND: while((*mp&msk)) mp += p[2]; p+=3; break;
	case T_RAILC: while((*mp&msk)) {*mp -=1; mp += p[2]; } p+=3; break;
	case T_RAILZ: while((*mp&msk)) {*mp =0; mp += p[2]; } p+=3; break;
	case T_PRT: putchar(*mp); p+=2; break;
	case T_INP: if((a=getchar()) != EOF) *mp = a; p+=2; break;
	case T_MUL: *mp += p[2] * a; p+=3; break;
	case T_QSET: if(a) *mp = p[2]; p+=3; break;
	case T_QMUL: if(a) *mp += p[2]*a; p+=3; break;
	case T_QMUL1: if(a) *mp += a; p+=2; break;
	case T_STOP: return;
#ifdef PART2
	case T_DUMP: dumpmem(mp); p+=2; break;
#endif
	}
    }
}

#ifndef PART2
#define PART2
#include __FILE__
#endif
