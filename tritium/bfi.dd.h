
#ifndef _BFI_DD_H
#define _BFI_DD_H

void print_dd(void);
#define BE_DD

#endif

#ifdef XX
X(dd,DD,
    printf("   -d   Code for be-pipe of bf2any\n");,
    case 'd': do_codestyle = c_dd; break;			    ,
    case c_dd: print_dd(); break;                                   ,
    Nothing_Here                                                    )

#endif
