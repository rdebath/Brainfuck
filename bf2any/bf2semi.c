#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Semicolon translation from BF, runs at about 0 instructions per second.
 */

int ind = 0;
int loopid = 1;

struct stkdat { struct stkdat * up; int id; } *sp = 0;

static void print_string(void);

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    if (strcmp(arg, "-savestring") == 0) return 1;
    if (strcmp(arg, "-M") == 0) {
	tapelen = 0;
	return 1;
    }
    return 0;
}

static void
putnum(unsigned long num)
{
    unsigned long v, max;

    max = 1; v = num;
    for(;;) {
	v /= 2;
	if (v == 0) break;
	max *= 2;
    }
#ifdef SEMIBUG
    putchar('#');
#endif
    for(;;) {
	v = num / max;
	num = num % max;
	if (v == 0) putchar(';'); else printf("⁏");
	if (max == 1) break;
	max /= 2;
    }
    putchar('\n');
}

static void
putsnum(long val)
{
#ifdef SEMIBUG
    putchar('?');
#endif
    if (val >= 0) {
	putchar(';');
	putnum(val);
    } else {
	printf("⁏");
	putnum(-val);
    }
}

static void
do_bytewrap(void)
{
    if(!bytecell) return;

    printf(";;⁏"); /* dup */
    printf(";;⁏"); /* dup */
    printf("; ⁏"); /* fetch */
    printf(";;;"); /* push */
    putsnum(256);
    printf(";⁏;"); /* swap */
    printf("⁏  "); /* mod */
    printf("; ;"); /* store */
}

void
outcmd(int ch, int count)
{
#ifdef SEMIBUG
    if (count == 1)
	printf("{%c}", ch);
    else
	printf("{%c%d}", ch, count);
#endif

#define WRITETOS \
	printf(";;⁏"); /* dup */ \
	printf("⁏ ;⁏"); /* outnum */ \
	printf(";;;"); /* push */ \
	putsnum(10); \
	printf("⁏ ;;"); /* outchar */

    if (ch == 'N' || ch == 'n')
	count = -count;

    switch(ch) {
    case '!':
	printf(";;;");
	putsnum(tapeinit+2);
	break;

    case '~':
	printf(";⁏⁏"); /* discard */
	printf("  ;"); /* exit */
	printf(" ;;"); /* label */
	putnum(0); /* End the program with a newline */
	break;

    case '=':
	printf(";;⁏"); /* dup */
	printf(";;;"); /* push */
	putsnum(count);
	printf("; ;"); /* store */
	break;
    case 'B':
	if(bytecell) do_bytewrap();
	printf(";;⁏"); /* dup */
	printf("; ⁏"); /* fetch */
	printf(";;;"); /* push */
	putsnum(0);
	printf(";⁏;"); /* swap */
	printf("; ;"); /* store */
	break;

    case 'N': case 'S': case 'M':
	printf(";;⁏"); /* dup */
	printf(";;⁏"); /* dup */
	printf("; ⁏"); /* fetch */
	printf(";;;"); /* push */
	putsnum(0);
	printf("; ⁏"); /* fetch */
	if (count != 1) {
	    printf(";;;"); /* push */
	    putsnum(count);
	    printf("⁏⁏;"); /* mul */
	}
	printf("⁏;;"); /* add */
	printf("; ;"); /* store */
	break;

    case 'Q':
	printf(";;;"); /* push */
	putsnum(0);
	printf("; ⁏"); /* fetch */
	printf(" ⁏;"); /* jz */
	putnum(loopid);

	printf(";;⁏"); /* dup */
	printf(";;;"); /* push */
	putsnum(count);
	printf("; ;"); /* store */

	printf(" ;;"); /* label */
	putnum(loopid);
	loopid++;
	break;

    case 'n': case 's': case 'm':
	printf(";;;"); /* push */
	putsnum(0);
	printf("; ⁏"); /* fetch */
	printf(" ⁏;"); /* jz */
	putnum(loopid);

	printf(";;⁏"); /* dup */
	printf(";;⁏"); /* dup */
	printf("; ⁏"); /* fetch */
	printf(";;;"); /* push */
	putsnum(0);
	printf("; ⁏"); /* fetch */
	if (count != 1) {
	    printf(";;;"); /* push */
	    putsnum(count);
	    printf("⁏⁏;"); /* mul */
	}
	printf("⁏;;"); /* add */
	printf("; ;"); /* store */

	printf(" ;;"); /* label */
	putnum(loopid);
	loopid++;
	break;

    case 'X':
	{
	    char * s = "Aborted Infinite Loop.\n";
	    for(;*s; s++) {
		printf(";;;"); /* push */
		putsnum(*s);
		printf("⁏ ;;"); /* outchar */
	    }
	    printf("  ;"); /* exit */
	}
	break;

    case '-':
	count = -count;
    case '+':
	printf(";;⁏"); /* dup */
	printf(";;⁏"); /* dup */
	printf("; ⁏"); /* fetch */
	printf(";;;"); /* push */
	putsnum(count);
	printf("⁏;;"); /* add */
	printf("; ;"); /* store */
	break;
    case '<':
	printf(";;;"); /* push */
	putsnum(-count);
	printf("⁏;;"); /* add */
	break;
    case '>':
	printf(";;;"); /* push */
	putsnum(count);
	printf("⁏;;"); /* add */
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

	    printf(" ;;"); /* label */
	    putnum(n->id);
	    printf(";;⁏"); /* dup */
	    printf("; ⁏"); /* fetch */

	    printf(" ⁏;"); /* jz */
	    putnum(n->id+1);
        }
        break;

    case ']':
        {
            struct stkdat * n = sp;
            sp = n->up;

	    if(bytecell) do_bytewrap();
	    ind--;

	    printf(" ⁏ "); /* jump */
	    putnum(n->id);
	    printf(" ;;"); /* label */
	    putnum(n->id+1);
            free(n);
        }
        break;

    case '"': print_string(); break;

    case '.':
	printf(";;⁏"); /* dup */
	printf("; ⁏"); /* fetch */
	printf("⁏ ;;"); /* outchar */
	break;

    case ',':
	printf(";;⁏"); /* dup */
	printf("⁏ ⁏;"); /* readchar */
	break;
    }
}

static void
print_string(void)
{
    char * str = get_string();

    if (!str) return;

    for(;*str; str++) {
	printf(";;;"); /* push */
	putsnum(*str);
	printf("⁏ ;;"); /* outchar */
    }
}
