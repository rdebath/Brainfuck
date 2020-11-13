
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "bfi.tree.h"
#include "bfi.nasm.h"

static void print_asm_string(char * charmap, char * strbuf, int strsize);
static void print_asm_header(void);
static void print_asm_footer(void);
static void print_nasm_elf_hello_world(void);

#ifdef __APPLE__
/* Apple has old version of nasm so we must use maximums for sizes. */
#define oldnasm
#endif

#ifdef __ELF__
static int ulines = 0;
#else
static int ulines = 1;
#endif
static int outp_line;
static int lib_getch = 0, lib_putch = 0;

static int hello_world = 0;
static int qmagic = 0, omagic = 0;

/* loop_class: condition at 1=> end, 2=>start, 3=>both */
static int loop_class = 3;

/* Intel GAS translation and linkable objects. */
static int intel_gas = 0;
static void print_nasm_elf_header(void);
static void print_nasm_qmagic_header(void);
static void print_nasm_omagic_header(void);
static void print_nasm_header(void);
static void print_gas_header(void);

int
checkarg_nasm(char * opt, char * arg UNUSED)
{
    if (!strcmp(opt, "-fnasm")) { intel_gas = 0; return 1; }
    if (!strcmp(opt, "-qmagic")) { intel_gas = 0; qmagic = 1; return 1; }
    if (!strcmp(opt, "-omagic")) { intel_gas = 0; omagic = 1; return 1; }

    if (!strcmp(opt, "-fgas")) { intel_gas = 1; return 1; }
    if (!strcmp(opt, "-fwin32")) { intel_gas = 1; ulines = 1; return 1; }
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
    char charmap[256];
    char const * neartok = "";
    int i;
    int div_id = 0;

    memset(charmap, 0, sizeof(charmap));
    outp_line = -1;

    hello_world = (total_nodes == node_type_counts[T_CHR] + node_type_counts[T_STR] &&
		    !noheader && !qmagic && !omagic);

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
	n->iprof = i;
	switch(n->type) {
#ifndef oldnasm
	case T_MOV: i++; break;
	case T_ADD: i+=3; break;
	case T_SET: i+=3; break;
	case T_CALC: i+=6; break;
	case T_PRT: i+=9; break;
	case T_CHR: i+=9; break;
	case T_STR: i+=9*n->str->length; break;
	case T_INP: i+=9; break;
	case T_DIV: i+=24; break;
	default: i+=6; break;
#else
	case T_MOV: i+=6; break;
	case T_ADD: i+=6; break;
	case T_SET: i+=7; break;
	case T_CALC: i+=64; break;
	case T_PRT: i+=12; break;
	case T_CHR: i+=9; break;
	case T_STR: i+=9*n->str->length; break;
	case T_INP: i+=24; break;
	case T_DIV: i+=24; break;
	default: i+=12; break;
#endif
	}
    }

    n = bfprog;
    while(n)
    {

	if (enable_trace) {
	    if (n->line != 0 && n->line != outp_line && !intel_gas) {
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

	case T_CALCMULT:
	    if (n->count == 0 && n->count2 == 1 && n->count3 == 1) {
		printf("\tmovzx eax,byte ptr [ecx%s]\n", oft(n->offset2));
		printf("\tmovzx ebx,byte ptr [ecx%s]\n", oft(n->offset3));
		printf("\timul eax,ebx\n");
		printf("\tmov byte ptr [ecx%s],al\n", oft(n->offset));
		break;
	    }

	    fprintf(stderr, "Error on code generation:\n"
	           "Bad T_CALCMULT node incorrect counts.\n");
	    exit(99);

	    break;

	case T_LT:
	    if (n->count != 0 || n->count2 != 1 || n->count3 != 1)
                fprintf(stderr, "Warning: T_LT with bad counts.\n");

	    printf("\tmovzx ebx,byte ptr [ecx%s]\n", oft(n->offset2));
	    printf("\tmovzx edi,byte ptr [ecx%s]\n", oft(n->offset3));
	    printf("\tcmp ebx,edi\n");
	    printf("\tsetb al\n");
	    printf("\tadd byte ptr [ecx%s],al\n", oft(n->offset));
	    break;

	case T_DIV:
	    printf("\tmov byte ptr [ecx%s],0\n", oft(n->offset+3));
	    printf("\tmovzx eax,byte ptr [ecx%s]\n", oft(n->offset));
	    printf("\tmov byte ptr [ecx%s],al\n", oft(n->offset+2));

	    printf("\tmovzx ebx,byte ptr [ecx%s]\n", oft(n->offset+1));

	    printf("\tcmp dh,bl\n");
	    printf("\tjz zerodiv_%d\n", ++div_id);

	    printf("\tdiv bl\n");
	    printf("\tmov byte ptr [ecx%s],al\n", oft(n->offset+3)); /* Div */
	    printf("\tmov byte ptr [ecx%s],ah\n", oft(n->offset+2)); /* Mod */

	    printf("zerodiv_%d:\n", div_id);
	    printf("\txor edx,edx\n");
	    printf("\tinc edx\n");
	    break;

	case T_CHR:
	    {
		char buf[8];
		buf[0] = (char) /*GCC -Wconversion*/ n->count;
		print_asm_string(charmap, buf, 1);
	    }
	    break;

	case T_STR:
	    print_asm_string(charmap, n->str->buf, n->str->length);
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
	    lib_getch = 1;

	    printf("\tpush ecx\n");
	    if (n->offset != 0)
		printf("\tadd ecx,%d\n", n->offset);
	    printf("\tcall getch\n");
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
	    if (abs(n->iprof - n->jmp->iprof) > 120 && !intel_gas)
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
	    if (abs(n->iprof - n->jmp->iprof) > 120 && !intel_gas)
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
	    if (intel_gas)
		printf("\tjmp exit_prog\n");
	    else
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
	    fprintf(stderr, "Error on code generation:\n"
	           "Bad node: type %s: ptr+%d, cnt=%d.\n",
		    tokennames[n->type], n->offset, n->count);
	    exit(99);
	}
	n=n->next;
    }

    print_asm_footer();
}

