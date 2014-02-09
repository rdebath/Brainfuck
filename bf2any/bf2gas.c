#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * gas32 translation from BF, runs at about 4,800,000,000 instructions per second.
 * gas64 translation from BF, runs at about 4,600,000,000 instructions per second.
 *
 * Note: tcc generates slower assembler.
 *
 * Compile output with: gcc -o bfp bfp.s
 * Add -m32 or -m64 option if it's not your default target arch.
 */

int ind = 0;

#if !defined(USE32) && !defined(USE64) && defined(__i386__)
#define USE32
#endif

#ifdef USE32
// Call params on stack
// Callee saves: %ebp, %ebx, %esi, %edi
// Caller saves: %eax, %ecx, and %edx

#define AX	"%%eax"
#define CX	"%%ecx"
#define MOVZB	"movzbl"
#else
// Call params in registers: %rdi, %rsi, %rdx, %rcx, %r8 and %r9
// MSWindows ONLY Call params in registers: %rcx, %rdx, %r8 and %r9
// Callee saves: %rbp, %rbx and %r12 through %r15
// Caller saves: %rax, %rcx, %rdx and the rest
// Register vars: %ebx, %r12d, %r13d, %r14d, %r15d
// Temps: %eax (set to zero for most FN calls)

#define AX	"%%rax"
#define CX	"%%rcx"
#define MOVZB	"movzbq"
#endif

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-b") == 0) return 1;
    return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	printf(".text\n");
	printf(".globl main\n");
	printf("main:\n");
#ifdef USE32
	printf("push %%ebp\n");
	printf("mov %%esp, %%ebp\n");
	printf("mov $buffer, "CX"\n");
#else
	printf("push %%rbp\n");
	printf("mov %%rsp, %%rbp\n");
	printf("mov $buffer, "CX"\n");

#if 0
        pushq   %rbp
        movq    %rsp, %rbp
        pushq   %r15
        pushq   %r14
        pushq   %r13
        pushq   %r12
        pushq   %rbx
        subq    $24, %rsp
	...
        addq    $24, %rsp
        popq    %rbx
        popq    %r12
        popq    %r13
        popq    %r14
        popq    %r15
        popq    %rbp
        ret
#endif

#endif
	break;
    case '+': printf("addb $%d,("CX")\n", count); break;
    case '-': printf("subb $%d,("CX")\n", count); break;
    case '<': printf("sub $%d,"CX"\n", count); break;
    case '>': printf("add $%d,"CX"\n", count); break;
    case '[':
	printf(MOVZB" ("CX"), "AX"\n");
	printf("test "AX", "AX"\n");
	printf("jz %df\n", ind*2+1);
	printf("%d:\n", ind*2+2);
	ind++;
	break;
    case ']':
	ind--;
	printf(MOVZB" ("CX"), "AX"\n");
	printf("test "AX", "AX"\n");
	printf("jnz %db\n", ind*2+2);
	printf("%d:\n", ind*2+1);
	break;
    case ',':
	printf("push "CX"\n");
	printf("call getchar\n");
	printf("pop "CX"\n");
	printf("movb %%al, ("CX")\n");
	break;
    case '.':
	printf(MOVZB" ("CX"), "AX"\n");
	printf("push "CX"\n");
#ifdef USE32
	printf("push "AX"\n");
	printf("call putchar\n");
	printf("pop "CX"\n");
#else
	printf("movq "AX", %%rdi\n");
	printf("call putchar\n");
#endif
	printf("pop "CX"\n");
	break;
    case '~':
	printf("leave\n");
	printf("ret\n");
	printf(".bss\n");
	printf(".globl buffer\n");
	printf("buffer:\n");
	printf(".space 0x8000\n");
	break;
    }
}
