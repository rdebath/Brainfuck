#include <stdio.h>
#include <string.h>


char * header =
	"; asmsyntax=nasm\n"
	"; nasm -f bin bf.s && chmod +x -\n"
	"\n"
	"BITS 32\n"
	"\n"
	"memsize\tequ\t0x10000\n"
	"orgaddr\tequ\t0x08048000\n"
	"\torg\torgaddr\n"
	"\n"
	"; A nice legal ELF header here, bit short, but that's okay.\n"
	"; I chose this as the smallest completely legal one from ...\n"
	"; http://www.muppetlabs.com/~breadbox/software/tiny/teensy.html\n"
	"\n"
	"ehdr:\t\t\t\t\t\t; Elf32_Ehdr\n"
	"\tdb\t0x7F, \"ELF\", 1, 1, 1, 0\t\t;   e_ident\n"
	"\ttimes 8 db\t0\n"
	"\tdw\t2\t\t\t\t;   e_type\n"
	"\tdw\t3\t\t\t\t;   e_machine\n"
	"\tdd\t1\t\t\t\t;   e_version\n"
	"\tdd\t_start\t\t\t\t;   e_entry\n"
	"\tdd\tphdr - $$\t\t\t;   e_phoff\n"
	"\tdd\t0\t\t\t\t;   e_shoff\n"
	"\tdd\t0\t\t\t\t;   e_flags\n"
	"\tdw\tehdrsize\t\t\t;   e_ehsize\n"
	"\tdw\tphdrsize\t\t\t;   e_phentsize\n"
	"\tdw\t1\t\t\t\t;   e_phnum\n"
	"\tdw\t0\t\t\t\t;   e_shentsize\n"
	"\tdw\t0\t\t\t\t;   e_shnum\n"
	"\tdw\t0\t\t\t\t;   e_shstrndx\n"
	"\n"
	"ehdrsize\tequ\t$ - ehdr\n"
	"\n"
	"phdr:\t\t\t\t\t\t; Elf32_Phdr\n"
	"\tdd\t1\t\t\t\t;   p_type\n"
	"\tdd\t0\t\t\t\t;   p_offset\n"
	"\tdd\t$$\t\t\t\t;   p_vaddr\n"
	"\tdd\t$$\t\t\t\t;   p_paddr\n"
	"\tdd\tfilesize\t\t\t;   p_filesz\n"
	"\tdd\tfilesize+memsize\t\t;   p_memsz\n"
	"\tdd\t7\t\t\t\t;   p_flags\n"
	"\tdd\t0x1000\t\t\t\t;   p_align\n"
	"\n"
	"phdrsize\tequ\t$ - phdr\n"
	"\n"
	"\tsection\t.text\n"
	"\tsection\t.data align=64\n"
	"\tsection\t.bss align=64\n"
	"filesize equ\tsection..bss.start-orgaddr\n"
	"tape:\n"
	"\n"
	"\tsection\t.text\n"
	"_start:\n"
	"xor eax, eax\n"
	"cdq\n"
	"inc edx\n"
	"mov ecx, tape\n"
	;

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

char * footer = "xchg eax, edx\nxor ebx, ebx\nint 0x80\n" ;

int main(int argc, char ** argv) {
	char * s;
	int c;
	int loopid = 1;
	int sp=0, stk[999];

	printf("%s", header);

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

	printf("%s", footer);
	return 0;
}
