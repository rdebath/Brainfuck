#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Whitespace translation
 */

int ind = 0;
int loopid = 1;
int embed_tokens = 0;
int signed_label_bug = 0;

struct stkdat { struct stkdat * up; int id; } *sp = 0;

static void print_string(void);
static void prttok(char *, char *);
static void putsnum(long val);
static void putunum(unsigned long num);
static void putlabel(unsigned long num);

#ifndef BF2SEMI

#define TAPE_PREALLOC	1
#define TAPE_PREALLOC_QUICK	0

#define CMD_ADD		"\t   "
#define CMD_CALL	"\n \t"
#define CMD_DIV		"\t \t "
#define CMD_DROP	" \n\n"
#define CMD_DUP		" \n "
#define CMD_EXIT	"\n\n\n"
#define CMD_FETCH	"\t\t\t"
#define CMD_JMP		"\n \n"
#define CMD_JN		"\n\t\t"
#define CMD_JZ		"\n\t "
#define CMD_LABEL	"\n  "
#define CMD_MOD		"\t \t\t"
#define CMD_MUL		"\t  \n"
#define CMD_OUTCHAR	"\t\n  "
#define CMD_OUTNUM	"\t\n \t"
#define CMD_PUSH	"  "
#define CMD_READCHAR	"\t\n\t "
#define CMD_READNUM	"\t\n\t\t"
#define CMD_RET		"\n\t\n"
#define CMD_STORE	"\t\t "
#define CMD_SUB		"\t  \t"
#define CMD_SWAP	" \n\t"

#define BIT_ZERO	" "
#define BIT_ONE		"\t"
#define END_NUM		"\n"

#else

#define BROKEN_CMD_CALL	" ;⁏"
#define BROKEN_CMD_RET	" ; "

#define CMD_ADD		"⁏;;"
#define CMD_DROP	";⁏⁏"
#define CMD_DUP		";;⁏"
#define CMD_EXIT	"  ;"
#define CMD_FETCH	"; ⁏"
#define CMD_JMP		" ⁏ "
#define CMD_JZ		" ⁏;"
#define CMD_LABEL	" ;;"
/* Semicolon has arguments to MOD, DIV and SUB swapped relative to WS and Forth
 * (the additive operations may also be swapped but it's impossible to tell) */
#define CMD_MOD		CMD_SWAP "⁏  "
#define CMD_MUL		"⁏⁏;"
#define CMD_OUTCHAR	"⁏ ;;"
#define CMD_OUTNUM	"⁏ ;⁏"
#define CMD_PUSH	";;;"
#define CMD_READCHAR	"⁏ ⁏;"
#define CMD_STORE	"; ;"
#define CMD_SWAP	";⁏;"

#define BIT_ZERO	";"
#define BIT_ONE		"⁏"
#define END_NUM		"\n"

#define AFTER_END \
	printf(CMD_LABEL); \
	putlabel(0); /* End the program with a newline */

#endif

#define PRTTOK(s)	prttok("(" #s ")", CMD_##s);

static check_arg_t fn_check_arg;
struct be_interface_s be_interface = {fn_check_arg};

static int
fn_check_arg(const char * arg)
{
    if (strcmp(arg, "-M") == 0) {
	tapelen = 0;
	return 1;
    }
    if (strcmp(arg, "-tokens") == 0) {
	embed_tokens = 1;
	return 1;
    }
    if (strcmp(arg, "-signbug") == 0) {
	signed_label_bug = 1;
	return 1;
    }
    return 0;
}

#define BYTEWRAP()	\
    PRTTOK(DUP);	\
    PRTTOK(DUP);	\
    PRTTOK(FETCH);	\
    PRTTOK(PUSH);	\
    putsnum(256);	\
    PRTTOK(MOD);	\
    PRTTOK(STORE);

#if defined(CMD_CALL)
static int bytewrap_label = -1;
#endif

static void
do_bytewrap(void)
{
    if(!bytecell) return;
#ifndef CMD_CALL
    BYTEWRAP();
#else
    PRTTOK(CALL);
    putlabel(bytewrap_label);
#endif
}

