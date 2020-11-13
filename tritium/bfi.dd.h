#ifndef _BFI_DD_H
#define _BFI_DD_H

void print_dd(void);
int checkarg_dd(char * opt, char * arg);
#define BE_DD
#endif

#ifdef XX
X(dd,DD,
    printf("   -d   Code for be-pipe of bf2any\n");
    printf("   -bfrle Generate an RLE variant of BF\n");,
    case 'd': do_codestyle = c_dd; break;			    ,
    case c_dd: print_dd(); break;                                   ,
    Nothing_Here                                                    )

#if XX == 4
    if (do_codestyle == c_dd) {
	opt_no_lessthan = 1;
	if (opt_regen_mov < 0) opt_regen_mov = 1;

        if (iostyle == 3) {
            fprintf(stderr, "The -d option does not support -fintio\n");
            exit(255);
        }
    }
#endif
#if XX == 9
    if (do_codestyle == c_default) {
	if (!strcmp(opt, "-bfrle")) {
	    (void) checkarg_dd(opt, arg);
	    do_codestyle = c_dd;
	    opt_no_calc = opt_no_endif = opt_no_litprt = 1;
	    return 1;
	}
    }
#endif

#endif
