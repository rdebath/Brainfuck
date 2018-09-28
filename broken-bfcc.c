#include <stdio.h>
#include <string.h>

int main(int argc, char ** argv) {
	char * s;
	int c;
	int offset = 0;

	printf(	"#include <stdio.h>"
	"\n"	"char array[30000];"
	"\n"	"int main(int argc, char ** argv){"
	"\n"	"register int p = 0;"
	"\n"	"setbuf(stdout,0);"
	"\n");

	while((c=getchar()) != EOF)
	    switch(c) {
	    case '>': offset ++; break;
	    case '<': offset --; break;
	    case '+': printf("array[%d]++;\n", offset); break;
	    case '-': printf("array[%d]--;\n", offset); break;
	    case '.': printf("putchar(array[%d]);\n", offset); break;
	    case ',': printf("array[%d] = getchar();\n", offset); break;
	    case '[': printf("while(array[%d]){\n", offset); break;
	    case ']': printf("}\n", offset); break;
	    }

	printf("return 0;}\n");
	return 0;
}
