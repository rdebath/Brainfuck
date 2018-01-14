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

#if defined(__FreeBSD__) && !defined(GLABELS)
#define GLABELS
#endif

#if !defined(USE32) && !defined(USE64) && defined(__i386__)
#define USE32
#endif

#ifdef USE32
/* Call params on stack
 * Callee saves: %ebp, %ebx, %esi, %edi
 * Caller saves: %eax, %ecx, and %edx
 */

#define AX	"eax"
#define BX	"ebx"
#define CX	"ecx"
#define DX	"edx"
#define BP	"ebp"
#define SP	"esp"
#else
/* Call params in registers: %rdi, %rsi, %rdx, %rcx, %r8 and %r9
 * MSWindows ONLY Call params in registers: %rcx, %rdx, %r8 and %r9
 * Callee saves: %rbp, %rbx and %r12 through %r15
 * Caller saves: %rax, %rcx, %rdx and the rest
 * Register vars: %ebx, %r12d, %r13d, %r14d, %r15d
 * Temps: %eax (set to zero for most FN calls)
 */

#define AX	"eax"
#define BX	"rbx"
#define CX	"rcx"
#define DX	"rdx"
#define BP	"rbp"
#define SP	"rsp"
#define DI	"edi"
#endif

#ifdef GLABELS
struct stkdat { struct stkdat * up; int id; } *sp = 0;
#endif

struct be_interface_s be_interface = {.bytesonly=1,.disable_be_optim=1};

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	/* puts(".file \"brainfuck.b\""); */
	puts(".intel_syntax noprefix");
	puts(".text");
	puts(".globl main");
	puts("main:");
	puts("push "BP);
	puts("mov "BP", "SP);
	puts("push "BX);
	puts("mov "BX", offset flat:buffer");
	if (tapeinit)
	    printf("add "BX", %d\n", tapeinit);
	break;
    case '=': printf("mov byte ptr ["BX"], %d\n", count & 0xFF); break;
    case '+': printf("add byte ptr ["BX"], %d\n", count); break;
    case '-': printf("sub byte ptr ["BX"], %d\n", count); break;
    case '<': printf("sub "BX", %d\n", count); break;
    case '>': printf("add "BX", %d\n", count); break;
    case '[':
	puts("movzx "AX", byte ptr ["BX"]");
	puts("test "AX", "AX);
#ifdef GLABELS
	{
	    struct stkdat * n = malloc(sizeof*n);
	    n->up = sp;
	    sp = n;
	    n->id = ++ind;
	    printf("jz LE%d\n", n->id);
	    printf("LB%d:\n", n->id);
	}
#else
	printf("jz %df\n", ind*2+11);
	printf("%d:\n", ind*2+10);
	ind++;
#endif
	break;
    case ']':
	puts("movzx "AX", byte ptr ["BX"]");
	puts("test "AX", "AX);
#ifdef GLABELS
	{
	    struct stkdat * n = sp;
            sp = n->up;
	    printf("jnz LB%d\n", n->id);
	    printf("LE%d:\n", n->id);
            free(n);
	}
#else
	ind--;
	printf("jnz %db\n", ind*2+10);
	printf("%d:\n", ind*2+11);
#endif
	break;
    case ',':
	puts("call getchar");
	puts("cmp "AX", -1");
	printf("je 1f\n");
	puts("mov byte ptr ["BX"], al");
	printf("1:\n");
	break;
    case '.':
	puts("movzx "AX", byte ptr ["BX"]");
#ifdef USE32
	puts("push "AX);
	puts("call putchar");
	puts("pop "AX);
#else
	puts("mov "DI", "AX);
	puts("call putchar");
#endif
	break;
    case '~':
	puts("pop "BX);
	puts("leave");
	puts("ret");
	printf(".comm buffer,%d,32\n", tapesz);
	break;
    }
}
