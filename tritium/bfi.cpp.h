#ifndef _BFI_CPP_H
#define _BFI_CPP_H

void print_cpp(void);
#define BE_CPP
#endif

#ifdef XX
X(cpp,CPP,
    printf("   -A   Code for C++ with Boost cpp_int\n");,
    case 'A': do_codestyle = c_cpp; break;			    ,
    case c_cpp: print_cpp(); break;                                   ,
    Nothing_Here                                                    )

#if XX == 4
    if (do_codestyle == c_cpp) {
        if (iostyle == 3) {
            fprintf(stderr, "The -A option does not support -fintio\n");
            exit(255);
        }
	opt_no_fullprt = 1;
    }
#endif
#endif