static void
print_asm_header(void)
{
    if (intel_gas) {
	print_gas_header();
	return;
    }

    if (hello_world) {
	print_nasm_elf_hello_world();

	printf("\n\n; Below is the standard NASM code\n");
	printf("\n\n%%ifnidn __OUTPUT_FORMAT__, bin\n");
    }

    print_nasm_header();
    if (omagic)
	print_nasm_omagic_header();
    else if (qmagic)
	print_nasm_qmagic_header();
    else if (!hello_world)
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

    printf("%%ifidn __OUTPUT_FORMAT__, bin\n");
    printf("\n");

    printf("BITS 32\n");
    printf("\n");

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
	"\n"	"        dd      filesize                ;   p_memsz"
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
	"\n"	"filesize equ    section..bflast.start-orgaddr"
	"\n"	"putchbuf: resb 1"
	);

    if (most_neg_maad_loop<0)
	printf("\tresb %d\n", -most_neg_maad_loop);
    printf("memsize\tequ\t%d\n", memsize);
    printf("%s\n",
		"mem:"
	"\n"
	"\n"	"        section .bftext"
	"\n"	"_start:"
	"\n"	"        xor     eax, eax        ; EAX = 0 ;don't change high bits."
	"\n"	"        cdq                     ; EDX = 0 ;sign bit of EAX"
	"\n"	"        inc     edx             ; EDX = 1 ;ARG4 for system calls"
	"\n"	"        mov     ecx,mem"
	"\n"
	"\n");

    printf("%%endif	; bin format\n");
    printf("\n");
}

