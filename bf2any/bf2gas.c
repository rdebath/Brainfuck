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
 *
 * Note: gcc -m64 -S -masm=intel -fno-asynchronous-unwind-tables example.c
 */

#if !defined(USE64)
#if !defined(__amd64__)
#define USE64	0
#else
#define USE64	1
#endif
#endif

/* Call params on stack
 * Callee saves: %ebp, %ebx, %esi, %edi
 * Caller saves: %eax, %ecx, and %edx
 */

#define AX32	"eax"
#define BX32	"ebx"
#define CX32	"ecx"
#define DX32	"edx"
#define BP32	"ebp"
#define SP32	"esp"
#define DI32	"edi"

/* Call params in registers: %rdi, %rsi, %rdx, %rcx, %r8 and %r9
 * MSWindows ONLY Call params in registers: %rcx, %rdx, %r8 and %r9
 * Callee saves: %rbp, %rbx and %r12 through %r15
 * Caller saves: %rax, %rcx, %rdx and the rest
 * Register vars: %ebx, %r12d, %r13d, %r14d, %r15d
 * Temps: %eax (set to zero for most FN calls)
 */

#define AX64	"eax"
#define BX64	"rbx"
#define CX64	"rcx"
#define DX64	"rdx"
#define BP64	"rbp"
#define SP64	"rsp"
#define DI64	"edi"

#define AX	(use_64bit?AX64:AX32)
#define BX	(use_64bit?BX64:BX32)
#define CX	(use_64bit?CX64:CX32)
#define DX	(use_64bit?DX64:DX32)
#define BP	(use_64bit?BP64:BP32)
#define SP	(use_64bit?SP64:SP32)
#define DI	(use_64bit?DI64:DI32)

static int ind = 0, text_labels = 1, use_64bit = USE64;
static struct stkdat { struct stkdat * up; int id; } *sp = 0;

static check_arg_t fn_check_arg;
static gen_code_t gen_code;
struct be_interface_s be_interface = {.bytesonly=1,.disable_be_optim=1,
    .check_arg=fn_check_arg, .gen_code=gen_code};

static int
fn_check_arg(const char * arg)
{
    if (strcmp(arg, "-64") == 0) { use_64bit = 1; return 1; }
    if (strcmp(arg, "-32") == 0) { use_64bit = 0; return 1; }
    if (strcmp(arg, "-static") == 0) { text_labels = 1; return 1; }
    if (strcmp(arg, "-local") == 0) { text_labels = 0; return 1; }

    if (strcmp("-h", arg) ==0) {
	fprintf(stderr, "%s\n",
	"\t"	"-64     x86 64 bit code"
	"\n\t"	"-32     x86 32 bit code"
	"\n\t"	"-static Use normal static labels"
	"\n\t"	"-local  Use numeric local labels"
	);
	return 1;
    }
    return 0;
}

static void
gen_code(int ch, int count, char * strn)
{
    switch(ch) {
    case '!':
	/* puts(".file \"brainfuck.b\""); */
	puts(".intel_syntax noprefix");
	puts(".text");
	puts(".globl main");
	puts("main:");
	printf("push %s\n", BP);
	printf("mov %s, %s\n", BP, SP);
	printf("push %s\n", BX);
	if (use_64bit)
	    printf("lea %s, buffer[rip]\n", BX);
	else
	    printf("mov %s, offset flat:buffer\n", BX);
	if (tapeinit)
	    printf("add %s, %d\n", BX, tapeinit);
	break;
    case '=': printf("mov byte ptr [%s], %d\n", BX, count & 0xFF); break;
    case '+': printf("add byte ptr [%s], %d\n", BX, count); break;
    case '-': printf("sub byte ptr [%s], %d\n", BX, count); break;
    case '<': printf("sub %s, %d\n", BX, count); break;
    case '>': printf("add %s, %d\n", BX, count); break;
    case '[':
	printf("movzx %s, byte ptr [%s]\n", AX, BX);
	printf("test %s, %s\n", AX, AX);
	if (text_labels)
	{
	    struct stkdat * n = malloc(sizeof*n);
	    n->up = sp;
	    sp = n;
	    n->id = ++ind;
	    printf("jz LE%d\n", n->id);
	    printf("LB%d:\n", n->id);
	}
	else
	{
	    printf("jz %df\n", ind*2+11);
	    printf("%d:\n", ind*2+10);
	    ind++;
	}
	break;
    case ']':
	printf("movzx %s, byte ptr [%s]\n", AX, BX);
	printf("test %s, %s\n", AX, AX);
	if (text_labels)
	{
	    struct stkdat * n = sp;
            sp = n->up;
	    printf("jnz LB%d\n", n->id);
	    printf("LE%d:\n", n->id);
            free(n);
	}
	else
	{
	    ind--;
	    printf("jnz %db\n", ind*2+10);
	    printf("%d:\n", ind*2+11);
	}
	break;
    case ',':
	if (use_64bit) {
	    printf("mov edx, 1\n");
	    printf("mov rsi, %s\n",BX);
	    printf("mov edi, 0\n");
	    printf("call read@PLT\n");
	} else {
	    printf("sub %s, 4\n", SP);
	    printf("push 1\n");
	    printf("push %s\n", BX);
	    printf("push 0\n");
	    printf("call read\n");
	    printf("add %s, 16\n", SP);
	}
	break;
    case '.':
	if (use_64bit) {
	    printf("mov edx, 1\n");
	    printf("mov rsi, %s\n", BX);
	    printf("mov edi, 1\n");
	    printf("call write@PLT\n");
	} else {
	    printf("sub %s, 4\n", SP);
	    printf("push 1\n");
	    printf("push %s\n", BX);
	    printf("push 1\n");
	    printf("call write\n");
	    printf("add %s, 16\n", SP);
	}
	break;
    case '~':
	printf("pop %s\n", BX);
	puts("leave");
	puts("ret");
	printf(".comm buffer,%d,32\n", tapesz);
	break;
    case '"':
	fprintf(stderr, "Fail cmd '\"' '%s'\n", strn);
	break;
    }
}
