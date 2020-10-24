#ifndef _BFI_BF_H
#define _BFI_BF_H

void print_bf(void);
#define BE_BF
#endif

#ifdef XX
X(bf,BF,
    printf("   -F   Attempt to regenerate BF code. (This disables a lot of optimisations.)\n");,
    case 'F': do_codestyle = c_bf; break;                           ,
    case c_bf: print_bf(); break;                                   ,
    Nothing_Here						    )
#if XX == 4
    if (do_codestyle == c_bf) {
	opt_no_calc = opt_no_endif = opt_no_litprt = 1;
	opt_regen_mov = 0;
	hard_left_limit = 0;

	if (iostyle > 0) {
	    fprintf(stderr, "The -F option only supports binary I/O.\n");
	    exit(255);
	}
	iostyle = 0;
    }
#endif
#endif
