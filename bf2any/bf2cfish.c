#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
 * BF translation to BF
 * This is a different way of translating to BF equlivents; the program will
 * also generate a set of C #define lines so the program is compilable as C.
 */

int col = 0;

char * bf = "<>+-,.[]";
char * fish[] = { "there", "once", "was", "a", "dead", "fish", "named", "Fred" };
char * c[] = { "m-=1;", "m+=1;", "++*m;", "--*m;", "read(0,m,1);", "write(1,m,1);", "while(*m){", "}" };

char * bc[] = { "l", "r", "u", "d", "i", "o", "b", "e" };
char * nice[] = { "left", "right", "up", "down", "in", "out", "begin", "end" };

char ** lang = fish;

int
check_arg(char * arg)
{
    if (strcmp(arg, "-c") == 0) {
	lang = bc; return 1;
    } else
    if (strcmp(arg, "-n") == 0) {
	lang = nice; return 1;
    } else
    if (strcmp(arg, "-f") == 0) {
	lang = fish; return 1;
    } else
	return 0;
}

void
outcmd(int ch, int count)
{
    char *p;
    while(count-->0){
	if (col>=72) {
	    putchar('\n');
	    col = 0;
	}
	if ((p=strchr(bf, ch)) != 0)
	    col+= printf("%s%s", col?" ":"", lang[p-bf]);
    }
    if (ch == '~') {
	printf("%s%s", col?" ":"", "_\n");
	col = 0;
    }
    if (ch == '!') {
	int i;
	printf("/*%s*/\n", lang[6]);
	printf("#include<unistd.h>\n");
	for (i=0; i<8; i++)
	    printf("#define %s %s\n", lang[i], c[i]);
	printf("#define _ return 0;}\n");
	printf("char mem[30000];int main(){char*m=mem;\n");
	printf("/*%s*/\n", lang[7]);
    }
}
