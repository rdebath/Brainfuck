#include <stdio.h>
#include <string.h>

char * header =
	".text\n"
	".globl main\n"
	"main:\n"
	"push %ebp\n"
	"mov %esp, %ebp\n"
	"mov $buffer, %ecx\n"
	;

char * bf = "+-><[].,";
char * cc[] = {
	"incb (%ecx)",
	"decb (%ecx)",
	"inc %ecx",
	"dec %ecx",
	"movzbl (%%ecx), %%eax\ntest %%eax, %%eax\njz %df\n%d:\n",
	"movzbl (%%ecx), %%eax\ntest %%eax, %%eax\njnz %db\n%d:\n",
	"movzbl (%ecx), %eax\npush %ecx\npush %eax\ncall putchar\npop %ecx\npop %ecx",
	"push %ecx\ncall getchar\npop %ecx\ncmpl $-1, %eax\nje 1f\nmovb %al, (%ecx)\n1:"
};

char * footer =
	"leave\n"
	"ret\n"
	".bss\n"
	".globl buffer\n"
	"buffer:\n"
	".space 0x8000\n"
	;

int main(int argc, char ** argv) {
	char * s;
	int c;
	int ind=1;

	printf("%s", header);

	while((c=getchar()) != EOF)
		if ((s = strchr(bf, c))) {
			if (c == '[') {
				printf(cc[s-bf], ind+1, ind+2);
				ind+=2;
			} else if (c == ']') {
				if(ind) ind-=2;
				printf(cc[s-bf], ind+2, ind+1);
			} else
				printf("%s\n", cc[s-bf]);
		}

	printf("%s", footer);
	return 0;
}
