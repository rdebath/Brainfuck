#include <stdio.h>
int main (int argc, char *argv[]) {
    char *b=0, *p, t[65536]={0};
    int stk[999], sp = 0;
    unsigned short m=0;
    int i=0;
    FILE *fp=argc>1?fopen(argv[1], "r"):stdin;
    size_t r=0;
    if(!fp || getdelim(&b,&r,argc>1?'\0':'!',fp)<0)
	perror(argv[1]);
    else if(b&&r>0)for(p=b;*p;p++)switch(*p) {
	case '>': m++;break;
	case '<': m--;break;
	case '+': t[m]++;break;
	case '-': t[m]--;break;
	case '.': putchar(t[m]);break;
	case ',': {int c=getchar();if(c!=EOF)t[m]=c;}break;
	case '[': if(t[m]>1) stk[sp++] = p-b; break;
	case ']': if(sp>0) p = b+stk[--sp]-1; break;
    }
    return 0;
}
