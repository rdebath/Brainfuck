#include <stdio.h>
#include <string.h>

char * bf = "+-><[].,";

#ifdef __ELF__
#define _
#else
#define _ "_"
#endif

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_AMD64)

#ifndef __pic__
#ifndef __code_model_small__
char * header =
	".comm buffer,65536,64\n"
	".text\n"
	".globl "_"main\n"
	_"main:\n"
	"push %rbx\n"
	"movabsq $buffer, %rbx\n"
	;

char * footer =
	"pop %rbx\n"
        "ret\n"
	;

char * cc[] = {
	"incb (%rbx)",
	"decb (%rbx)",
	"inc %rbx",
	"dec %rbx",
	"movzbl (%%rbx), %%eax\ntest %%eax, %%eax\njz %df\n%d:\n",
	"movzbl (%%rbx), %%eax\ntest %%eax, %%eax\njnz %db\n%d:\n",
	"movzbl (%rbx), %edi\nmovabsq $"_"putchar, %rax\ncall *%rax\n",
	"movabsq $"_"getchar, %rax\ncall *%rax\ncmpl $-1, %eax\nje 1f\nmovb %al, (%rbx)\n1:"
};

#else

char * header =
	".comm buffer,65536,64\n"
	".text\n"
	".globl "_"main\n"
	_"main:\n"
	"push %rbx\n"
	"movl $buffer, %ebx\n"
	;

char * footer =
	"pop %rbx\n"
        "ret\n"
	;

char * cc[] = {
	"incb (%rbx)",
	"decb (%rbx)",
	"inc %rbx",
	"dec %rbx",
	"movzbl (%%rbx), %%eax\ntest %%eax, %%eax\njz %df\n%d:\n",
	"movzbl (%%rbx), %%eax\ntest %%eax, %%eax\njnz %db\n%d:\n",
	"movzbl (%rbx), %edi\ncall "_"putchar\n",
	"call "_"getchar\ncmpl $-1, %eax\nje 1f\nmovb %al, (%rbx)\n1:"
};

#endif
#else

char * header =
	".comm buffer,65536\n"
	".text\n"
	".globl "_"main\n"
	_"main:\n"
	"push %rbx\n"
	"movq buffer@GOTPCREL(%rip), %rbx\n"
	;

char * footer =
	"pop %rbx\n"
        "ret\n"
	;

char * cc[] = {
	"incb (%rbx)",
	"decb (%rbx)",
	"inc %rbx",
	"dec %rbx",
	"movzbl (%%rbx), %%eax\ntest %%eax, %%eax\njz %df\n%d:\n",
	"movzbl (%%rbx), %%eax\ntest %%eax, %%eax\njnz %db\n%d:\n",
	"movzbl (%rbx), %edi\ncallq "_"putchar\n",
	"callq "_"getchar\ncmpl $-1, %eax\nje 1f\nmovb %al, (%rbx)\n1:"
};

#endif

#elif defined(__i386__) || defined(_M_IX86)

char * header =
	".text\n"
	".globl "_"main\n"
	_"main:\n"
	"push %ebp\n"
	"mov %esp, %ebp\n"
	"mov $buffer, %ecx\n"
	;

char * cc[] = {
	"incb (%ecx)",
	"decb (%ecx)",
	"inc %ecx",
	"dec %ecx",
	"movzbl (%%ecx), %%eax\ntest %%eax, %%eax\njz %df\n%d:\n",
	"movzbl (%%ecx), %%eax\ntest %%eax, %%eax\njnz %db\n%d:\n",
	"movzbl (%ecx), %eax\npush %ecx\npush %eax\ncall "_"putchar\npop %ecx\npop %ecx",
	"push %ecx\ncall "_"getchar\npop %ecx\ncmpl $-1, %eax\nje 1f\nmovb %al, (%ecx)\n1:"
};

char * footer =
	"leave\n"
	"ret\n"
	".bss\n"
	".globl buffer\n"
	"buffer:\n"
	".space 0x10000\n"
	;

/* #elif ?? TODO Other */
#endif

int main(int argc, char ** argv) {
	char * s;
	int c;
	int ind=1;
	FILE * fd = argc>1?fopen(argv[1],"r"):stdin;
	if(!fd) { perror(argv[1]); return 1; }

	printf("%s", header);

	while((c=getc(fd)) != EOF)
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
