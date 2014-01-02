#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Integer translation from BF, runs at about 1,000,000,000 instructions per second.
 *
 */

int do_input = 0;
int do_dump = 0;

#define MEMSIZE 65536

int
check_arg(char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
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

int *mem = 0, *m = 0, memlen = 0;
#define TOKEN_LIST(Mac) \
    Mac(STOP) Mac(ADD) Mac(PRT) Mac(INP) Mac(WHL) Mac(END) \
    Mac(SET) Mac(BEG) Mac(MUL) Mac(MUL1) Mac(QSET) Mac(QMUL) Mac(QMUL1) \
    Mac(ZFIND) Mac(RAILC) Mac(DUMP)

#define GEN_TOK_ENUM(NAME) T_ ## NAME,
enum token { TOKEN_LIST(GEN_TOK_ENUM) TCOUNT};
#define GEN_TOK_STRING(NAME) #NAME,
const char* tokennames[] = { TOKEN_LIST(GEN_TOK_STRING) };

struct stkdat { struct stkdat * up; int id; } *sp = 0;
int imov = 0;
int prevtk = 0;

void runprog(int * p, int *m);
void dumpprog(int * p, int *m);
void dumpmem(int *mp);

void
outcmd(int ch, int count)
{
    int t = -1;

    if (memlen < m-mem+MEMSIZE + BOFF) {
	size_t s = m-mem + MEMSIZE + BOFF + 8192;
	int * p;
	p = realloc(mem, s * sizeof(int));

	memset(p+memlen, '\0', (s-memlen) * sizeof(int));
	m = (m-mem + p);
	mem = p;
	memlen = s;
    }

    if (ch != '>' && ch != '<') {
	*m++ = imov;
	imov = 0;
    }

    switch(ch) {
    default: m--; imov = *m; break; /* Hmm; oops */

    case '=': *m++ = t = T_SET; *m++ = count; break;
    case 'B': *m++ = t = T_BEG; break;
    case 'M': *m++ = t = T_MUL; *m++ = count; break;
    case 'N': *m++ = t = T_MUL; *m++ = -count; break;
    case 'S': *m++ = t = T_MUL1; break;

    case 'Q': *m++ = t = T_QSET; *m++ = count; break;
    case 'm': *m++ = t = T_QMUL; *m++ = count; break;
    case 'n': *m++ = t = T_QMUL; *m++ = -count; break;
    case 's': *m++ = t = T_QMUL1; break;

    case '+': *m++ = t = T_ADD; *m++ = count; break;
    case '-': *m++ = t = T_ADD; *m++ = -count; break;
    case '<': imov -= count; break;
    case '>': imov += count; break;
    case 'X': *m++ = t = T_STOP; break;
    case ',': *m++ = t = T_INP; break;
    case '.': *m++ = t = T_PRT; break;
    case '#': *m++ = t = T_DUMP; break;
    case '~':
	*m++ = t = T_STOP;
	if (do_dump) {
	    dumpprog(mem, m);
	    return;
	}
	setbuf(stdout, 0);
	if (m-mem < BOFF)
	    m = mem+BOFF;
	runprog(mem, m);
	break;

    case '[':
	{
	    struct stkdat * n = malloc(sizeof*n);
	    n->up = sp;
	    sp = n;
	    *m++ = t = T_WHL;
	    n->id = m-mem;
	    *m++ = 0;	/* Default to NOP */
	}
	break;

    case ']':
	*m++ = t = T_END;
	if (sp) {
	    struct stkdat * n = sp;
	    sp = n->up;
	    mem[n->id] = (m-mem) - n->id;
	    *m++ = -mem[n->id];
	    free(n);
	} else
	    *m++ = 0;	/* On error NOP */

	if ((prevtk & 0xFF) == T_WHL && m[-3] != 0) {
	    /* [<<<] loop */
	    m[-5] = T_ZFIND;
	    m[-4] = m[-3];
	    m -= 3;
	} else
	if ((prevtk & 0xFFFF) == ((T_WHL<<8) + T_ADD) && m[-4] == -1 && m[-6] == 0 && m[-3] != 0) {
	    /* [-<<<] loop */
	    m[-8] = T_RAILC;
	    m[-7] = m[-3];
	    m -= 6;
	}
	/* TODO: Add special for [-<<<+] */
	break;
    }

    if (t>=0 && enable_optim) {
	prevtk = (prevtk << 8) + t;
    }
}

#ifdef __GUNC__
/* Tell GNU C to think really hard about this function! */
__attribute((optimize(3),hot,aligned(64)))
#endif
void
runprog(register int * p, register int *mp)
{
    register int a = 0;
    const int msk = (bytecell)?0xFF:-1;
    for(;;){
	mp += p[0];
	switch(p[1]) {
	case T_ADD: *mp += p[2]; p+=3; break;
	case T_SET: *mp = p[2]; p+=3; break;
	case T_END: if ((*mp&msk)!=0) p+= p[2]; p+=3; break;
	case T_WHL: if ((*mp&msk)==0) p+= p[2]; p+=3; break;
	case T_BEG: a = (*mp&msk); p+=2; break;
	case T_MUL1: *mp += a; p+=2; break;
	case T_ZFIND: while((*mp&msk)) mp += p[2]; p+=3; break;
	case T_RAILC: while((*mp&msk)) {*mp -=1; mp += p[2]; } p+=3; break;
	case T_PRT: putchar(*mp); p+=2; break;
	case T_INP: if((a=getchar()) != EOF) *mp = a; p+=2; break;
	case T_MUL: *mp += p[2] * a; p+=3; break;
	case T_QSET: if(a) *mp = p[2]; p+=3; break;
	case T_QMUL: if(a) *mp += p[2]*a; p+=3; break;
	case T_QMUL1: if(a) *mp += a; p+=2; break;
	case T_STOP: return;
	case T_DUMP: dumpmem(mp); p+=2; break;
	}
    }
}

void
dumpmem(int *mp)
{
    int i, j = 0;
    for (i = 0; i < MEMSIZE; i++) if (m[i]) j = i + 1;
    fprintf(stderr, "Ptr: %3d, mem:", (int)(mp-m));
    for (i = 0; i < j; i++)
	fprintf(stderr, "%s%d", m + i == mp ? ">" : " ", m[i]);
    fprintf(stderr, "\n");
}

void
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
	case T_ZFIND: case T_RAILC:
	    printf(" %d", *p++);
	    break;
	default:
	    printf(" DECODE ERROR\n");
	    return;
	}
	printf("\n");
    }
}