static void
print_nasm_qmagic_header(void)
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
    printf("; nasm %.*s.s && chmod +x %.*s\n",
            (int)(ep-np), np, (int)(ep-np), np);

    printf("%%ifidn __OUTPUT_FORMAT__, bin\n");
    printf("\n");
    if (most_neg_maad_loop<-2048)
	printf("memsize\tequ\t%d\n", memsize-most_neg_maad_loop);
    else
	printf("memsize\tequ\t%d\n", memsize);
    printf("\n");

    printf("%s\n",
		"; Beware this needs the a.out binformat loaded AND"
	"\n"	"; may need the the limit in /proc/sys/vm/mmap_min_addr"
	"\n"	"; to be no higher than 4096."
	"\n"	"BITS 32"
	"\n"
	"\n"	"orgaddr equ     0x0001000"
	"\n"	"        org     orgaddr"
	"\n"
	"\n"	"        dd      0x006400CC      ; a_info"
	"\n"	"        dd      a_text          ; a_text"
	"\n"	"        dd      a_data          ; a_data"
	"\n"	"        dd      a_bss           ; a_bss"
	"\n"	"        dd      0               ; a_syms"
	"\n"	"        dd      _start          ; a_entry"
	"\n"	"        dd      0               ; a_trsize"
	"\n"	"        dd      0               ; a_drsize"
	"\n"
	"\n"	"        section .text"
	"\n"	"        section .rodata"
	"\n"	"        section .textlib align=32"
	"\n"	"        section .bftext align=32"
	"\n"	"        section .bftail align=1"
	"\n"
	"\n"	"exit_prog:"
	"\n"	"        mov     bl, 0           ; Exit status"
	"\n"	"        xor     eax, eax"
	"\n"	"        inc     eax             ; syscall 1, exit"
	"\n"	"        int     0x80            ; exit(0)"
	"\n"
	"\n"	"        section .data align=4096"
	"\n"	"putchbuf:"
	"\n"	"        db      0xFF"
	"\n"
	"\n"	"datapad equ     4096-(($-$$)&4095)"
	"\n"	"a_text  equ     $$-orgaddr"
	"\n"	"a_data  equ     $-$$"
	"\n"	"a_bss   equ     memsize+datapad"
	"\n"	"        section .datapad align=1 nobits"
	"\n"	"        resb    datapad"
	"\n"
	"\n"	"        section .bss align=4096"
	"\n"	"mem:"
	"\n"	"        section .bftext"
	"\n"	"_start:"
	"\n"	"        xor     eax, eax        ; EAX = 0 ;don't change high bits."
	"\n"	"        cdq                     ; EDX = 0 ;sign bit of EAX"
	"\n"	"        inc     edx             ; EDX = 1 ;ARG4 for system calls"
	"\n"	"        mov     ecx,mem"
	);

    if (most_neg_maad_loop<-2048)
	printf("\tadd ecx,%d\n", -most_neg_maad_loop);

    printf("\n");

    printf("%%endif	; bin format\n");
    printf("\n");
}

static void
print_nasm_omagic_header(void)
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
    printf("; nasm %.*s.s && chmod +x %.*s\n",
            (int)(ep-np), np, (int)(ep-np), np);

    printf("%%ifidn __OUTPUT_FORMAT__, bin\n");
    printf("\n");
    if (most_neg_maad_loop<-2048)
	printf("memsize\tequ\t%d\n", memsize-most_neg_maad_loop);
    else
	printf("memsize\tequ\t%d\n", memsize);
    printf("\n");

    printf("%s\n",
		"; Beware this needs the a.out binformat loaded AND"
	"\n"	"; may need the the limit in /proc/sys/vm/mmap_min_addr"
	"\n"	"; to be zero. Also some Linux amd64 kernel versions"
	"\n"	"; will not run i386 OMAGIC executables correctly."
	"\n"	"BITS 32"
	"\n"
	"\n"	"orgaddr equ     -32"
	"\n"	"        org     orgaddr"
	"\n"
	"\n"	"        dd      0x00640107      ; a_info"
	"\n"	"        dd      a_text          ; a_text"
	"\n"	"        dd      a_data          ; a_data"
	"\n"	"        dd      a_bss           ; a_bss"
	"\n"	"        dd      0               ; a_syms"
	"\n"	"        dd      _start          ; a_entry"
	"\n"	"        dd      0               ; a_trsize"
	"\n"	"        dd      0               ; a_drsize"
	"\n"
	"\n"	"        section .text"
	"\n"	"        section .rodata"
	"\n"	"        section .textlib align=32"
	"\n"	"        section .bftext align=32"
	"\n"	"        section .bftail align=1"
	"\n"
	"\n"	"exit_prog:"
	"\n"	"        mov     bl, 0           ; Exit status"
	"\n"	"        xor     eax, eax"
	"\n"	"        inc     eax             ; syscall 1, exit"
	"\n"	"        int     0x80            ; exit(0)"
	"\n"
	"\n"	"        section .data align=1"
	"\n"	"putchbuf:"
	"\n"	"        db      0xFF"
	"\n"
	"\n"	"a_text  equ     $$"
	"\n"	"a_data  equ     $-$$"
	"\n"	"a_bss   equ     memsize"
	"\n"
	"\n"	"        section .bss align=1"
	"\n"	"mem:"
	"\n"	"        section .bftext"
	"\n"	"_start:"
	"\n"	"        xor     eax, eax        ; EAX = 0 ;don't change high bits."
	"\n"	"        cdq                     ; EDX = 0 ;sign bit of EAX"
	"\n"	"        inc     edx             ; EDX = 1 ;ARG4 for system calls"
	"\n"	"        mov     ecx,mem"
	);

    if (most_neg_maad_loop<-2048)
	printf("\tadd ecx,%d\n", -most_neg_maad_loop);
    printf("\n");

    printf("%%endif	; bin format\n");
    printf("\n");
}

