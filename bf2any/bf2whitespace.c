#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Whitespace translation
 */

static int loopid = 1;
static int embed_tokens = 0;
static int signed_label_bug = 0;
static int semicolon = 0;
static int bytewrap_label = -1;
static int call_works = 1;
static enum {no_init, quick_init, full_init} init_type = full_init;

static struct stkdat { struct stkdat * up; int id; } *sp = 0;

static void print_string(void);
static void prttok(char *, char *);
static void putsnum(long val);
static void putunum(unsigned long num);
static void putlabel(unsigned long num);

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

/* This is the alternate "semicolon" syntax */

#define SCMD_CALL	" ;⁏"	/* This doesn't work in the reference interpreter */
#define SCMD_RET	" ; "	/* This doesn't work in the reference interpreter */

#define SCMD_ADD	"⁏;;"
#define SCMD_DROP	";⁏⁏"
#define SCMD_DUP	";;⁏"
#define SCMD_EXIT	"  ;"
#define SCMD_FETCH	"; ⁏"
#define SCMD_JMP	" ⁏ "
#define SCMD_JZ		" ⁏;"
#define SCMD_LABEL	" ;;"
/* Semicolon has arguments to MOD, DIV and SUB swapped relative to WS and Forth
 * (the additive operations may also be swapped but it's impossible to tell) */
#define SCMD_MOD	SCMD_SWAP "⁏  "
#define SCMD_MUL	"⁏⁏;"
#define SCMD_OUTCHAR	"⁏ ;;"
#define SCMD_OUTNUM	"⁏ ;⁏"
#define SCMD_PUSH	";;;"
#define SCMD_READCHAR	"⁏ ⁏;"
#define SCMD_STORE	"; ;"
#define SCMD_SUB	"⁏;⁏"
#define SCMD_SWAP	";⁏;"

#define SBIT_ZERO	";"
#define SBIT_ONE	"⁏"
#define SEND_NUM	"\n"

#define SAFTER_END \
	printf(SCMD_LABEL); \
	putlabel(0); /* End the program with a newline */

#define PRTTOK(s)	prttok("(" #s ")", semicolon? SCMD_##s : CMD_##s);

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
    if (strcmp(arg, "-semi") == 0) {
	semicolon = 1;
	call_works = 0;
	return 1;
    }

    if (strcmp(arg, "-quickinit") == 0) { init_type = quick_init; return 1; }
    if (strcmp(arg, "-fullinit") == 0) { init_type = full_init; return 1; }
    if (strcmp(arg, "-noinit") == 0) { init_type = no_init; return 1; }

    if (strcmp("-h", arg) ==0) {
        fprintf(stderr, "%s\n",
        "\t"    "-tokens Include token names in output."
        "\n\t"  "-semi   Generate code for 'semicolon' language"
        "\n\t"  "-signbug    Interpreter has signed int bug"
        "\n\t"  "-fullinit   Init every tape cell."
        "\n\t"  "-quickinit  Write to last tape cell."
        );
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


static void
do_bytewrap(void)
{
    if(!bytecell) return;
    if (!call_works) {
	BYTEWRAP();
    } else {
	PRTTOK(CALL);
	putlabel(bytewrap_label);
    }
}

void
outcmd(int ch, int count)
{
    if (embed_tokens) {
	if (count == 1)
	    printf("{%c}", ch);
	else
	    printf("{%c%d}", ch, count);
    }

#define WRITETOS \
	printf(CMD_DUP); \
	printf(CMD_OUTNUM); \
	printf(CMD_PUSH); \
	putsnum(10); \
	printf(CMD_OUTCHAR);

    if (ch == 'N') { ch = 'M'; count = -count; }
    if (ch == 'S') { ch = 'M'; count = 1; }
    if (ch == 'T') { ch = 'M'; count = -1; }

    switch(ch) {
    case '!':

	if (tapelen) {
	    switch(init_type)
	    {
	    case no_init: break;
	    case quick_init:
		/* The Haskell interpreter needs us to poke the highest cell we
		 * want to use before we read any before that cell.
		 */
		PRTTOK(PUSH);
		putsnum(tapesz+2);
		PRTTOK(PUSH);
		putsnum(0);
		PRTTOK(STORE);
		break;

	    case full_init:
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
		break;
	    }
	}

	if(bytecell && call_works) {
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

	PRTTOK(PUSH);
	putsnum(tapeinit+2);

	break;

    case '~':
	PRTTOK(DROP);
	PRTTOK(EXIT);
	if (semicolon) {
	    SAFTER_END;
	}
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

    case 'M':
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

	    PRTTOK(LABEL);
	    putlabel(n->id);

	    if(bytecell) do_bytewrap();

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

	    PRTTOK(JMP);
	    putlabel(n->id);
	    PRTTOK(LABEL);
	    putlabel(n->id+1);
            free(n);
        }
        break;

    case 'I':
        {
            struct stkdat * n = malloc(sizeof*n);
            n->up = sp;
            sp = n;
            n->id = loopid;
            loopid++;

	    if(bytecell) do_bytewrap();

	    PRTTOK(DUP);
	    PRTTOK(FETCH);

	    PRTTOK(JZ);
	    putlabel(n->id);
        }
        break;

    case 'E':
        {
            struct stkdat * n = sp;
            sp = n->up;

	    PRTTOK(LABEL);
	    putlabel(n->id);
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
	if (semicolon)
	    prttok("(pos)", SBIT_ZERO);
	else
	    prttok("(pos)", BIT_ZERO);
	putunum(val);
    } else {
	if (semicolon)
	    prttok("(neg)", SBIT_ONE);
	else
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
	if (semicolon) {
	    if (v == 0) printf(SBIT_ZERO); else printf(SBIT_ONE);
	} else {
	    if (v == 0) printf(BIT_ZERO); else printf(BIT_ONE);
	}
	if (max == 1) break;
	max /= 2;
    }

    if (embed_tokens)
	printf(")");

    if (semicolon) printf(SEND_NUM); else printf(END_NUM);
}
