
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "bfi.tree.h"
#include "bfi.nasm.h"

static void print_asm_string(char * charmap, char * strbuf, int strsize);
static void print_asm_header(void);
static void print_asm_footer(void);
static void print_hello_world(void);

#ifdef __APPLE__
/* Apple has old version of nasm so we must use maximums for sizes. */
#define oldnasm
#endif

#ifdef __linux__
static int link_main = 0;
#else
static int link_main = 1;
#endif
#ifdef __ELF__
static int ulines = 0;
#else
static int ulines = 1;
#endif
static int outp_line;

static int hello_world = 0;

/* loop_class: condition at 1=> end, 2=>start, 3=>both */
static int loop_class = 3;

/* Intel GAS translation and linkable objects. */
static int intel_gas = 0;
static void print_nasm_elf_header(void);
static void print_as_header(void);

int
checkarg_nasm(char * opt, char * arg UNUSED)
{
    if (!strcmp(opt, "-fgas")) { intel_gas = 1; link_main = 1; return 1; }
    if (!strcmp(opt, "-fnasm")) { intel_gas = 0; link_main = 1; return 1; }
    if (!strcmp(opt, "-flinux")) { intel_gas = 0; link_main = 0; return 1; }
    if (!strcmp(opt, "-fwin32")) { intel_gas = 1; link_main = 1; ulines = 1; return 1; }
    if (!strcmp(opt, "-fwin32n")) { intel_gas = 0; link_main = 1; ulines = 1; return 1; }
    if (!strcmp(opt, "-fuline")) { ulines = 1; return 1; }
    if (!strcmp(opt, "-fno-uline")) { ulines = 0; return 1; }
    return 0;
}

static char * oft(int i)
{
    static char buf[32];
    if (i < 0)
	sprintf(buf, "%d", i);
    else if (i == 0)
	*buf = 0;
    else
	sprintf(buf, "+%d", i);
    return buf;
}