static void
print_nasm_header(void)
{
    printf("; asmsyntax=nasm\n");
    printf("; This is to be compiled using nasm to produce an elf32 object file\n");
    printf(";   nasm -f elf32 bfprg.s\n");
    printf("; Or a windows object file\n");
    printf(";   nasm -f win32 bfprg.s\n");
    printf("; Which is then linked with the system C compiler.\n");
    printf(";   cc -m32 -s -o bfpgm bfpgm.o\n");
    printf(";\n");
    printf("%%define ptr\t\t; I'm putting these in for dumber assemblers.\n");
    printf("%%define .byte\tdb\t; Sigh\n");
    printf("\n");
    printf("%%ifnidn __OUTPUT_FORMAT__, bin\n");
    printf("\n");
    printf("\n");

    printf("; For unix underscores used --prefix _\n");
    printf("%%ifidn __OUTPUT_FORMAT__, win32\n");
    printf("%%define use_prefix 1\n");
    printf("%%endif\n");
    printf("%%ifidn __OUTPUT_FORMAT__, win\n");
    printf("%%define use_prefix 1\n");
    printf("%%endif\n");
    printf("%%ifidn __OUTPUT_FORMAT__, macho32\n");
    printf("%%define use_prefix 1\n");
    printf("%%endif\n");
    printf("%%ifidn __OUTPUT_FORMAT__, macho\n");
    printf("%%define use_prefix 1\n");
    printf("%%endif\n");
    printf("%%ifndef use_prefix\n");
    printf("%%define use_prefix 0\n");
    printf("%%endif\n");
    printf("%%if use_prefix+0\n");
    printf("  %%macro  cglobal 1 \n");
    printf("    global  _%%1 \n");
    printf("    %%define %%1 _%%1 \n");
    printf("  %%endmacro \n");
    printf("\n");
    printf("  %%macro  cextern 1 \n");
    printf("    extern  _%%1 \n");
    printf("    %%define %%1 _%%1 \n");
    printf("  %%endmacro\n");
    printf("%%else\n");
    printf("  %%define cglobal global\n");
    printf("  %%define cextern extern\n");
    printf("%%endif\n");
    printf("\n");

    printf("\tsection\t.bss\n");
    printf("putchbuf: resb 1\n");
    if (most_neg_maad_loop<0)
	printf("\tresb %d\n", -most_neg_maad_loop);
    printf("mem:\n");
    printf("\tresb %d\n", memsize);
    printf("\tsection\t.text\n");
    printf("\tcextern read\n");
    printf("\tcextern write\n");
    printf("\tcglobal main\n");
    printf("\n");

    printf("main:\n");
    printf("\tpush ebp\n");
    printf("\tmov ebp, esp\n");
    printf("\tpush ebx\n");
    printf("\tmov ecx, mem\n");
    printf("\txor edx,edx\n");
    if ( most_neg_maad_loop != 0)
	printf("\tadd ecx, %d\n", -most_neg_maad_loop);

    printf("%%endif	; !bin format\n");
}

