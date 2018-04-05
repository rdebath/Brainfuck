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
"\n"
"\n"	"memsize equ     0x100000"
"\n"	"orgaddr equ     0x08048000"
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
"\n"	"        section .text"
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
"\n"	"mem:"
"\n"
"\n"	"        section .bftext"
"\n"	"_start:"
"\n"	"        xor     eax, eax        ; EAX = 0 ;don't change high bits."
"\n"	"        cdq                     ; EDX = 0 ;sign bit of EAX"
"\n"	"        inc     edx             ; EDX = 1 ;ARG4 for system calls"
"\n"	"        mov     ecx,mem"
;
