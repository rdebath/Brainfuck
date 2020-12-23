#include <stdio.h>
#include <string.h>

extern char * header;
extern char * cc[];
char * bf = "+-><[].,";

int main(int argc, char ** argv) {
	char * s;
	int c;
	int loopid = 1;
	int sp=0, stk[9999];
	FILE * fd = argc>1?fopen(argv[1],"r"):stdin;
	if(!fd) { perror(argv[1]); return 1; }

	printf("%s\n", header);

	while((c=getc(fd)) != EOF)
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

#if !defined(USE_AMD64) && !defined(USE_X86)
#if defined(__x86_64__) || defined(__amd64__) || defined(_M_AMD64)
#define USE_AMD64
#else
#define USE_X86
#endif
#endif

#ifndef USE_NMAGIC
#define USE_ELF
#endif

#ifdef USE_X86
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

#ifdef USE_ELF
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
#endif

#ifdef USE_NMAGIC
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
#endif
#endif

#ifdef USE_AMD64
char * cc[] = {
    "inc byte [r12]",
    "dec byte [r12]",
    "inc r12",
    "dec r12",
    "cmp byte [r12],bl\njz near lbl_%d\nlbl_%d:\n",
    "lbl_%d:\ncmp byte [r12],bl\njnz near lbl_%d\n",
    "xor eax,eax\ninc eax\nmov edi,eax\nmov rsi,r12\nmov edx,eax\nsyscall",
    "xor eax,eax\nxor edi,edi\nmov rsi,r12\nxor edx,edx\ninc edx\nsyscall"
};

#ifdef USE_ELF
char * header =
	";"
"\n"	"; Small, self-contained 64-bit ELF executable for NASM"
"\n"	"; compile with nasm -f bin -o a.out"
"\n"	";"
"\n"	"; Adapted from: http://www.muppetlabs.com/~breadbox/software/tiny/teensy.html"
"\n"	";"
"\n"	"; http://blog.markloiseau.com/2012/05/tiny-64-bit-elf-executables/"
"\n"
"\n"	"BITS 64"
"\n"	"memsize equ     1048576"
"\n"
"\n"	"orgaddr equ     0x0010000"
"\n"	"        org     orgaddr"
"\n"
"\n"	"; 64-bit ELF header"
"\n"	"ehdr:"
"\n"	"    ; ELF Magic, 2 -> 64-bit, 1 -> LSB, 1 -> ELF version, 0 -> ABI sysvish"
"\n"	"    db 0x7F, \"ELF\", 2, 1, 1, 0          ; e_ident"
"\n"	"    times 8 db 0        ; reserved for e_ident"
"\n"	"    "
"\n"	"    dw 2                ; e_type:       ET_EXEC"
"\n"	"    dw 62               ; e_machine:    EM_X86_64"
"\n"	"    dd 1                ; e_version"
"\n"	"    dq _start           ; e_entry"
"\n"	"    dq phdr - $$        ; e_phoff"
"\n"	"    dq 0                ; e_shoff"
"\n"	"    dd 0                ; e_flags"
"\n"	"    dw ehdrsize         ; e_ehsize"
"\n"	"    dw phdrsize         ; e_phentsize"
"\n"	"    dw 2                ; e_phnum"
"\n"	"    dw 0x40             ; e_shentsize"
"\n"	"    dw 0                ; e_shnum"
"\n"	"    dw 0                ; e_shstrndx"
"\n"
"\n"	"ehdrsize equ $ - ehdr"
"\n"
"\n"	"; 64-bit ELF program header"
"\n"	"phdr:"
"\n"	"    dd 1                ; p_type:               loadable segment"
"\n"	"    dd 5                ; p_flags               read, write and execute"
"\n"	"    dq 0                ; p_offset"
"\n"	"    dq $$               ; p_vaddr:              start of the current section"
"\n"	"    dq 0                ; p_paddr"
"\n"	"    dq filesize         ; p_filesz"
"\n"	"    dq ramsize          ; p_memsz"
"\n"	"    dq 0x001000         ; p_align"
"\n"
"\n"	"; program header size"
"\n"	"phdrsize equ $ - phdr"
"\n"
"\n"	"    dd      1                       ;   p_type"
"\n"	"    dd      6                       ;   p_flags"
"\n"	"    dq      0                       ;   p_offset"
"\n"	"    dq      section..bss.start      ;   p_vaddr"
"\n"	"    dq      0                       ;   p_paddr"
"\n"	"    dq      0                       ;   p_filesz"
"\n"	"    dq      memsize                 ;   p_memsz"
"\n"	"    dq      0x1000                  ;   p_align"
"\n"
"\n"	"    section .text"
"\n"	"    section .bftext align=64"
"\n"	"    section .bftail align=1"
"\n"
"\n"	"exit_prog:"
"\n"	"    ; sys_exit(return_code)"
"\n"	"    xor eax,eax"
"\n"	"    mov al,60"
"\n"	"    xor edi,edi"
"\n"	"    syscall"
"\n"
"\n"	"    section .bflast align=1"
"\n"	"    section .bss align=4096"
"\n"	"ramsize equ     section..bss.start-orgaddr"
"\n"	"filesize equ    section..bflast.start-orgaddr"
"\n"	"mem:"
"\n"
"\n"	"    section .bftext"
"\n"	"_start:"
"\n"	"    xor     eax, eax"
"\n"	"    xor     ebx, ebx"
"\n"	"    xor     ecx, ecx"
"\n"	"    mov     r12,mem"
"\n"
;
#endif
#endif
