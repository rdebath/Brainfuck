#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * COBOL translation from BF, runs at about 480,000,000 instructions per second.
 * COBOL translation from BF, runs at about 12,000,000 instructions per second.
 *
 * The second speed is for decimal variables.
 *
 * NOTE: The input has a maximum line length and CANNOT detect EOF.
 */

/* The input must have preallocated space as PIC X has a defined length.
 * This is maximum line length that can be read. */
#define MAXINPLINE	132

static int ind = 7;
/* NB: Cobol too has a maximum line length (a really tiny one) */
#define I printf("%*s", (ind>27?27:ind), "")

static int use_decimal = 0;
static int do_input = 0;
static int maxinpline = MAXINPLINE;

static void print_cstring(char * str);

static check_arg_t fn_check_arg;
static gen_code_t gen_code;
struct be_interface_s be_interface = {.check_arg=fn_check_arg, .gen_code=gen_code};

static int
fn_check_arg(const char * arg)
{
    if (strcmp(arg, "-decimal") == 0) {
	use_decimal = 1;
	return 1;
    }
    if (strncmp(arg, "-l", 2) == 0 && arg[2]) {
	maxinpline = atoi(arg+2);
	if (maxinpline < 2) maxinpline = 2;
    }

    if (strcmp("-h", arg) ==0) {
        fprintf(stderr, "%s%d%s%d%s\n",
        "\t"    "-decimal Generate decimal type"
        "\n\t"  "-l",maxinpline,"    Input size of ",maxinpline," characters/line"
        );
        return 1;
    }

    return 0;
}

