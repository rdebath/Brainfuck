#ifndef _BFI_NASM_H
#define _BFI_NASM_H

void print_nasm(void);
int checkarg_nasm(char * opt, char * arg);
#define BE_NASM
#endif

#ifdef XX
X(nasm,NASM,
    printf("   -s   Create i386 Linux ELF NASM code.\n");           ,
    case 's': do_codestyle = c_nasm; break;                         ,
    case c_nasm: print_nasm(); break;                               ,
    Nothing_Here						    )
#if XX == 4
    if (do_codestyle == c_nasm)
    {
	if (cell_length && cell_size != 8) {
	    fprintf(stderr, "The 'nasm' generator only supports 8 bit cells.\n");
	    exit(255);
	}
	set_cell_size(8);
	if (iostyle > 0) {
	    fprintf(stderr, "The 'nasm' generator only supports binary I/O.\n");
	    exit(255);
	}
	iostyle = 0;
    }
#endif
#if XX == 6
if (cell_size == 0 && do_codestyle == c_nasm) set_cell_size(-1);
#endif
#if XX == 9
    {	int f = checkarg_nasm(opt, arg);
	if (f) return f;
    }
#endif
#endif
