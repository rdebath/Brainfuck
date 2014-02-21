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

	puts(	"#include <stdio.h>"
	"\n"	"int array[30000];"
	"\n"	"int main(void){"
	"\n"	"register int p = 0;"
	"\n"	"setbuf(stdout,0);");

	for(c=0; bf[c]; c++)
	    printf("#define %c %s\n", c+'A', cc[c]);

	while((c=getchar()) != EOF)
		if ((s = strchr(bf, c)))
			printf("%c\n", 'A'+(s-bf));

	puts("return 0;}");
	return 0;
}