void
print_nasm(void)
{
    struct bfi * n = bfprog;
    char string_buffer[BUFSIZ+2], *sp = string_buffer;
    char charmap[256];
    char const * neartok = "";
    int i;

    memset(charmap, 0, sizeof(charmap));
    outp_line = -1;

    hello_world = (total_nodes == node_type_counts[T_CHR] &&
		    !noheader && !link_main);

    if (hello_world) {
	print_hello_world();
	return;
    }

/* System calls used ...
    int 0x80: %eax is the syscall number; %ebx, %ecx, %edx, %esi, %edi and %ebp

    eax
    1	exit(ebx);		Used.
    2	fork(
    3	read(ebx,ecx,edx);	Used.
    4	write(ebx,ecx,edx);	Used.
    5	open(
    6	close(
    7 ...

    Register allocation.
    eax =
    ebx =
    ecx = current pointer.
    edx = 1, dh used for loop cmp.
    esi =
    edi =
    ebp =

    Intel order: AX, CX, DX, BX, SP, BP, SI, DI
                 AL, CL, DL, BL, AH, CH, DH, BH

    Sigh ...
    Modern processors don't seem to have a true assembly language; this one
    is so full of old junk that it's impossible to optimise without full and
    complete documentation on the specific processor variant.

    eg:
	http://www.agner.org/optimize/
	http://www.agner.org/optimize/optimizing_assembly.pdf
	http://www.agner.org/optimize/microarchitecture.pdf
	http://www.agner.org/optimize/instruction_tables.pdf

    In a real assembly language the only reason for there to be more limited
    versions of instructions is because they're better in some way. perhaps
    faster, perhaps they don't touch the CC register.

    This processor doesn't run x86 machine code; it translates it on the fly
    to it's real internal machine code.

    I need an assembler for *that* machine code, obviously to run it the
    assembler will have to do a reverse translation to x86 machine code
    though.

    Perhaps, just programming to GNU lightning would be close enough ...
    it does seem that most of the changes I can make to this code don't
    actually change performance at all.

    Though, it seems the ATOM is different, it isn't out of order and only,
    normally, does one instruction per clock. Using CISC instructions can
    be quite a bit faster.
*/

    if (!noheader)
	print_asm_header();

    /* Scan the nodes so we get an APPROXIMATE distance measure */
    for(i=0, n = bfprog; n; n=n->next) {
	n->ipos = i;
	switch(n->type) {
#ifndef oldnasm
	case T_MOV: i++; break;
	case T_ADD: i+=3; break;
	case T_SET: i+=3; break;
	case T_CALC: i+=6; break;
	case T_PRT: i+=9; break;
	case T_CHR: i+=9; break;
	case T_INP: i+=9; break;
	default: i+=6; break;
#else
	case T_MOV: i+=6; break;
	case T_ADD: i+=6; break;
	case T_SET: i+=7; break;
	case T_CALC: i+=64; break;
	case T_PRT: i+=12; break;
	case T_CHR: i+=9; break;
	case T_INP: i+=24; break;
	default: i+=12; break;
#endif
	}
    }

    n = bfprog;
    while(n)
    {
	if (sp != string_buffer) {
	    if ( n->type != T_CHR ||
		(sp >= string_buffer + sizeof(string_buffer) - 2)
	       ) {
		print_asm_string(charmap, string_buffer, sp-string_buffer);
		sp = string_buffer;
	    }
	}

	if (enable_trace) {
	    if (n->line != 0 && n->line != outp_line &&
		n->type != T_CHR && !intel_gas) {
		printf("%%line %d+0 %s\n", n->line, bfname);
		outp_line = n->line;
	    }

	    printf("%c ", intel_gas?'#':';');
	    printtreecell(stdout, 0, n); printf("\n");
	}

	switch(n->type)
	{
	case T_MOV:
	    /* INC & DEC modify part of the flags register.
	     * This can stall, but smaller seems to win. */
	    if (n->count == 1)
		printf("\tinc ecx\n");
	    else if (n->count == -1)
		printf("\tdec ecx\n");
	    else if (n->count == 2)
		printf("\tinc ecx\n" "\tinc ecx\n");
	    else if (n->count == -2)
		printf("\tdec ecx\n" "\tdec ecx\n");
	    else

	    if (n->count < 0 && n->count != -128)
		printf("\tsub ecx,%d\n", -n->count);
	    else
		printf("\tadd ecx,%d\n", n->count);
	    break;

	case T_ADD:
	    /* INC & DEC modify part of the flags register.
	     * This can stall, but smaller seems to win. */
	    if (n->count == 1 && n->offset == 0)
		printf("\tinc byte ptr [ecx]\n");
	    else if (n->count == -1 && n->offset == 0)
		printf("\tdec byte ptr [ecx]\n");
	    else if (n->count == 1)
		printf("\tinc byte ptr [ecx%s]\n", oft(n->offset));
	    else if (n->count == -1)
		printf("\tdec byte ptr [ecx%s]\n", oft(n->offset));
	    else

	    if (n->count < 0 && n->count > -128)
		printf("\tsub byte ptr [ecx%s],%d\n", oft(n->offset), -n->count);
	    else
		printf("\tadd byte ptr [ecx%s],%d\n", oft(n->offset), SM(n->count));
	    break;

	case T_SET:
	    printf("\tmov byte ptr [ecx%s],%d\n", oft(n->offset), SM(n->count));
	    break;

	case T_CALC:
	    if (n->count2 == 1 && n->count3 == 0) {
		/* m[1] = m[2]*1 + m[3]*0 */
		printf("\tmovzx eax,byte ptr [ecx%s]\n", oft(n->offset2));
		printf("\tmov byte ptr [ecx%s],al\n", oft(n->offset));
	    } else if (n->count2 == 1 && n->offset2 == n->offset) {
		/* m[1] = m[1]*1 + m[3]*n */
		if (n->count3 == -1) {
		    printf("\tmovzx eax,byte ptr [ecx%s]\n", oft(n->offset3));
		    printf("\tsub byte ptr [ecx%s],al\n", oft(n->offset));
		} else if (n->count3 == 1) {
		    printf("\tmovzx eax,byte ptr [ecx%s]\n", oft(n->offset3));
		    printf("\tadd byte ptr [ecx%s],al\n", oft(n->offset));
		} else if (n->count3 == 2) {
		    printf("\tmovzx eax,byte ptr [ecx%s]\n", oft(n->offset3));
		    printf("\tadd eax,eax\n");
		    printf("\tadd byte ptr [ecx%s],al\n", oft(n->offset));
		} else {
		    printf("\tmovzx eax,byte ptr [ecx%s]\n", oft(n->offset3));
		    printf("\timul eax,eax,%d\n", SM(n->count3));
		    printf("\tadd byte ptr [ecx%s],al\n", oft(n->offset));
		}
	    } else if (n->count2 == 1 && n->count3 == -1) {
		printf("\tmovzx eax,byte ptr [ecx%s]\n", oft(n->offset2));
		printf("\tmovzx ebx,byte ptr [ecx%s]\n", oft(n->offset3));
		printf("\tsub eax,ebx\n");
		printf("\tmov byte ptr [ecx%s],al\n", oft(n->offset));
	    } else {
		printf("\t%c Full T_CALC: [ecx+%d] = %d + [ecx+%d]*%d + [ecx+%d]*%d\n",
			intel_gas?'#':';',
			n->offset, n->count,
			n->offset2, n->count2,
			n->offset3, n->count3);
		if (UM(n->count2) == 0) {
		    printf("\tmov bl,0\n");
		} else if (n->count2 == -1 ) {
		    printf("\tmov bl,byte ptr [ecx%s]\n", oft(n->offset2));
		    printf("\tneg bl\n");
		} else {
		    printf("\tmov al,byte ptr [ecx%s]\n", oft(n->offset2));
		    printf("\timul ebx,eax,%d\n", SM(n->count2));
		}
		if (UM(n->count3) != 0) {
		    printf("\tmov al,byte ptr [ecx%s]\n", oft(n->offset3));
		    printf("\timul eax,eax,%d\n", SM(n->count3));
		    printf("\tadd bl,al\n");
		}
		printf("\tmov byte ptr [ecx%s],bl\n", oft(n->offset));
	    }

	    if (UM(n->count))
		printf("\tadd byte ptr [ecx%s],%d\n", oft(n->offset), SM(n->count));
	    break;

	case T_CHR:
	    *sp++ = (char) /*GCC -Wconversion*/ n->count;
	    break;

	case T_PRT:
	    print_asm_string(0,0,0);
	    if (enable_trace && !intel_gas && n->line != 0 && n->line != outp_line) {
		printf("%%line %d+0 %s\n", n->line, bfname);
		outp_line = n->line;
	    }
	    printf("\tmov al,[ecx%s]\n", oft(n->offset));
	    printf("\tcall putch\n");
	    break;

	case T_INP:

	    if (n->offset != 0 || link_main) {
		printf("\tpush ecx\n");
		printf("\tadd ecx,%d\n", n->offset);
	    }
	    if (!link_main) {
		printf("\txor eax,eax\n");
		printf("\tmov ebx,eax\n");
		printf("\tcdq\n");
		printf("\tinc edx\n");
		printf("\tmov al, 3\n");
		printf("\tint 0x80\t; read(ebx, ecx, edx);\n");
	    } else {
		printf("\tcall getch\n");
	    }
	    if (n->offset != 0 || link_main)
		printf("\tpop ecx\n");
	    break;

	case T_MULT: case T_CMULT: case T_IF:
	case T_WHL:

	    /* Need a "near" for jumps that we know are a long way away because
	     * nasm is VERY slow at working out which sort of jump to use
	     * without the hint. But we can't be sure exactly what number to
	     * put here without being a lot more detailed about the
	     * instructions we use so we don't force short jumps.
	     */
	    if (abs(n->ipos - n->jmp->ipos) > 120 && !intel_gas)
		neartok = " near";
	    else
		neartok = "";

	    if (n->type == T_IF || (loop_class & 2) == 2) {
		if ((loop_class & 1) == 0)
		    printf("start_%d:\n", n->count);
		printf("\tcmp dh,byte ptr [ecx%s]\n", oft(n->offset));
		printf("\tjz%s end_%d\n", neartok, n->count);
		if ((loop_class & 1) == 1 && n->type != T_IF)
		    printf("loop_%d:\n", n->count);
	    } else {
		printf("\tjmp%s last_%d\n", neartok, n->count);
		printf("loop_%d:\n", n->count);
	    }
	    break;

	case T_END:
	    if (abs(n->ipos - n->jmp->ipos) > 120 && !intel_gas)
		neartok = " near";
	    else
		neartok = "";

	    if (n->jmp->type == T_IF) {
		printf("end_%d:\n", n->jmp->count);
	    } else if ((loop_class & 1) == 0) {
		printf("\tjmp%s start_%d\n", neartok, n->jmp->count);
		printf("end_%d:\n", n->jmp->count);
	    } else {
		if ((loop_class & 2) == 0)
		    printf("last_%d:\n", n->jmp->count);
		printf("\tcmp dh,byte ptr [ecx%s]\n", oft(n->jmp->offset));
		printf("\tjnz%s loop_%d\n", neartok, n->jmp->count);
		if ((loop_class & 2) == 2)
		    printf("end_%d:\n", n->jmp->count);
	    }

	    if ((loop_class & 2) == 2)
		while(n->next && n->next->type == T_END && n->offset == n->next->offset) {
		    n=n->next;
		    printf("end_%d:\n", n->jmp->count);
		}
	    break;

	case T_ENDIF:
	    printf("end_%d:\n", n->jmp->count);
	    break;

	case T_STOP:
	    printf("\tjmp near exit_prog\n");
	    break;

	case T_NOP:
	case T_DUMP:
	    fprintf(stderr, "Warning on code generation: "
		    "%s node: ptr+%d, cnt=%d, @(%d,%d).\n",
		    tokennames[n->type],
		    n->offset, n->count, n->line, n->col);
	    break;

	default:
	    printf("; Bad node: type %d: ptr+%d, cnt=%d.\n",
		    n->type, n->offset, n->count);
	    fprintf(stderr, "Error on code generation:\n"
	           "Bad node: type %d: ptr+%d, cnt=%d.\n",
		    n->type, n->offset, n->count);
	    break;
	}
	n=n->next;
    }

    if(sp != string_buffer) {
	print_asm_string(charmap, string_buffer, sp-string_buffer);
    }

    if (link_main)
	print_asm_footer();
}

