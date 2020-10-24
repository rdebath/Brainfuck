#ifndef PART

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "bf2any.h"

/*
 * Integer translation from BF, runs at about 1,000,000,000 instructions per second.
 *
 */

static int do_dump = 0;

typedef unsigned int icell;

static int *mem = 0, *mptr = 0;
static size_t memlen = 0;
static icell * tape = 0;
static size_t tapealloc;

#define TOKEN_LIST(Mac) \
    Mac(STOP) Mac(ADD) Mac(PRT) Mac(INP) Mac(WHL) Mac(END) \
    Mac(SET) Mac(BEG) Mac(MUL) Mac(MUL1) Mac(SETNV) Mac(MULV) \
    Mac(DIVV) Mac(MODV) Mac(SETV) \
    Mac(ZFIND) Mac(RAILC) Mac(RAILZ) Mac(CHR) \
    Mac(DUMP) Mac(NOP)

#define GEN_TOK_ENUM(NAME) T_ ## NAME,
enum token { TOKEN_LIST(GEN_TOK_ENUM) TCOUNT};
#define GEN_TOK_STRING(NAME) #NAME,
static const char* tokennames[] = { TOKEN_LIST(GEN_TOK_STRING) };

static struct stkdat { struct stkdat * up; int id; } *sp = 0;
static int imov = 0;
static int prevtk = 0;
static int checklimits = 0;

static void runprog_byte(register int * p, register unsigned char *mp);
static void runprog_int(register int * p, register unsigned int *mp);
static void debugprog(int * p, icell *ep);
static void dumpprog(int * p, int *ep);
static void dumpmem(icell *tp);

static check_arg_t fn_check_arg;
static gen_code_t gen_code;
struct be_interface_s be_interface = {
    .check_arg=fn_check_arg, .gen_code=gen_code,
    .cells_are_ints=1, .hasdebug=1};

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

static void testmem() {
    if (mem+memlen < mptr + 8) {
	size_t s = memlen + 1024;
	int * p;
	p = realloc(mem, s * sizeof(int));

	memset(p+memlen, '\0', (s-memlen) * sizeof(int));
	mptr = (mptr-mem + p);
	mem = p;
	memlen = s;
    }
}

