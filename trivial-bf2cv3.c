#include <stdio.h>

int
main(void)
{
    int rle = 0, cc = 0, ch;
    puts("#include<unistd.h>");
    puts("#define r ;m+=1");
    puts("#define l ;m-=1");
    puts("#define u ;*m+=1");
    puts("#define d ;*m-=1");
    puts("#define o ;write(1,m,1)");
    puts("#define i ;read(0,m,1)");
    puts("#define b ;while(*m){");
    puts("#define e ;}");
    puts("#define x +1");
    puts("#define _ ;return 0;}");
    puts("char mem[30000];int main(){register char*m=mem;");

    while((ch = getchar()) != EOF) {
	int och = 0, rleok = 1;
	if (ch == '+') och = 'u';
	else if (ch == '-') och = 'd';
	else if (ch == '<') och = 'l';
	else if (ch == '>') och = 'r';
	else
	{
	    rleok = 0;
	    if (ch == '.') och = 'o';
	    else if (ch == ',') och = 'i';
	    else if (ch == '[') och = 'b';
	    else if (ch == ']') och = 'e';
	}
	if (och) {
	    if (rle == och) putchar('x');
	    else putchar(och);
	    if (rleok) rle = och; else rle = 0;
	    if (++cc == 38) {
		putchar('\n');
		cc = 0;
	    } else
		putchar(' ');
	}
    }
    putchar('_');
    putchar('\n');
    return 0;
}