static void
print_asm_header(void)
{
    if (link_main)
	print_as_header();
    else
	print_nasm_elf_header();
}

static void
print_nasm_elf_header(void)
{
    char const *np =0, *ep;
    if (bfname && *bfname) {
	np = strrchr(bfname, '/');
	if (np) np++; else np = bfname;
    }
    if (!np || !*np) np = "brainfuck";
    ep = strrchr(np, '.');
    if (!ep || (strcmp(ep, ".bf") && strcmp(ep, ".b")))
	ep = np + strlen(np);

    printf("; asmsyntax=nasm\n");
    printf("; yasm %.*s.s && chmod +x %.*s\n",
	    (int)(ep-np), np, (int)(ep-np), np);
    printf("\n");
    printf("BITS 32\n");
    printf("\n");
    printf("memsize\tequ\t%d\n", memsize);

    printf("%s\n",
		"orgaddr equ     0x08048000"
	"\n"	"        org     orgaddr"
	"\n"
	"\n"	"; A nice legal ELF header here, bit short, but that's okay."
	"\n"	"; Based on the headers from ..."
	"\n"	"; http://www.muppetlabs.com/~breadbox/software/tiny/teensy.html"
	"\n"	"; but with RX and RW segments. See also ..."
	"\n"	"; http://blog.markloiseau.com/2012/05/tiny-64-bit-elf-executables"
	"\n"
	"\n"	"ehdr:                                   ; Elf32_Ehdr"
	"\n"	"        db      0x7F, \"ELF\", 1, 1, 1, 0 ;   e_ident"
	"\n"	"        times 8 db      0"
	"\n"	"        dw      2                       ;   e_type"
	"\n"	"        dw      3                       ;   e_machine"
	"\n"	"        dd      1                       ;   e_version"
	"\n"	"        dd      _start                  ;   e_entry"
	"\n"	"        dd      phdr - $$               ;   e_phoff"
	"\n"	"        dd      0                       ;   e_shoff"
	"\n"	"        dd      0                       ;   e_flags"
	"\n"	"        dw      ehdrsize                ;   e_ehsize"
	"\n"	"        dw      phdrsize                ;   e_phentsize"
	"\n"	"        dw      2                       ;   e_phnum"
	"\n"	"        dw      40                      ;   e_shentsize"
	"\n"	"        dw      0                       ;   e_shnum"
	"\n"	"        dw      0                       ;   e_shstrndx"
	"\n"
	"\n"	"ehdrsize        equ     $ - ehdr"
	"\n"
	"\n"	"phdr:                                   ; Elf32_Phdr"
	"\n"	"        dd      1                       ;   p_type"
	"\n"	"        dd      0                       ;   p_offset"
	"\n"	"        dd      $$                      ;   p_vaddr"
	"\n"	"        dd      0                       ;   p_paddr"
	"\n"	"        dd      filesize                ;   p_filesz"
	"\n"	"        dd      ramsize                 ;   p_memsz"
	"\n"	"        dd      5                       ;   p_flags"
	"\n"	"        dd      0x1000                  ;   p_align"
	"\n"
	"\n"	"phdrsize        equ     $ - phdr"
	"\n"
	"\n"	"        dd      1                       ;   p_type"
	"\n"	"        dd      0                       ;   p_offset"
	"\n"	"        dd      section..bss.start      ;   p_vaddr"
	"\n"	"        dd      0                       ;   p_paddr"
	"\n"	"        dd      0                       ;   p_filesz"
	"\n"	"        dd      memsize                 ;   p_memsz"
	"\n"	"        dd      6                       ;   p_flags"
	"\n"	"        dd      0x1000                  ;   p_align"
	"\n"
	"\n"	"; The program prolog, a few register inits and some NASM"
	"\n"	"; segments so I can put the library routines inline at their"
	"\n"	"; first use but have them collected at the start in the binary."
	"\n"
	"\n"	"        section .text"
	"\n"	"        section .rodata"
	"\n"	"        section .textlib align=64"
	"\n"	"        section .bftext align=64"
	"\n"	"        section .bftail align=1"
	"\n"
	"\n"	"exit_prog:"
	"\n"	"        mov     bl, 0           ; Exit status"
	"\n"	"        xor     eax, eax"
	"\n"	"        inc     eax             ; syscall 1, exit"
	"\n"	"        int     0x80            ; exit(0)"
	"\n"
	"\n"	"        section .bflast align=1"
	"\n"	"        section .bss align=4096"
	"\n"	"ramsize equ     section..bss.start-orgaddr"
	"\n"	"filesize equ    section..bflast.start-orgaddr"
	"\n"	"putchbuf: resb 1"
	);

    if (most_neg_maad_loop<0)
	printf("\tresb %d\n", -most_neg_maad_loop);
    printf("mem:\n");

    printf("\n");
    printf("\tsection\t.bftext\n");
    printf("_start:\n");
    printf("\txor\teax, eax\t; EAX = 0 ;don't change high bits.\n");
    printf("\tcdq\t\t\t; EDX = 0 ;sign bit of EAX\n");
    printf("\tinc\tedx\t\t; EDX = 1 ;ARG4 for system calls\n");
    printf("\tmov\tecx,mem\n");
    printf("\n");
    printf("%%define ptr\t\t; I'm putting these in for dumber assemblers.\n");
    printf("%%define .byte\tdb\t; Sigh\n");
    printf("\n");
}

