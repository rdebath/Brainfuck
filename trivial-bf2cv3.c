#include <stdio.h>
#include <string.h>

const char bf[] = "><+-.,[]";
const char tk[] = "rludoibex";

int
main(int argc, char **argv)
{
    int lastc = -1, cc = 0, ch;
    FILE * fd = argc>1?fopen(argv[1],"r"):stdin;
    if(!fd) { perror(argv[1]); return 1; }

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
    puts("char mem[30000];int main(){register char*m=mem");

    while((ch=getc(fd)) != EOF) {
	char * p = strchr(bf,ch);
	if (p) {
	    ch = p-bf;
	    if (ch < 4) {
		if (ch == lastc) ch = 8;
		else lastc = ch;
	    } else
		lastc = -1;

	    putchar(tk[ch]);
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
