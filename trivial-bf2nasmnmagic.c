#include <stdio.h>
#include <string.h>


extern char * header;
char * bf = "+-><[].,";

/* These translations are the same as used by:
 * http://www.muppetlabs.com/~breadbox/software/tiny/bf.asm.txt
 */
char * cc[] = {
	"inc byte [ecx]",
	"dec byte [ecx]",
	"inc ecx",
	"dec ecx",
	"jmp near lbl_%d\nlbl_%d:\n",
	"lbl_%d:\ncmp dh, byte [ecx]\njnz near lbl_%d\n",
	"mov al, 4\nmov ebx, edx\nint 0x80",
	"mov al, 3\nxor ebx, ebx\nint 0x80"
};

int main(int argc, char ** argv) {
	char * s;
	int c;
	int loopid = 1;
	int sp=0, stk[999];

	printf("%s\n", header);

	while((c=getchar()) != EOF)
		if ((s = strchr(bf, c))) {
			if (c == '[') {
				printf(cc[s-bf], loopid+1, loopid);
				stk[sp++] = loopid;
				loopid += 2;
			} else if (c == ']') {
				if(sp) sp--;
				printf(cc[s-bf], stk[sp]+1, stk[sp]);
			} else
				printf("%s\n", cc[s-bf]);
		}
	return 0;
}

char * header =
	"; asmsyntax=nasm"
"\n"	"; nasm brainfuck.s && chmod +x brainfuck"
"\n"
"\n"	"BITS 32"
"\n"	"memsize equ     1048576"
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
;