static void
print_as_header(void)
{
    if (intel_gas) {
	printf("# This is for gcc's 'gas' assembler running in 'intel' mode\n");
	printf("# The code is 32bit so you may need a -m32\n");
	printf("# Assembler and link with libc using \"gcc -m32 -s -o bfpgm bfpgm.s\"\n");
	if (ulines) {
	    printf("# This uses underlines on the external variables so will probably\n");
	    printf("# only compile for Windows.\n");
	} else
	    printf("# Works on Linux, NetBSD and probably any other x86 OS with GCC.\n");
	printf("#\n");
	printf(".intel_syntax noprefix\n");
	printf(".text\n");
    } else {
	printf("; asmsyntax=nasm\n");
	printf("; This is to be compiled using nasm to produce an elf32 object file\n");
	printf(";   nasm -f elf32 bfprg.s\n");
	printf("; Which is then linked with the system C compiler.\n");
	printf(";   cc -m32 -s -o bfpgm bfpgm.o\n");
	printf(";\n");
	printf("%%define ptr\t\t; I'm putting these in for dumber assemblers.\n");
	printf("%%define .byte\tdb\t; Sigh\n");
	printf("\n");
	printf("\tsection\t.bss\n");
	printf("putchbuf: resb 1\n");
	if (most_neg_maad_loop<0)
	    printf("\tresb %d\n", -most_neg_maad_loop);
	printf("mem:\n");
	printf("\tresb %d\n", memsize);
	printf("\tsection\t.text\n");
	printf("\textern %sread\n", ulines?"_":"");
	printf("\textern %swrite\n", ulines?"_":"");
	printf("\tglobal %smain\n", ulines?"_":"");
	printf("\n");
    }

    printf("getch:\n");
    printf("\tsub esp, 28\n");
    printf("\tmov dword ptr [esp+8], 1\n");
    printf("\tmov dword ptr [esp+4], ecx\n");
    printf("\tmov dword ptr [esp], 0\n");
    printf("\tcall %sread\n", ulines?"_":"");
    printf("\tadd esp, 28\n");
    printf("\txor edx,edx\n");
    printf("\tret\n");

    printf("putch:\n");
    printf("\tsub esp, 32\n");
    printf("\tmov dword ptr [esp+16], ecx\n");
    printf("\tmov byte ptr [esp+12], al\n");
    printf("\tmov dword ptr [esp+8], 1\n");
    printf("\tlea eax, [esp+12]\n");
    printf("\tmov dword ptr [esp+4], eax\n");
    printf("\tmov dword ptr [esp], 1\n");
    printf("\tcall %swrite\n", ulines?"_":"");
    printf("\tmov ecx, dword ptr [esp+16]\n");
    printf("\tadd esp, 32\n");
    printf("\txor edx,edx\n");
    printf("\tret\n");

    printf("prttext:\n");
    printf("\tsub esp, 32\n");
    printf("\tmov dword ptr [esp+12], ecx\n");
    printf("\tmov dword ptr [esp+8], edx\n");
    printf("\tmov dword ptr [esp+4], eax\n");
    printf("\tmov dword ptr [esp], 1\n");
    printf("\tcall %swrite\n", ulines?"_":"");
    printf("\tmov ecx, dword ptr [esp+12]\n");
    printf("\tadd esp, 32\n");
    printf("\txor edx,edx\n");
    printf("\tret\n");

    if (intel_gas)
	printf(".globl %smain\n", ulines?"_":"");

    printf("%smain:\n", ulines?"_":"");
    printf("\tpush ebp\n");
    printf("\tmov ebp, esp\n");
    printf("\tpush ebx\n");
    printf("\tmov ecx, %smem\n", intel_gas?"offset flat:":"");
    printf("\txor edx,edx\n");
    if ( most_neg_maad_loop != 0)
	printf("\tadd ecx, %d\n", -most_neg_maad_loop);

    if (intel_gas)
	printf("\t.comm mem,%d,32\n", memsize - most_neg_maad_loop);
}