void
outcmd(int ch, int count)
{
    if (embed_tokens) {
	if (count == 1)
	    printf("{%c}", ch);
	else if (enable_be_optim || count == 0)
	    printf("{%c%d}", ch, count);
	else {
	    int i;
	    putchar('{');
	    for(i=0; i<count; i++)
		putchar(ch);
	    putchar('}');
	}
    }

#define WRITETOS \
	printf(CMD_DUP); \
	printf(CMD_OUTNUM); \
	printf(CMD_PUSH); \
	putsnum(10); \
	printf(CMD_OUTCHAR);

    if (ch == 'N' || ch == 'n')
	count = -count;

    switch(ch) {
    case '!':

#if TAPE_PREALLOC_QUICK
	if (tapelen) {
	    /* The Haskell interpreter needs us to poke the highest cell we
	     * want to use before we read any before that cell.
	     */
	    PRTTOK(PUSH);
	    putsnum(tapesz+2);
	    PRTTOK(PUSH);
	    putsnum(0);
	    PRTTOK(STORE);
	}
#elif TAPE_PREALLOC
	if (tapelen) {
	    /* Some WS interpreters need EVERY cell cleared manually before we
	     * use them.
	     */
	    PRTTOK(PUSH); putsnum(0);
	    PRTTOK(LABEL); putlabel(loopid);
	    PRTTOK(DUP);
	    PRTTOK(PUSH); putsnum(tapesz+4);
	    PRTTOK(SUB);
	    PRTTOK(JZ); putlabel(loopid+1);
	    PRTTOK(DUP);
	    PRTTOK(PUSH); putsnum(0);
	    PRTTOK(STORE);
	    PRTTOK(PUSH); putsnum(1);
	    PRTTOK(ADD);
	    PRTTOK(JMP); putlabel(loopid);
	    PRTTOK(LABEL); putlabel(loopid+1);
	    PRTTOK(DROP);
	    loopid += 2;
	}
#endif

#if defined(CMD_CALL)
	if(bytecell) {
	    /* The Semicolon language doesn't have a working call instruction
	     * without it the %256 for byte cells takes up a lot of space.
	     */
	    PRTTOK(JMP);
	    putlabel(loopid+1);
	    PRTTOK(LABEL);
	    putlabel(loopid);
	    BYTEWRAP();
	    PRTTOK(RET);
	    PRTTOK(LABEL);
	    putlabel(loopid+1);
	    bytewrap_label = loopid;
	    loopid += 2;
	}
#endif

	PRTTOK(PUSH);
	putsnum(tapeinit+2);

	break;

    case '~':
	PRTTOK(DROP);
	PRTTOK(EXIT);
#ifdef AFTER_END
	AFTER_END;
#endif
	break;

    case '=':
	PRTTOK(DUP);
	PRTTOK(PUSH);
	putsnum(count);
	PRTTOK(STORE);
	break;
    case 'B':
	if(bytecell) do_bytewrap();
	PRTTOK(DUP);
	PRTTOK(FETCH);
	PRTTOK(PUSH);
	putsnum(0);
	PRTTOK(SWAP);
	PRTTOK(STORE);
	break;

    case 'N': case 'S': case 'M':
	PRTTOK(DUP);
	PRTTOK(DUP);
	PRTTOK(FETCH);
	PRTTOK(PUSH);
	putsnum(0);
	PRTTOK(FETCH);
	if (count != 1) {
	    PRTTOK(PUSH);
	    putsnum(count);
	    PRTTOK(MUL);
	}
	PRTTOK(ADD);
	PRTTOK(STORE);
	break;

    case 'Q':
	PRTTOK(PUSH);
	putsnum(0);
	PRTTOK(FETCH);
	PRTTOK(JZ);
	putlabel(loopid);

	PRTTOK(DUP);
	PRTTOK(PUSH);
	putsnum(count);
	PRTTOK(STORE);

	PRTTOK(LABEL);
	putlabel(loopid);
	loopid++;
	break;

    case 'n': case 's': case 'm':
	PRTTOK(PUSH);
	putsnum(0);
	PRTTOK(FETCH);
	PRTTOK(JZ);
	putlabel(loopid);

	PRTTOK(DUP);
	PRTTOK(DUP);
	PRTTOK(FETCH);
	PRTTOK(PUSH);
	putsnum(0);
	PRTTOK(FETCH);
	if (count != 1) {
	    PRTTOK(PUSH);
	    putsnum(count);
	    PRTTOK(MUL);
	}
	PRTTOK(ADD);
	PRTTOK(STORE);

	PRTTOK(LABEL);
	putlabel(loopid);
	loopid++;
	break;

    case 'X':
	{
	    char * s = "Aborted Infinite Loop.\n";
	    for(;*s; s++) {
		PRTTOK(PUSH);
		putsnum(*s);
		PRTTOK(OUTCHAR);
	    }
	    PRTTOK(EXIT);
	}
	break;

    case '-':
	count = -count;
    case '+':
	PRTTOK(DUP);
	PRTTOK(DUP);
	PRTTOK(FETCH);
	PRTTOK(PUSH);
	putsnum(count);
	PRTTOK(ADD);
	PRTTOK(STORE);
	break;
    case '<':
	PRTTOK(PUSH);
	putsnum(-count);
	PRTTOK(ADD);
	break;
    case '>':
	PRTTOK(PUSH);
	putsnum(count);
	PRTTOK(ADD);
	break;

    case '[':
        {
            struct stkdat * n = malloc(sizeof*n);
            n->up = sp;
            sp = n;
            n->id = loopid;
            loopid+=2;

	    if(bytecell) do_bytewrap();
	    ind++;

	    PRTTOK(LABEL);
	    putlabel(n->id);
	    PRTTOK(DUP);
	    PRTTOK(FETCH);

	    PRTTOK(JZ);
	    putlabel(n->id+1);
        }
        break;

    case ']':
        {
            struct stkdat * n = sp;
            sp = n->up;

	    if(bytecell) do_bytewrap();
	    ind--;

	    PRTTOK(JMP);
	    putlabel(n->id);
	    PRTTOK(LABEL);
	    putlabel(n->id+1);
            free(n);
        }
        break;

    case '"': print_string(); break;

    case '.':
	PRTTOK(DUP);
	PRTTOK(FETCH);
	PRTTOK(OUTCHAR);
	break;

    case ',':
	PRTTOK(DUP);
	PRTTOK(READCHAR);
	break;
    }
}