static void
gen_code(int ch, int count, char * strn)
{
    int t = -1;

    testmem();

    if (ch != '>' && ch != '<' && ch != '"') {
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
    case 'T': *mptr++ = t = T_MUL; *mptr++ = -1; break;
    case '*': *mptr++ = t = T_MULV; break;
    case '/': *mptr++ = t = T_DIVV; break;
    case '%': *mptr++ = t = T_MODV; break;

    case 'C': *mptr++ = t = T_SETNV; *mptr++ = count; break;
    case 'D': *mptr++ = t = T_SETNV; *mptr++ = -count; break;
    case 'V': *mptr++ = t = T_SETV; break;
    case 'W': *mptr++ = t = T_SETNV; *mptr++ = -1; break;

    case '+': *mptr++ = t = T_ADD; *mptr++ = count; break;
    case '-': *mptr++ = t = T_ADD; *mptr++ = -count; break;
    case '<': imov -= count; break;
    case '>': imov += count; break;
    case 'X': *mptr++ = t = T_STOP; break;
    case ',': *mptr++ = t = T_INP; break;
    case '.': *mptr++ = t = T_PRT; break;
    case '#': *mptr++ = t = T_DUMP; checklimits = 1; break;
    case '"':
	{
	    char * str = strn;
	    if (!str) break;
	    for(; *str; str++) {
		testmem();
		*mptr++ = 0;
		*mptr++ = T_CHR;
		*mptr++ = *str;
	    }
	    break;
	}
    case '~':
	*mptr++ = t = T_STOP;
	if (do_dump) {
	    dumpprog(mem, mptr);
	    return;
	}
	if (isatty(fileno(stdout))) setbuf(stdout, 0);

	tapealloc = tapesz;
	if(!checklimits) {
	    if (bytecell) {
		unsigned char * btape;
		btape = calloc(tapealloc, sizeof(unsigned char));
		runprog_byte(mem, btape+tapeinit);
	    } else {
		unsigned int * itape;
		itape = calloc(tapealloc, sizeof(unsigned int));
		runprog_int(mem, itape+tapeinit);
	    }
	} else {
	    tape = calloc(tapealloc, sizeof(icell));
	    debugprog(mem, tape+tapeinit);
	}
	break;

    case 'I':
	{
	    struct stkdat * n = malloc(sizeof*n);
	    n->up = sp;
	    sp = n;
	    *mptr++ = t = T_WHL;
	    n->id = mptr-mem;
	    *mptr++ = 0;	/* Default to NOP */
	}
	break;

    case 'E':
	{
	    int offt;
	    if (mptr[-1]) offt = 1; else offt = 3;
	    *mptr++ = T_NOP;
	    if (sp) {
		struct stkdat * n = sp;
		sp = n->up;
		mem[n->id] = (mptr-mem) - n->id - offt;
		free(n);
	    }
	    if (offt != 1) mptr-=2;
	}
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
dumpmem(icell *mp)
{
    size_t i, j = 0;
    const icell msk = (bytecell)?0xFF:-1;
    for (i = 0; i < tapealloc; i++) if (tape[i]&msk) j = i + 1;
    fprintf(stderr, "Ptr: %3d, mem:", (int)(mp-tape-tapeinit));
    for (i = tapeinit; i < j; i++)
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
	case T_BEG: case T_MUL1:
	case T_SETV: case T_MULV:
	case T_DIVV: case T_MODV:
	case T_DUMP: case T_NOP:
	    break;
	case T_WHL: case T_END:
	    printf(" %d (%06d)", *p, (int)(p-mem + *p+1));
	    p++;
	    break;
	case T_SET: case T_ADD: case T_MUL: case T_SETNV:
	case T_ZFIND: case T_RAILC: case T_RAILZ: case T_CHR:
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

#ifndef PART
#define AM(_x) ((_x)&=msk)
#define M(_x) ((_x)&msk)
static void
debugprog(register int * p, register icell *mp)
#else
#undef AM
#undef M
#define AM(_x) (_x)
#define M(_x) (_x)

#if defined(__GNUC__) && ((__GNUC__>4) || (__GNUC__==4 && __GNUC_MINOR__>=4))
/* Tell GNU C to think really hard about this function! */
__attribute__((optimize(3),hot))
#endif

#undef tcell
#if PART == 2
#define tcell unsigned char
static void
runprog_byte(register int * p, register tcell *mp)
#else
#define tcell unsigned int
static void
runprog_int(register int * p, register tcell *mp)
#endif
#endif
{
#ifdef PART
    register tcell a = 1;
#else
    register icell a = 1;
    const icell msk = (bytecell)?0xFF:-1;
#endif
    for(;;){
	mp += p[0];
#ifndef PART
	if (mp>=tape+tapealloc || (mp<tape+tapeinit && (a || mp<tape)))
	{
	    fprintf(stderr,
		    "Error: Tape pointer has moved to position %d\n",
		    (int)(mp-tape-tapeinit));
	    exit(42);
	}
#endif
	switch(p[1]) {
	case T_ADD: *mp += p[2]; p+=3; break;
	case T_SET: *mp = p[2]; p+=3; break;
	case T_END: if (AM(*mp)!=0) p+= p[2]; p+=3; break;
	case T_WHL: if (AM(*mp)==0) p+= p[2]; p+=3; break;
	case T_BEG: a = AM(*mp); p+=2; break;

	case T_MUL1: *mp += a; p+=2; break;
	case T_MULV: *mp *= a; p+=2; break;
	case T_DIVV: *mp /= a; p+=2; break;
	case T_MODV: *mp %= a; p+=2; break;
	case T_SETV: *mp = a; p+=2; break;
	case T_MUL: *mp += p[2] * a; p+=3; break;
	case T_SETNV: *mp = p[2] * a; p+=3; break;

	case T_ZFIND: while(M(*mp)) mp += p[2]; p+=3; break;
	case T_RAILC: while(M(*mp)) {*mp -=1; mp += p[2]; } p+=3; break;
	case T_RAILZ: while(M(*mp)) {*mp =0; mp += p[2]; } p+=3; break;
	case T_PRT: putchar(*mp); p+=2; break;
	case T_CHR: putchar(p[2]); p+=3; break;
	case T_INP: {int i; if((i=getchar()) != EOF) *mp = i; } p+=2; break;
	case T_NOP: p += 2; break;
	case T_STOP: return;
#ifndef PART
	case T_DUMP: dumpmem(mp); p+=2; break;
#endif
	}
    }
}

#ifndef PART
#define PART 2
#include __FILE__
#undef PART
#define PART 3
#include __FILE__
#endif