static void
print_asm_footer(void)
{
    printf("exit_prog:\n");
    printf("\tpop ebx\n");
    printf("\tleave\n");
    printf("\tret\n");
}

static void
print_asm_string(char * charmap, char * strbuf, int strsize)
{
static int textno = -1;
    int saved_line = outp_line;

    if (textno == -1 && link_main)
	textno ++;
    if (textno == -1) {
	textno++;

	if (enable_trace && !link_main)
	    printf("%%line 1 lib_string.s\n");
	outp_line = -1;
	printf("section .textlib\n");
	printf("putch:\n");
	printf("\tpush ecx\n");
	printf("\tmov ecx,putchbuf\n");
	printf("\tmov byte [ecx],al\n");
	printf("\txor eax, eax\n");
	printf("\tmov edx,1\n");
	printf("\tjmp syswrite\n");
	printf("prttext:\n");
	printf("\tpush ecx\n");
        printf("\tmov ecx,eax\n");
	printf("syswrite:\n");
        printf("\txor eax,eax\n");
        printf("\tmov ebx,eax\n");
        printf("\tinc ebx\n");
        printf("\tmov al, 4\n");
        printf("\tint 0x80\t; write(ebx, ecx, edx);\n");
	printf("\txor eax, eax\n");
	printf("\tcdq\n");
	printf("\tinc edx\n");
	printf("\tpop ecx\n");
        printf("\tret\n");
	printf("section .bftext\n");
    }

    if (strsize <= 0) return;
    if (strsize == 1 && !link_main) {
	int ch = *strbuf & 0xFF;
	if (charmap[ch] == 0) {
	    charmap[ch] = 1;
	    if (enable_trace && saved_line > 0)
		printf("%%line 1 lib_putch_%02x.s\n", ch);
	    printf("section .textlib\n");
	    printf("litprt_%02x:\n", ch);
	    printf("\tmov al,0x%02x\n", ch);
	    printf("\tjmp putch\n");
	    if (intel_gas)
		printf(".text\n");
	    else if (link_main)
		printf("section .text\n");
	    else
		printf("section .bftext\n");
	    if (enable_trace && saved_line > 0)
		printf("%%line %d+0 %s\n", saved_line, bfname);
	    outp_line = saved_line;
	}
	printf("\tcall litprt_%02x\n", ch);
    } else if (strsize == 1 && link_main) {
	int ch = *strbuf & 0xFF;
	if (charmap[ch] == 0) {
	    charmap[ch] = 1;
	    if (intel_gas)
		printf(".section .rodata\n");
	    else
		printf("section .rodata\n");
	    printf("text_x%02x:\n", ch);
	    printf("\t.byte 0x%02x\n", ch);
	    if (intel_gas)
		printf(".text\n");
	    else
		printf("section .text\n");
	}
	printf("\tmov eax, %stext_x%02x\n", intel_gas?"offset flat:":"", ch);
	printf("\tmov edx,%d\n", strsize);
	printf("\tcall prttext\n");
    } else {
	int i;
	textno++;
	if (intel_gas)
	    printf(".section .rodata\n");
	else {
	    if (saved_line != outp_line) {
		if (enable_trace && saved_line > 0)
		    printf("%%line %d+0 %s\n", saved_line, bfname);
		outp_line = saved_line;
	    }
	    printf("section .rodata\n");
	}
	printf("text_%d:\n", textno);
	for(i=0; i<strsize; i++) {
	    if (i%8 == 0) {
		if (i) printf("\n");
		printf("\t.byte ");
	    } else
		printf(", ");
	    printf("0x%02x", (strbuf[i] & 0xFF));
	}
	printf("\n");
	if (intel_gas)
	    printf(".text\n");
	else if (link_main)
	    printf("section .text\n");
	else
	    printf("section .bftext\n");
	printf("\tmov eax,%stext_%d\n", intel_gas?"offset flat:":"", textno);
	printf("\tmov edx,%d\n", strsize);
	printf("\tcall prttext\n");
    }
}

