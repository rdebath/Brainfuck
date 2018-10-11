#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Fortran translation from BF, runs at about 1,000,000,000 instructions per second.
 */

static int ind = 0;
/* NB: f90 has a maximum line length */
#define I printf("%*s", (ind>40?40:ind)*2, "")

struct be_interface_s be_interface = {};

static void print_cstring(void);

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	printf("%s%d%s%d%s",
		"PROGRAM brainfuck\n"
		"  INTEGER, DIMENSION(0:",tapesz,") :: m = 0\n"
		"  INTEGER :: r, v, p = ", tapeinit, "\n"
		"  CHARACTER :: ch\n\n"
	    );
	ind ++;
	break;
    case '~':
	puts("END PROGRAM brainfuck");
	break;

    case '=': I; printf("m(p) = %d\n", count); break;
    case 'B':
	if(bytecell) { I; printf("m(p) = MOD(m(p), 256)\n"); }
	I; printf("v = m(p)\n");
	break;
    case 'M': I; printf("m(p) = m(p)+v*%d\n", count); break;
    case 'N': I; printf("m(p) = m(p)-v*%d\n", count); break;
    case 'S': I; printf("m(p) = m(p)+v\n"); break;
    case 'T': I; printf("m(p) = m(p)-v\n"); break;

    case 'X': I; printf("STOP 'Aborting Infinite Loop.'\n"); break;

    case '+': I; printf("m(p) = m(p) + %d\n", count); break;
    case '-': I; printf("m(p) = m(p) - %d\n", count); break;
    case '<': I; printf("p = p - %d\n", count); break;
    case '>': I; printf("p = p + %d\n", count); break;
    case '[':
	if(bytecell) { I; printf("m(p) = MOD(m(p), 256)\n"); }
	I; printf("DO WHILE (m(p) .NE. 0)\n");
	ind++;
	break;
    case ']':
	if(bytecell) { I; printf("m(p) = MOD(m(p), 256)\n"); }
	ind--; I; printf("END DO\n");
	break;
    case 'I':
	if(bytecell) { I; printf("m(p) = MOD(m(p), 256)\n"); }
	I; printf("IF (m(p) .NE. 0) THEN\n");
	ind++;
	break;
    case 'E':
	ind--; I; printf("END IF\n");
	break;
    case '.': I; printf("WRITE (*,'(A)',advance='no') CHAR(m(p))\n"); break;
    case '"': print_cstring(); break;
    case ',': I; printf("READ (*, '(A)',advance='no',iostat=r) ch\n");
              I; printf("IF (r .EQ. 0) THEN\n");
              I; printf("  m(p) = ICHAR(ch)\n");
              I; printf("ELSE IF (IS_IOSTAT_EOR(r)) THEN\n");
              I; printf("  m(p) = 10\n");
              I; printf("END IF\n");
	      break;
    }
}

static void
print_cstring(void)
{
    char * str = get_string();
    char buf[256];
    int badchar = 0;
    size_t outlen = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || badchar || outlen > sizeof(buf)-8))
	{
	    buf[outlen] = 0;
	    if (badchar == '\n') {
		I; printf("WRITE (*,'(A)') '%s'\n", buf);
		badchar = 0;
	    } else {
		I; printf("WRITE (*,'(A)',advance='no') '%s'\n", buf);
	    }
	    outlen = 0;
	}
	if (badchar) {
	    I; printf("WRITE (*,'(A)',advance='no') CHAR(%d)\n", badchar);
	    badchar = 0;
	}
	if (!*str) break;

	if (*str == '\'') {
	    buf[outlen++] = *str; buf[outlen++] = *str;
	} else if (*str >= ' ' && *str <= '~' && *str != '\\') {
	    buf[outlen++] = *str;
	} else {
	    badchar = (*str & 0xFF);
	}
    }
}
