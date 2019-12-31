#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    int c, s, g=0, n=0;
    char *b = malloc(s=1024);
    FILE *fp=argc>1?fopen(argv[1], "r"):stdin;
    if(fp) while ((c=getc(fp)) != EOF && (c!='!' || argc>1 || !g)) {
	if (n+2>s && !(b = realloc(b, s += 1024))) break;
	g |= ((b[n++] = c) == ','); b[n] = 0;
    }
    if(!fp || !b) { perror(argv[1]); return 1;}
    argv[1] = b;



    char arr[65536] = { 0 };
    unsigned short m = 0;
    for (int i = 0, op; (op = argv[1][i]); i ++)
    switch (op) {
	case '>': m ++; break;
	case '<': m --; break;
	case '+': arr[m] ++; break;
	case '-': arr[m] --; break;
	case '.': putchar(arr[m]); break;
	case ',': arr[m] = getchar(); break;
	case ']':
	case '[':
	    if (!((op == '[' && arr[m]) || (op == ']' && !arr[m])))
		for (int m = 0; m || op == argv[1][i]; ) {
		    m = argv[1][i] == '[' ? m + 1 : m;
		    m = argv[1][i] == ']' ? m - 1 : m;
		    i += op == '[' ? 1 : -1;
		}
	    break;
    }
    return 0;
}