/*
 *  A special function for generating the NASM code for a very small routine
 *  to print a fixed string. For a short string it's 103 bytes plus the string.
 *
 *  If you go to the URL mentioned in the header, you'll see a 45 byte ELF
 *  program; I have avoided using that so that my ELF headers are still legal
 *  and non-overlapping.
 *
 *  On the other hand this does use the fact that a modern Linux will zero
 *  the registers at process startup. (Linux 2.0 and earlier won't work)
 */
static void
print_hello_world(void)
{
    struct bfi * n = bfprog;
    int i, bytecount = 0;

    if (link_main)
	print_as_header();
    else
	printf("%s\n",
		"; asmsyntax=nasm"
	"\n"	"; nasm hello_world.s && chmod +x hello_world"
	"\n"
	"\n"	"BITS 32"
	"\n"
	"\n"	"orgaddr equ     0x08048000"
	"\n"	"        org     orgaddr"
	"\n"
	"\n"	"; A nice legal ELF header here, bit short, but that's okay."
	"\n"	"; I chose this as the smallest completely legal one from ..."
	"\n"	"; http://www.muppetlabs.com/~breadbox/software/tiny/teensy.html"
	"\n"
	"\n"	"ehdr:                                   ; Elf32_Ehdr"
	"\n"	"        db      0x7F, \"ELF\", 1, 1, 1, 0 ;   e_ident"
	"\n"	"        times 8 db      0"
	"\n"	"        dw      2                       ;   e_type"
	"\n"	"        dw      3                       ;   e_machine"
	"\n"	"        dd      1                       ;   e_version"
	"\n"	"        dd      _start                  ;   e_entry"
	"\n"	"        dd      phdr - $$               ;   e_phoff"
	"\n"	"        dd      0                       ;   e_shoff"
	"\n"	"        dd      0                       ;   e_flags"
	"\n"	"        dw      ehdrsize                ;   e_ehsize"
	"\n"	"        dw      phdrsize                ;   e_phentsize"
	"\n"	"        dw      1                       ;   e_phnum"
	"\n"	"        dw      40                      ;   e_shentsize"
	"\n"	"        dw      0                       ;   e_shnum"
	"\n"	"        dw      0                       ;   e_shstrndx"
	"\n"
	"\n"	"ehdrsize        equ     $ - ehdr"
	"\n"
	"\n"	"phdr:                                   ; Elf32_Phdr"
	"\n"	"        dd      1                       ;   p_type"
	"\n"	"        dd      0                       ;   p_offset"
	"\n"	"        dd      $$                      ;   p_vaddr"
	"\n"	"        dd      0                       ;   p_paddr"
	"\n"	"        dd      filesize                ;   p_filesz"
	"\n"	"        dd      filesize                ;   p_memsz"
	"\n"	"        dd      5                       ;   p_flags"
	"\n"	"        dd      0x1000                  ;   p_align"
	"\n"
	"\n"	"phdrsize        equ     $ - phdr"
	"\n"
	"\n"	"; The program prolog."
	"\n"	"; This is a special version for a 'Hello World' program."
	"\n"	"filesize equ    msgend-orgaddr"
	);

    printf("\tsection\t.text\n");
    printf("\tsection\t.rodata align=1\n");
    for(n = bfprog; n; n=n->next) {
	if (n->next && (n->count & 0xFF) == '\n') {
	    printf("\tdb\t0x0a\n");
	    break;
	}
    }
    printf("msg:\n");
    for(i=0, n = bfprog; n; n=n->next) {
	if (i == 0) printf("\tdb\t"); else printf(", ");
	printf("0x%02x", n->count & 0xFF);
	bytecount ++;
	i = ((i+1) & 7);
	if (i == 0|| n->next == 0) printf("\n");
    }
    printf("msgend:\n");
    printf("%%ifdef __YASM_MAJOR__\n");
    printf("%%define msgsz\t%d\n", bytecount);
    printf("%%else\n");
    printf("%%define msgsz\tmsgend-msg\n");
    printf("%%endif\n");

    printf("\tsection .text\n");
    printf("_start:\n");

    if (total_nodes > 0) {

	/* printf("\txor\teax,eax\n");	Linux is zero */
	/* printf("\txor\tebx,ebx\n");	Linux is zero */
	/* printf("\txor\tedx,edx\n");	Linux is zero */

	printf("\tmov\tal,4\t\t; syscall 4, write\n");
	printf("\tinc\tebx\n");
	printf("\tmov\tecx,msg\n");
	printf("%%if msgsz < 128\n");
	    printf("\tmov\tdl,msgsz\n");
	printf("%%else\n");
	    printf("\tmov\tedx,msgsz\n");
	printf("%%endif\n");
	printf("\tint\t0x80\t\t; write(ebx, ecx, edx);\n");
	printf("\txor\teax,eax\n");
	printf("\txor\tebx,ebx\n");
    }
    printf("\tinc\teax\t\t; syscall 1, exit\n");
    printf("\tint\t0x80\t\t; exit(0)\n");
    return;
}
