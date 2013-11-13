
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "bfi.tree.h"
#include "bfi.nasm.h"

static void print_asm_string(char * charmap, char * strbuf, int strsize);
static void print_asm_header(void);
static void print_hello_world(void);

static int hello_world = 0;

void
print_asm()
{
    struct bfi * n = bfprog;
    char string_buffer[BUFSIZ+2], *sp = string_buffer;
    char charmap[256];
    char * neartok = "";
    int i;

    memset(charmap, 0, sizeof(charmap));
    curr_line = -1;

    calculate_stats();
    hello_world = (total_nodes == node_type_counts[T_PRT] && !noheader);

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
	case T_MOV: i++; break;
	case T_ADD: i+=3; break;
	case T_SET: i+=3; break;
	case T_CALC: i+=6; break;
	case T_PRT: i+=9; break;
	case T_INP: i+=9; break;
	default: i+=6; break;
	}
    }

    n = bfprog;
    while(n)
    {
	if (sp != string_buffer) {
	    if ((n->type != T_SET && n->type != T_ADD && n->type != T_PRT) ||
		(n->type == T_PRT && n->count == -1) ||
		(sp >= string_buffer + sizeof(string_buffer) - 2)
	       ) {
		print_asm_string(charmap, string_buffer, sp-string_buffer);
		sp = string_buffer;
	    }
	}

	if (enable_trace) {
	    if (n->line != 0 && n->line != curr_line && n->type != T_PRT) {
		printf("%%line %d+0 %s\n", n->line, curfile);
		curr_line = n->line;
	    }

	    printf("; "); printtreecell(stdout, 0, n); printf("\n");
	}

	switch(n->type)
	{
	case T_MOV:
	    if (opt_level<0 && n->count == 1)
		printf("\tinc ecx\n");
	    else if (opt_level<0 && n->count == -1)
		printf("\tdec ecx\n");
	    else
#if 0
	    /* INC & DEC modify part of the flags register, this can stall. */
	    if (n->count == 1)
		printf("\tinc ecx\n");
	    else if (n->count == -1)
		printf("\tdec ecx\n");
	    else if (n->count == 2)
		printf("\tinc ecx\n" "\tinc ecx\n");
	    else if (n->count == -2)
		printf("\tdec ecx\n" "\tdec ecx\n");
	    else
#endif
	    if (n->count < -128)
		printf("\tsub ecx,%d\n", -n->count);
	    else
		printf("\tadd ecx,%d\n", n->count);
	    break;

	case T_ADD:
	    if (opt_level<0 && n->count == 1)
		printf("\tinc byte [ecx]\n");
	    else if (opt_level<0 && n->count == -1)
		printf("\tdec byte [ecx]\n");
	    else
#if 0
	    /* INC & DEC modify part of the flags register, this can stall. */
	    if (n->count == 1 && n->offset == 0)
		printf("\tinc byte [ecx]\n");
	    else if (n->count == -1 && n->offset == 0)
		printf("\tdec byte [ecx]\n");
	    else if (n->count < -128 && n->offset == 0)
		printf("\tsub byte [ecx],%d\n", -n->count);
	    else if (n->count == 1)
		printf("\tinc byte [ecx+%d]\n", n->offset);
	    else if (n->count == -1)
		printf("\tdec byte [ecx+%d]\n", n->offset);
	    else
#endif
	    if (n->count < -128)
		printf("\tsub byte [ecx+%d],%d\n", n->offset, -n->count);
	    else
		printf("\tadd byte [ecx+%d],%d\n", n->offset, SM(n->count));
	    break;

	case T_SET:
	    printf("\tmov byte [ecx+%d],%d\n", n->offset, SM(n->count));
	    break;

	case T_CALC:
	    if (0) {
		printf("; CALC [ecx+%d] = %d + [ecx+%d]*%d + [ecx+%d]*%d\n",
			n->offset, n->count,
			n->offset2, n->count2,
			n->offset3, n->count3);
	    }

	    if (n->count2 == 1 && n->count3 == 0) {
		/* m[1] = m[2]*1 + m[3]*0 */
		printf("\tmovzx eax,byte [ecx+%d]\n", n->offset2);
		printf("\tmov byte [ecx+%d],al\n", n->offset);
	    } else if (n->count2 == 1 && n->offset2 == n->offset) {
		/* m[1] = m[1]*1 + m[3]*n */
		if (n->count3 == -1) {
		    printf("\tmovzx eax,byte [ecx+%d]\n", n->offset3);
		    printf("\tsub byte [ecx+%d],al\n", n->offset);
		} else if (n->count3 == 1) {
		    printf("\tmovzx eax,byte [ecx+%d]\n", n->offset3);
		    printf("\tadd byte [ecx+%d],al\n", n->offset);
		} else if (n->count3 == 2) {
		    printf("\tmovzx eax,byte [ecx+%d]\n", n->offset3);
		    printf("\tadd eax,eax\n");
		    printf("\tadd byte [ecx+%d],al\n", n->offset);
		} else {
		    printf("\tmovzx eax,byte [ecx+%d]\n", n->offset3);
		    printf("\timul eax,eax,%d\n", SM(n->count3));
		    printf("\tadd byte [ecx+%d],al\n", n->offset);
		}
	    } else if (n->count2 == 1 && n->count3 == -1) {
		printf("\tmovzx eax,byte [ecx+%d]\n", n->offset2);
		printf("\tmovzx ebx,byte [ecx+%d]\n", n->offset3);
		printf("\tsub eax,ebx\n");
		printf("\tmov byte [ecx+%d],al\n", n->offset);
	    } else {
		printf("\t; Full T_CALC: [ecx+%d] = %d + [ecx+%d]*%d + [ecx+%d]*%d\n",
			n->offset, n->count,
			n->offset2, n->count2,
			n->offset3, n->count3);
		if (SM(n->count2) == 0) {
		    printf("\tmov bl,0\n");
		} else if (n->count2 == -1 ) {
		    printf("\tmov bl,byte [ecx+%d]\n", n->offset2);
		    printf("\tneg bl\n");
		} else {
		    printf("\tmov al,byte [ecx+%d]\n", n->offset2);
		    printf("\timul ebx,eax,%d\n", SM(n->count2));
		}
		if (SM(n->count3) != 0) {
		    printf("\tmov byte al,[ecx+%d]\n", n->offset3);
		    printf("\timul eax,eax,%d\n", SM(n->count3));
		    printf("\tadd bl,al\n");
		}
		printf("\tmov byte [ecx+%d],bl\n", n->offset);
	    }

	    if (SM(n->count))
		printf("\tadd byte [ecx+%d],%d\n", n->offset, SM(n->count));
	    break;

	case T_PRT:
	    if (n->count > -1) {
		*sp++ = n->count;
		break;
	    }

	    if (n->count == -1) {
		print_asm_string(0,0,0);
		if (enable_trace && n->line != 0 && n->line != curr_line) {
		    printf("%%line %d+0 %s\n", n->line, curfile);
		    curr_line = n->line;
		}
		if (n->offset == 0)
		    printf("\tmov al,[ecx]\n");
		else
		    printf("\tmov al,[ecx+%d]\n", n->offset);
		printf("\tcall putch\n");
	    } else {
		*string_buffer = n->count;
		print_asm_string(charmap, string_buffer, 1);
	    }
	    break;

	case T_INP:

	    if (n->offset != 0) {
		printf("\tpush ecx\n");
		printf("\tadd ecx,%d\n", n->offset);
	    }
	    printf("\txor eax,eax\n");
	    printf("\tmov ebx,eax\n");
	    printf("\tcdq\n");
	    printf("\tinc edx\n");
	    printf("\tmov al, 3\n");
	    printf("\tint 0x80\t; read(ebx, ecx, edx);\n");
	    if (n->offset != 0) {
		printf("\tpop ecx\n");
	    }
	    break;

/* LoopClass: condition at 1=> end, 2=>start, 3=>both */
#define LoopClass 3

	case T_MULT: case T_CMULT:
	case T_MFIND: case T_ZFIND:
	case T_ADDWZ: case T_IF: case T_FOR:
	case T_WHL:

	    /* Need a "near" for jumps that we know are a long way away because
	     * nasm is VERY slow at working out which sort of jump to use 
	     * without the hint. But we can't be sure exactly what number to
	     * put here without being a lot more detailed about the
	     * instructions we use so we don't force short jumps.
	     */
	    if (abs(n->ipos - n->jmp->ipos) > 120)
		neartok = " near";
	    else
		neartok = "";

	    if (n->type == T_IF || (LoopClass & 2) == 2) {
		if ((LoopClass & 1) == 0)
		    printf("start_%d:\n", n->count);
		printf("\tcmp dh,byte [ecx+%d]\n", n->offset);
		printf("\tjz%s end_%d\n", neartok, n->count);
		if ((LoopClass & 1) == 1 && n->type != T_IF)
		    printf("loop_%d:\n", n->count);
	    } else {
		printf("\tjmp%s last_%d\n", neartok, n->count);
		printf("loop_%d:\n", n->count);
	    }
	    break;

	case T_END:
	    if (abs(n->ipos - n->jmp->ipos) > 120)
		neartok = " near";
	    else
		neartok = "";

	    if (n->jmp->type == T_IF) {
		printf("end_%d:\n", n->jmp->count);
	    } else if ((LoopClass & 1) == 0) {
		printf("\tjmp%s start_%d\n", neartok, n->jmp->count);
		printf("end_%d:\n", n->jmp->count);
	    } else {
		if ((LoopClass & 2) == 0)
		    printf("last_%d:\n", n->jmp->count);
		printf("\tcmp dh,byte [ecx+%d]\n", n->jmp->offset);
		printf("\tjnz%s loop_%d\n", neartok, n->jmp->count);
		if ((LoopClass & 2) == 2)
		    printf("end_%d:\n", n->jmp->count);
	    }

	    if ((LoopClass & 2) == 2)
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
	    fprintf(stderr, "Warning on code generation: "
	           "NOP node: ptr+%d, cnt=%d, @(%d,%d).\n",
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
}

static void
print_asm_header(void)
{
    char *np =0, *ep;
    if (curfile && *curfile) {
	np = strrchr(curfile, '/');
	if (np) np++; else np = curfile;
    }
    if (!np || !*np) np = "brainfuck";
    ep = strrchr(np, '.');
    if (!ep || (strcmp(ep, ".bf") && strcmp(ep, ".b")))
	ep = np + strlen(np);

    printf("; asmsyntax=nasm\n");
    printf("; nasm -f bin -Ox %.*s.s && chmod +x %.*s\n",
	    (int)(ep-np), np, (int)(ep-np), np);
    printf("\n");
    printf("BITS 32\n");
    printf("\n");
    printf("memsize\tequ\t0x10000\n");
    printf("orgaddr\tequ\t0x08048000\n");
    printf("\torg\torgaddr\n");
    printf("\n");
    printf("; A nice legal ELF header here, bit short, but that's okay.\n");
    printf("; I chose this as the smallest completely legal one from ...\n");
    printf("; http://www.muppetlabs.com/~breadbox/software/tiny/teensy.html\n");
    printf("; See also ...\n");
    printf("; http://blog.markloiseau.com/2012/05/tiny-64-bit-elf-executables\n");
    printf("\n");
    printf("ehdr:\t\t\t\t\t\t; Elf32_Ehdr\n");
    printf("\tdb\t0x7F, \"ELF\", 1, 1, 1, 0\t\t;   e_ident\n");
    printf("\ttimes 8 db\t0\n");
    printf("\tdw\t2\t\t\t\t;   e_type\n");
    printf("\tdw\t3\t\t\t\t;   e_machine\n");
    printf("\tdd\t1\t\t\t\t;   e_version\n");
    printf("\tdd\t_start\t\t\t\t;   e_entry\n");
    printf("\tdd\tphdr - $$\t\t\t;   e_phoff\n");
    printf("\tdd\t0\t\t\t\t;   e_shoff\n");
    printf("\tdd\t0\t\t\t\t;   e_flags\n");
    printf("\tdw\tehdrsize\t\t\t;   e_ehsize\n");
    printf("\tdw\tphdrsize\t\t\t;   e_phentsize\n");
    printf("\tdw\t1\t\t\t\t;   e_phnum\n");
    printf("\tdw\t0\t\t\t\t;   e_shentsize\n");
    printf("\tdw\t0\t\t\t\t;   e_shnum\n");
    printf("\tdw\t0\t\t\t\t;   e_shstrndx\n");
    printf("\n");
    printf("ehdrsize      equ     $ - ehdr\n");
    printf("\n");
    printf("phdr:\t\t\t\t\t\t; Elf32_Phdr\n");
    printf("\tdd\t1\t\t\t\t;   p_type\n");
    printf("\tdd\t0\t\t\t\t;   p_offset\n");
    printf("\tdd\t$$\t\t\t\t;   p_vaddr\n");
    printf("\tdd\t$$\t\t\t\t;   p_paddr\n");
    printf("\tdd\tfilesize\t\t\t;   p_filesz\n");
    printf("\tdd\tfilesize+memsize\t\t;   p_memsz\n");
    printf("\tdd\t7\t\t\t\t;   p_flags\n");
    printf("\tdd\t0x1000\t\t\t\t;   p_align\n");
    printf("\n");
    printf("phdrsize      equ     $ - phdr\n");
    printf("\n");

    if (hello_world) {
	printf("filesize equ\tsection..bss.start-orgaddr\n");
	printf("\tsection\t.text\n");
	printf("\tsection\t.data\n");
	printf("\tsection\t.bss\n");
	printf("\tsection\t.text\n");
	printf("_start:\n");
	return;
    }

    printf("; The program prolog, a few register inits and some NASM\n");
    printf("; segments so I can put the library routines inline at their\n");
    printf("; first use but have them collected at the start in the binary.\n");
    printf("\n");
    printf("\tsection\t.text\n");
    printf("\tsection\t.rodata\n");
    printf("\tsection\t.textlib align=64\n");
    printf("\tsection\t.bftext align=64\n");
    printf("\tsection\t.bftail align=1\n");
    printf("\n");
    printf("exit_prog:\n");
    printf("\tmov\tbl, 0\t\t; Exit status\n");
    printf("\txor\teax, eax\n");
    printf("\tinc\teax\t\t; syscall 1, exit\n");
    printf("\tint\t0x80\t\t; exit(0)\n");
    printf("\n");
    printf("\tsection\t.data align=64\n");
    printf("\tsection\t.bss align=4096\n");
    printf("filesize equ\tsection..bss.start-orgaddr\n");
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
}

static void
print_asm_string(char * charmap, char * strbuf, int strsize)
{
static int textno = -1;
    int saved_line = curr_line;
    if (textno == -1) {
	textno++;

	if (enable_trace)
	    printf("%%line 1 lib_string.s\n");
	curr_line = -1;
	printf("section .data\n");
	printf("putchbuf: db 0\n");
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
    if (strsize == 1) {
	int ch = *strbuf & 0xFF;
	if (charmap[ch] == 0) {
	    charmap[ch] = 1;
	    if (enable_trace && saved_line > 0)
		printf("%%line 1 lib_putch_%02x.s\n", ch);
	    printf("section .textlib\n");
	    printf("litprt_%02x:\n", ch);
	    printf("\tmov al,0x%02x\n", ch);
	    printf("\tjmp putch\n");
	    printf("section .bftext\n");
	    if (enable_trace && saved_line > 0)
		printf("%%line %d+0 %s\n", saved_line, curfile);
	    curr_line = saved_line;
	}
	printf("\tcall litprt_%02x\n", ch);
    } else {
	int i;
	textno++;
	if (saved_line != curr_line) {
	    if (enable_trace && saved_line > 0)
		printf("%%line %d+0 %s\n", saved_line, curfile);
	    curr_line = saved_line;
	}
	printf("section .rodata\n");
	printf("text_%d:\n", textno);
	for(i=0; i<strsize; i++) {
	    if (i%8 == 0) {
		if (i) printf("\n");
		printf("\tdb ");
	    } else
		printf(", ");
	    printf("0x%02x", (strbuf[i] & 0xFF));
	}
	printf("\n");
	printf("section .bftext\n");
	printf("\tmov eax,text_%d\n", textno);
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
    int i;

    print_asm_header();
    if (total_nodes > 0) {
	/* printf("\txor\teax,eax\n");	Linux is zero */
	/* printf("\txor\tebx,ebx\n");	Linux is zero */
	printf("\tmov\tal,4\t\t; syscall 4, write\n");
	printf("\tinc\tebx\n");
	printf("\tmov\tecx,msg\n");
	if (total_nodes < 128) {
	    /* printf("\tmov\tedx,eax\n");	Linux is zero */
	    printf("\tmov\tdl,%d\n", total_nodes);
	} else {
	    printf("\tmov\tedx,%d\n", total_nodes);
	}
	printf("\tint\t0x80\t\t; write(ebx, ecx, edx);\n");
	printf("\txor\teax,eax\n");
	printf("\txor\tebx,ebx\n");
    }
    printf("\tinc\teax\t\t; syscall 1, exit\n");
    printf("\tint\t0x80\t\t; exit(0)\n");
    printf("msg:\n");
    for(i=0, n = bfprog; n; n=n->next) {
	if (i == 0) printf("\tdb\t"); else printf(", ");
	printf("0x%02x", n->count & 0xFF);
	i = ((i+1) & 7);
	if (i == 0|| n->next == 0) printf("\n");
    }
    return;
}
