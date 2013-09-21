
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "bfi.tree.h"

#define SM(vx) (( ((int)(vx)) <<((sizeof(int)-1)*8))>>((sizeof(int)-1)*8))

void
print_asm_string(char * charmap, char * strbuf, int strsize)
{
static int textno = -1;
    int saved_line = curr_line;
    if (textno == -1) {
	textno++;

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
	printf("section .text\n");
    }

    if (strsize <= 0) return;
    if (strsize == 1) {
	int ch = *strbuf & 0xFF;
	if (charmap[ch] == 0) {
	    charmap[ch] = 1;
	    if (saved_line > 0)
		printf("%%line 1 lib_putch_%02x.s\n", ch);
	    printf("section .textlib\n");
	    printf("litprt_%02x:\n", ch);
	    printf("\tmov al,0x%02x\n", ch);
	    printf("\tjmp putch\n");
	    printf("section .text\n");
	    if (saved_line > 0)
		printf("%%line %d+0 %s\n", saved_line, curfile);
	    curr_line = saved_line;
	}
	printf("\tcall litprt_%02x\n", ch);
    } else {
	int i;
	textno++;
	if (saved_line != curr_line) {
	    if (saved_line > 0)
		printf("%%line %d+0 %s\n", saved_line, curfile);
	    curr_line = saved_line;
	}
	printf("section .data\n");
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
	printf("section .text\n");
	printf("\tmov eax,text_%d\n", textno);
	printf("\tmov edx,%d\n", strsize);
	printf("\tcall prttext\n");
    }
}

