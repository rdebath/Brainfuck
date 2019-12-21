/*

    >*		Right in binary > is 1, * is 0
    <*		Left in binary < is 1, * is 0
    +*		Up in binary + is 1, * is 0
    -*		Down in binary - is 1, * is 0

    [		Begin
    ]		End
    ,		Input
    .		Output

    =		Set current cell to zero.

    { ??? }	Nested comments in braces.
    \n\s\r...	Whitespace is ignored.
 */

#include <stdio.h>
#include <stdlib.h>

void do_file(FILE *fd);

int
main(int argc, char** argv)
{
    int ar;
    if (argc == 1)
	do_file(stdin);
    else for(ar=1; ar<argc; ar++) {
	FILE * fd = fopen(argv[ar], "r");
	if (!fd) { perror("fopen"); break; }
	do_file(fd);
	fclose(fd);
    }
    return 0;
}

void
do_file(FILE *fd)
{
    int ch;
    int pch = 0;
    int cnest = 0, col = 0;
    double pcnt = 0;

    for(;;)
    {
	ch = getc(fd);
	if (ch == '{') cnest++;
	if (cnest && ch == '}') cnest--;
	if (cnest && ch == EOF) break;
	if (cnest || ch == '}') continue;
	if (ch == ' '  || ch == '\t' || ch == '\n') continue;
	if (ch == '\f' || ch == '\v' || ch == '\r') continue;
	if (pcnt && ch == '*') { pcnt *= 2; continue; }
	if (pch != ch && pcnt > 0) {
	    if (pcnt > 1)
		col += printf("%.0f%c", pcnt, pch);
	    else {
		col++; putchar(pch);
	    }
	    pch = pcnt = 0;
	    if (col > 70) {col=0; putchar('\n');}
	}
	if (ch == EOF) break;
	if (ch == '>' || ch == '<' || ch == '+' || ch == '-') {
	    pcnt = pcnt*2 + 1;
	    pch = ch;
	    continue;
	}
	if (ch == '.' || ch == ',' || ch == '[' || ch == ']')
	{
	    putchar(ch);
	    if (++col > 70) {col=0; putchar('\n');}
	    continue;
	}
	if (ch == '=') {
	    putchar('=');
	    if (++col > 70) {col=0; putchar('\n');}
	    continue;
	}
	fprintf(stderr, "Illegal character '%c' (%d)\n",
		ch>' '&&ch<='~'?ch:' ', ch);
	exit(1);
    }
    if (col) {col=0; putchar('\n');}
}