static void
print_nasm_footer(void)
{
    printf("\n");
    printf("%%ifnidn __OUTPUT_FORMAT__, bin\n");
    printf("exit_prog:\n");
    printf("\tpop ebx\n");
    printf("\tleave\n");
    printf("\tret\n");
    printf("%%endif    ; !bin format\n");

    if (lib_getch) {
	printf("\n");
	printf("%%ifidn __OUTPUT_FORMAT__, bin\n");
	printf("section .textlib\n");
	printf("getch:\n");
	printf("\txor eax,eax\n");
	printf("\tmov ebx,eax\n");
	printf("\tcdq\n");
	printf("\tinc edx\n");
	printf("\tmov al, 3\n");
	printf("\tint 0x80\t; read(ebx, ecx, edx);\n");
	printf("\tret\n");
	printf("section .bftext\n");
	printf("%%endif    ; bin format\n");

	printf("%%ifnidn __OUTPUT_FORMAT__, bin\n");
	printf("getch:\n");
	printf("\tsub esp, 28\n");
	printf("\tmov dword ptr [esp+8], 1\n");
	printf("\tmov dword ptr [esp+4], ecx\n");
	printf("\tmov dword ptr [esp], 0\n");
	printf("\tcall read\n");
	printf("\tadd esp, 28\n");
	printf("\txor edx,edx\n");
	printf("\tret\n");
	printf("%%endif    ; !bin format\n");
    }

    if (lib_putch) {
	printf("%%ifidn __OUTPUT_FORMAT__, bin\n");
	if (enable_trace)
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
	printf("%%endif\n");

	printf("%%ifnidn __OUTPUT_FORMAT__, bin\n");
	printf("putch:\n");
	printf("\tsub esp, 32\n");
	printf("\tmov dword ptr [esp+16], ecx\n");
	printf("\tmov byte ptr [esp+12], al\n");
	printf("\tmov dword ptr [esp+8], 1\n");
	printf("\tlea eax, [esp+12]\n");
	printf("\tmov dword ptr [esp+4], eax\n");
	printf("\tmov dword ptr [esp], 1\n");
	printf("\tcall write\n");
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
	printf("\tcall write\n");
	printf("\tmov ecx, dword ptr [esp+12]\n");
	printf("\tadd esp, 32\n");
	printf("\txor edx,edx\n");
	printf("\tret\n");
	printf("%%endif\n");
    }

    if (hello_world && !intel_gas) {
	printf("%%endif ; Hello world code\n");
    }
}

static void
print_gas_header(void)
{
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

    printf(".globl %smain\n", ulines?"_":"");
    printf("%smain:\n", ulines?"_":"");
    printf("\tpush ebp\n");
    printf("\tmov ebp, esp\n");
    printf("\tpush ebx\n");
    printf("\tmov ecx, %smem\n", intel_gas?"offset flat:":"");
    printf("\txor edx,edx\n");
    if ( most_neg_maad_loop != 0)
	printf("\tadd ecx, %d\n", -most_neg_maad_loop);

    printf("\t.comm mem,%d,32\n", memsize - most_neg_maad_loop);
}

static void
print_gas_footer(void)
{
    printf("exit_prog:\n");
    printf("\tpop ebx\n");
    printf("\tleave\n");
    printf("\tret\n");

    if (lib_getch) {
	printf("getch:\n");
	printf("\tsub esp, 28\n");
	printf("\tmov dword ptr [esp+8], 1\n");
	printf("\tmov dword ptr [esp+4], ecx\n");
	printf("\tmov dword ptr [esp], 0\n");
	printf("\tcall %sread\n", ulines?"_":"");
	printf("\tadd esp, 28\n");
	printf("\txor edx,edx\n");
	printf("\tret\n");
    }

    if (lib_putch) {
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
    }
}

static void
print_asm_footer(void)
{
    if (intel_gas)
	print_gas_footer();
    else
	print_nasm_footer();
}