static void
print_string(void)
{
    char * str = get_string();

    if (!str) return;

    for(;*str; str++) {
	PRTTOK(PUSH);
	putsnum(*str);
	while(*str == str[1]) {
	    str++;
	    PRTTOK(DUP);
	    PRTTOK(OUTCHAR);
	}
	PRTTOK(OUTCHAR);
    }
}

static void
prttok(char * comment, char * token)
{
    if (embed_tokens)
	printf("%s", comment);
    printf("%s", token);
}

static void
putlabel(unsigned long num)
{
    if (signed_label_bug)
	putsnum(num);
    else
	putunum(num);
}

static void
putsnum(long val)
{
    if (val >= 0) {
	prttok("(pos)", BIT_ZERO);
	putunum(val);
    } else {
	prttok("(neg)", BIT_ONE);
	putunum(-val);
    }
}

static void
putunum(unsigned long num)
{
    unsigned long v, max;

    if (embed_tokens)
	printf("(%ld:", num);

    max = 1; v = num;
    for(;;) {
	v /= 2;
	if (v == 0) break;
	max *= 2;
    }
    for(;;) {
	v = num / max;
	num = num % max;
	if (v == 0) printf(BIT_ZERO); else printf(BIT_ONE);
	if (max == 1) break;
	max /= 2;
    }

    if (embed_tokens)
	printf(")");

    printf(END_NUM);
}
