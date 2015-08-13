#include <stdio.h>
#include <string.h>

char * bf = "+-><[].,";
char * cc[] = {
	"array[p]++;",
	"array[p]--;",
	"p++;",
	"p--;",
	"while(array[p]) {",
	"}",
	"putchar(array[p]);",
	"array[p]=getchar();"
};

int main(int argc, char ** argv) {
	char * s;
	int c;
	FILE * fd = argc>1?fopen(argv[1],"r"):stdin;

	puts(	"#include <stdio.h>"
	"\n"	"int array[30000];"
	"\n"	"int main(void){"
	"\n"	"register int p = 0;"
	"\n"	"setbuf(stdout,0);");

	while((c=getc(fd)) != EOF)
		if ((s = strchr(bf, c)))
			puts(cc[s-bf]);

	puts("return 0;}");
	return 0;
}