static void
print_asm_string(char * charmap, char * strbuf, int strsize)
{
static int textno = 0;
    int saved_line = outp_line;
    lib_putch = 1;

    if (strsize <= 0) return;

    if (strsize == 1 && !intel_gas) {
	int ch = *strbuf & 0xFF;
	int flg = (charmap[ch] == 0);
	charmap[ch] = 1;

	printf("%%ifidn __OUTPUT_FORMAT__, bin\n");

	if (flg) {
	    if (enable_trace && saved_line > 0)
		printf("%%line 1 lib_putch_%02x.s\n", ch);
	    printf("section .textlib\n");
	    printf("litprt_%02x:\n", ch);
	    printf("\tmov al,0x%02x\n", ch);
	    printf("\tjmp putch\n");
	    printf("section .bftext\n");
	    if (enable_trace && saved_line > 0)
		printf("%%line %d+0 %s\n", saved_line, bfname);
	    outp_line = saved_line;
	}
	printf("\tcall litprt_%02x\n", ch);

	printf("%%else\n");

	if (flg) {
	    printf("section .rodata\n");
	    printf("text_x%02x:\n", ch);
	    printf("\t.byte 0x%02x\n", ch);
	    printf("section .text\n");
	}
	printf("\tmov eax, %stext_x%02x\n", intel_gas?"offset flat:":"", ch);
	printf("\tmov edx,%d\n", strsize);
	printf("\tcall prttext\n");

	printf("%%endif\n");
    }
    else
    {
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
	else {
	    printf("%%ifnidn __OUTPUT_FORMAT__, bin\n");
	    printf("section .text\n");
	    printf("%%else\n");
	    printf("section .bftext\n");
	    printf("%%endif\n");
	}
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
print_nasm_elf_hello_world(void)
{
    struct bfi * n = bfprog;
    int bytecount = 0;

    printf("%s\n",
		"; asmsyntax=nasm"
	"\n"	"; nasm hello_world.s && chmod +x hello_world"
	"\n"
	"\n"	"%ifidn __OUTPUT_FORMAT__, bin"
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
    printf("msg:\n");

    {
	char txtbuf[80], *p=0;
	int l = 0, instrn = 0, i;
	for(n = bfprog; n; n=n->next) {
	    int need;
	    int ch = n->count & 0xFF;
	    char *bp, buf[8];
	    int ln;
	    if (n->type == T_CHR) { *(bp=buf)=ch; ln=1; }
	    else {bp = n->str->buf; ln = n->str->length; }

	    for(i=0; i<ln; i++)
	    {
		ch = bp[i];

		bytecount++;

		if (ch >= ' ' && ch <= '~' && ch != '\'') {
		    if (instrn) need = 2; else need = 5;
		} else if (ch == 10)
		    need = 4 + instrn;
		else
		    need = 6 + instrn;

		if (l+need>64) {
		    printf("%s\n", txtbuf);
		    l = 0;
		    instrn = 0;
		}

		if (l==0) {
		    strcpy(txtbuf, "\tdb\t");
		    p = txtbuf + strlen(txtbuf);
		}

		if (ch >= ' ' && ch <= '~' && ch != '\'') {
		    if (!instrn) {
			if (l) {
			    strcpy(p, ", '");
			    p+=3;
			    l+=3;
			} else {
			    strcpy(p, "'");
			    p++;
			    l++;
			}
			instrn = 1;
		    }
		    l++;
		    *p++ = ch;
		    *p = '\'';
		    p[1] = 0;
		} else {
		    if (instrn) { *p++ = '\''; l++; instrn=0; }
		    if (ch == 10) {
			if (l) {
			    sprintf(p, ", %d", ch);
			    p += 4;
			    l += 4;
			} else {
			    sprintf(p, "%d", ch);
			    p += 2;
			    l += 2;
			}
		    } else {
			if (l) {
			    sprintf(p, ", 0x%02x", ch);
			    p += 6;
			    l += 6;
			} else {
			    sprintf(p, "0x%02x", ch);
			    p += 4;
			    l += 4;
			}
		    }

		}
	    }
	}
	if (l>0) printf("%s\n", txtbuf);
    }

    printf("msgend:\n");
    if (total_nodes > 0) {
	printf("%%define msgsz\tmsgend-msg\n");
	printf("%%ifdef __YASM_MAJOR__\n");
	printf("%%define smallmsg\t0\t; Yasm requires a literal integer\n");
	printf("%%else\n");
	printf("%%define smallmsg\t(msgsz < 128)\n");
	printf("%%endif\n");

	printf("\tsection .text\n");
	printf("_start:\n");

	printf("\t; xor\teax,eax\t\t; Linux is already zero\n");
	printf("\t; xor\tebx,ebx\n");
	printf("\t; xor\tedx,edx\n\n");

	printf("\tmov\tal,4\t\t; syscall 4, write\n");
	printf("\tinc\tebx\n");
	printf("\tmov\tecx,msg\n");
	printf("%%if smallmsg\n");
	    printf("\tmov\tdl,msgsz\n");
	    printf("\tint\t0x80\t\t; write(ebx, ecx, edx);\n");
	    printf("\tmov\tal,1\n");
	printf("%%else\n");
	    printf("\tmov\tedx,msgsz\n");
	    printf("\tint\t0x80\t\t; write(ebx, ecx, edx);\n");
	    printf("\txor\teax,eax\n");
	    printf("\tinc\teax\n");
	printf("%%endif\n");
	printf("\tdec\tebx\n");
	printf("\tint\t0x80\t\t; exit(0)\n");
    } else {
	printf("\tsection .text\n");
	printf("_start:\n");
	printf("\tinc\teax\t\t; syscall 1, exit\n");
	printf("\tint\t0x80\t\t; exit(0)\n");
    }
    printf("%%endif\n");
    return;
}
