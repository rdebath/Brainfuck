#include <stdio.h>
#include <string.h>

const char bf[] = "><+-.,[]";
const char tk[] = "rludoibex";
const char hd[] =	"#include<unistd.h>"
		"\n"	"#define b while(*m) {"
		"\n"	"#define i read(0,m,1);"
		"\n"	"#define o write(1,m,1);"
		"\n"	"#define d --*m;"
		"\n"	"#define u ++*m;"
		"\n"	"#define l m-=1;"
		"\n"	"#define r m+=1;"
		"\n"	"#define e }"
		"\n"	"#define _ return 0;}"
		"\n"	"char mem[1048576];int main(){register char*m=mem;";
int
main(int argc, char **argv)
{
    int cc = 0, ch;
    FILE * fd = argc>1?fopen(argv[1],"r"):stdin;
    if(!fd) { perror(argv[1]); return 1; }
    puts(hd);
    while((ch=getc(fd)) != EOF) {
	char * p = strchr(bf,ch);
	if (p) {
	    ch = p-bf;
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