static void
gen_code(int ch, int count, char * strn)
{
    switch(ch) {
    case '!':
	I; printf("PROGRAM-ID. brainfuck.\n");
	if (!count) {
	    I; printf("DATA DIVISION.\n");
	    I; printf("WORKING-STORAGE SECTION.\n");

	    if (use_decimal) {
		I; printf("01 DEC-TAPE.\n");
		I; printf("  02 m    PIC S9(3) OCCURS %d TIMES.\n", tapesz);
		I; printf("01 p      PIC 9(9) VALUE %d.\n", tapeinit);
		I; printf("01 v      PIC S9(3).\n");
		I; printf("01 chr    PIC S9(3) GLOBAL.\n");
	    } else if (bytecell) {
		I; printf("01 CHAR-TAPE.\n");
		I; printf("  02 m    BINARY-CHAR UNSIGNED OCCURS %d TIMES.\n", tapesz);
		I; printf("01 p      BINARY-LONG VALUE %d.\n", tapeinit);
		I; printf("01 v      BINARY-CHAR UNSIGNED.\n");
		I; printf("01 chr    BINARY-LONG GLOBAL.\n");
	    } else {
		I; printf("01 WORD-TAPE.\n");
		I; printf("  02 m    BINARY-LONG UNSIGNED OCCURS %d TIMES.\n", tapesz);
		I; printf("01 p      BINARY-LONG VALUE %d.\n", tapeinit);
		I; printf("01 v      BINARY-LONG UNSIGNED.\n");
		I; printf("01 chr    BINARY-LONG GLOBAL.\n");
	    }
	    I; printf("01 inpl   PIC X(%d) GLOBAL.\n", maxinpline);
	    I; printf("01 goteof PIC 9 GLOBAL.\n");
	    I; printf("01 gotln  PIC 9 GLOBAL.\n");
	}
	I; printf("PROCEDURE DIVISION.\n");
	break;

    case '~':
	I; printf("STOP RUN.\n");

	if (do_input) {
	    printf("\n");
	    I; printf("PROGRAM-ID. getchr.\n");
	    I; printf("PROCEDURE DIVISION.\n");
	    I; printf("MOVE -1 TO chr\n");
	    I; printf("IF goteof EQUALS 1 THEN\n");
	    I; printf("  EXIT PROGRAM\n");
	    I; printf("END-IF\n");
	    I; printf("IF gotln EQUALS ZERO THEN\n");
	    I; printf("  ACCEPT inpl\n");
	    I; printf("  MOVE 1 TO gotln\n");
	    I; printf("END-IF\n");
	    I; printf("IF inpl EQUALS SPACES THEN\n");
	    I; printf("  MOVE ZERO TO gotln\n");
	    I; printf("  MOVE 10 TO chr\n");
	    I; printf("  EXIT PROGRAM\n");
	    I; printf("END-IF\n");
	    I; printf("MOVE FUNCTION ORD(inpl) TO chr\n");
	    I; printf("SUBTRACT 1 FROM chr\n");
	    I; printf("MOVE inpl (2:) TO inpl\n");
	    I; printf("EXIT PROGRAM.\n");

	    printf("\n");
	    I; printf("END PROGRAM getchr.\n");
	    I; printf("END PROGRAM brainfuck.\n");
	}

	break;

    case '=':
	if (bytecell) {
	    I; printf("MOVE %d TO m(p)\n", count & 0xFF);
	} else {
	    if (count < 0) {
		I; printf("MOVE 0 TO m(p)\n");
		I; printf("SUBTRACT %d FROM m(p)\n", -count);
	    } else {
		I; printf("MOVE %d TO m(p)\n", count);
	    }
	}
	break;
    case 'B':
	I; printf("MOVE m(p) TO v\n");
	break;
    case 'M': I; printf("COMPUTE m(p) EQUAL m(p)+v*%d\n", count); break;
    case 'N': I; printf("COMPUTE m(p) EQUAL m(p)-v*%d\n", count); break;
    case 'S': I; printf("ADD v TO m(p)\n"); break;
    case 'T': I; printf("SUBTRACT v FROM m(p)\n"); break;
    case '*': I; printf("COMPUTE m(p) EQUAL m(p)*v\n"); break;
    case '/': I; printf("COMPUTE m(p) EQUAL m(p)/v\n"); break;
    case '%': I; printf("COMPUTE m(p) EQUAL FUNCTION MOD(m(p), v)\n"); break;

    case 'C': I; printf("COMPUTE m(p) EQUAL v*%d\n", count); break;
    case 'D': I; printf("COMPUTE m(p) EQUAL -v*%d\n", count); break;
    case 'V': I; printf("MOVE v TO m(p)\n"); break;
    case 'W': I; printf("COMPUTE m(p) EQUAL -v\n"); break;

    case 'X': I; printf("DISPLAY \"Aborting Infinite Loop.\"\n");
              I; printf("STOP RUN RETURNING 1\n");
              break;

    case '+': I; printf("ADD %d TO m(p)\n", count); break;
    case '-': I; printf("SUBTRACT %d FROM m(p)\n", count); break;
    case '<': I; printf("SUBTRACT %d FROM p\n", count); break;
    case '>': I; printf("ADD %d TO p\n", count); break;
    case '[':
	I; printf("PERFORM UNTIL m(p) EQUALS ZERO\n");
	ind+=2;
	break;
    case ']':
	ind-=2; I; printf("END-PERFORM\n");
	break;

    case 'I':
	I; printf("IF NOT m(p) EQUALS ZERO THEN\n");
	ind+=2;
	break;
    case 'E':
	ind-=2; I; printf("END-IF\n");
	break;

    case '.': I; printf("MOVE m(p) TO chr\n");
	      I; printf("ADD 1 TO chr\n");
	      I; printf("DISPLAY FUNCTION CHAR(chr) WITH NO ADVANCING\n");
	      break;
    case '"': print_cstring(strn); break;
    case ',':
	      I; printf("CALL 'getchr'\n");
	      I; printf("IF chr GREATER THAN OR EQUAL TO ZERO THEN\n");
	      I; printf("  MOVE chr TO m(p)\n");
	      I; printf("END-IF\n");
	      do_input = 1;
	      break;
    }
}

static void
print_cstring(char * str)
{
    char buf[60];
    int badchar = 0;
    size_t outlen = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || badchar || outlen > sizeof(buf)-ind))
	{
	    buf[outlen] = 0;
	    if (badchar == '\n') {
		I; printf("DISPLAY \"%s\"\n", buf);
		badchar = 0;
	    } else {
		I; printf("DISPLAY \"%s\"\n", buf);
		I; printf("  WITH NO ADVANCING\n");
	    }
	    outlen = 0;
	}
	if (badchar) {
	    I; printf("DISPLAY FUNCTION CHAR(%d) WITH NO ADVANCING\n", badchar+1);
	    badchar = 0;
	}
	if (!*str) break;

	if (*str == '"') {
	    buf[outlen++] = *str; buf[outlen++] = *str;
	} else if (*str >= ' ' && *str <= '~' && *str != '\\') {
	    buf[outlen++] = *str;
	} else {
	    badchar = (*str & 0xFF);
	}
    }
}