void 
print_asm()
{
    struct bfi * n = bfprog;
    char string_buffer[BUFSIZ], *sp = string_buffer;
    char charmap[256];
    char * neartok = "";
    int i;

    memset(charmap, 0, sizeof(charmap));
    curr_line = -1;

    if (verbose)
	fprintf(stderr, "Generating NASM Code.\n");

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
    eax = 0x000000xx, only AL is loaded on syscall.
    ebx = unknown, always reset.
    ecx = current pointer.
    edx = 1, dh used for loop cmp.

    Sigh ...
    Modern processors don't seem to have a true assembly language; this one
    is so full of old junk that it's impossible to optimise without full and
    complete documentation on the specific processor variant.

    In a real assembly language the only reason for there to be more limited
    versions of instructions is because they're better in some way. perhaps
    faster, perhaps they don't touch the CC register.

    This processor doesn't run x86 machine code; it translates it on the fly
    to it's real internal machine code.

    I need an assembler for *that* machine code, obviously to run it the 
    assembler will have to do a reverse translation to x86 machine code
    though.

    Perhaps, just programming to GNU lightning would be close enough ...
    it does seem that most of the changes I make to this code don't actually
    change performance at all.
*/

    if (!noheader) {
	calculate_stats();

	printf("; %s, asmsyntax=nasm\n", curfile);
	printf("; nasm -f bin -Ox brainfuck.asm ; chmod +x brainfuck\n");
	printf("\n");
	printf("BITS 32\n");
	printf("\n");
	printf("memsize\tequ\t0x10000\n");
	printf("orgaddr\tequ\t0x08048000\n");
	printf("\torg\torgaddr\n");
	printf("\n");
	printf("; A nice legal ELF header here, bit short, but that's okay.\n");
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
	printf("_start:\n");
	printf("\txor\teax, eax\t; EAX = 0 ;don't change high bits.\n");
	printf("\tcdq\t\t\t; EDX = 0 ;sign bit of EAX\n");
	printf("\tinc\tedx\t\t; EDX = 1 ;ARG4 for system calls\n");
	printf("\tmov\tecx,mem\n");
	if (node_type_counts[T_MOV] != 0)
	    printf("\talign 64\n");
	printf("\tsection\t.textlib align=64\n");
	printf("\tsection\t.text\n");
	printf("\n");
    }

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
	    printf("; "); printtreecell(stdout, 0, n); printf("\n");
	}

	if (n->line != 0 && n->line != curr_line && n->type != T_PRT) {
	    printf("%%line %d+0 %s\n", n->line, curfile);
	    curr_line = n->line;
	}

	switch(n->type)
	{
	case T_MOV:
#if 1
	    if (n->count == 1)
		printf("\tinc ecx\n");
	    else if (n->count == -1)
		printf("\tdec ecx\n");
	    else if (n->count == 2)
		printf("\tinc ecx\n" "\tinc ecx\n");
	    else if (n->count == -2)
		printf("\tdec ecx\n" "\tdec ecx\n");
	    else if (n->count < -128)
		printf("\tsub ecx,%d\n", -n->count);
	    else
#endif
	    printf("\tadd ecx,%d\n", n->count);
	    break;

	case T_ADD:
#if 1
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
	    else if (n->count < -128)
		printf("\tsub byte [ecx+%d],%d\n", n->offset, -n->count);
	    else
#endif
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
		printf("\tmov bl,byte [ecx+%d]\n", n->offset2);
		printf("\tmov byte [ecx+%d],bl\n", n->offset);
	    } else if (n->count2 == 1 && n->offset2 == n->offset) {
		/* m[1] = m[1]*1 + m[3]*n */
		if (n->count3 == -1) {
		    printf("\tmov al,byte [ecx+%d]\n", n->offset3);
		    printf("\tsub byte [ecx+%d],al\n", n->offset);
		} else if (n->count3 == 1) {
		    printf("\tmov al,byte [ecx+%d]\n", n->offset3);
		    printf("\tadd byte [ecx+%d],al\n", n->offset);
		} else {
		    printf("\tmov byte al,[ecx+%d]\n", n->offset3);
		    printf("\timul ebx,eax,%d\n", SM(n->count3));
		    printf("\tadd byte [ecx+%d],bl\n", n->offset);
		}
	    } else if (n->count2 == 1 && n->count3 == -1) {
		printf("\tmov al,byte [ecx+%d]\n", n->offset2);
		printf("\tsub al,byte [ecx+%d]\n", n->offset3);
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
		if (n->line != 0 && n->line != curr_line) {
		    printf("%%line %d+0 %s\n", n->line, curfile);
		    curr_line = n->line;
		}
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

	case T_ZFIND:
#if 0
	    if (n->next->count == 1) {
		printf("\tlea edi,[ecx+%d]\n",  n->offset);

		printf("\tcmp dh,byte [edi]\n");
		printf("\tjz end_%d\n", n->count);

		printf("\txor al,al\n");
		printf("\tmov ebx,ecx\n");

		printf("\txor ecx,ecx\n");
		printf("\tnot ecx\n");
		printf("\tcld\n");
		printf("\trepne	scasb\n");
		printf("\tnot ecx\n");

		printf("\tadd ecx,ebx\n");
		printf("\tdec ecx\n");
		printf("end_%d:\n", n->count);
		n = n->jmp;
	    } else
		goto While_loop_start;
	    break;

	While_loop_start:;
#endif

/* LoopClass: condition at 1=> end, 2=>start, 3=>both */
#define LoopClass 3

	case T_MULT: case T_CMULT:
	case T_MFIND:
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
	    break;

	case T_ENDIF:
	    printf("end_%d:\n", n->jmp->count);
	    break;

	case T_STOP: 
	    printf("\tcmp dh,byte [ecx+%d]\n", n->offset);
	    printf("\tjz near exit_prog\n");
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

    if (!noheader) {
	printf("\n");
	printf("exit_prog:\n");
	printf("\tmov\tbl, 0\t\t; Exit status\n");
	printf("\txor\teax, eax\n");
	printf("\tinc\teax\t\t; syscall(1, 0)\n");
	printf("\tint\t0x80\n");
	printf("\t;; EXIT ;;\n");
	printf("\n");
	printf("filesize equ\tsection..bss.start-orgaddr\n");
	printf("\tsection\t.bss align=4096\n");
	if (!hard_left_limit) 
	    printf("\tresb 4096\n");
	printf("mem:\n");
    }
}
