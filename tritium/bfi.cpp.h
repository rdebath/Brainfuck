#ifndef _BFI_CPP_H
#define _BFI_CPP_H

void print_cpp(void);
#define BE_CPP
#endif

#ifdef XX
X(cpp,CPP,
    printf("   -C   Code for C or C++ with Boost cpp_int\n");,
    case 'C': case 'A': do_codestyle = c_cpp; break;		    ,
    case c_cpp: print_cpp(); break;                                   ,
    Nothing_Here                                                    )

#if XX == 4
    if (do_codestyle == c_cpp) {
        if (iostyle == 3) {
            fprintf(stderr, "The -C option does not support -fintio\n");
            exit(255);
        }
    }
#endif
#endif
