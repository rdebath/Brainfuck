#ifndef _BFI_CCODE_H
#define _BFI_CCODE_H
#define BE_CCODE

void run_ccode(void);
void print_ccode(FILE * ofd);
int checkarg_ccode(char * opt, char * arg);
#endif

#ifdef XX
#if !defined(DISABLE_TCCLIB) || !defined(DISABLE_DLOPEN)
X(ccode,CCODE,
    printf("   -c   Create C code. If combined with -r the code is loaded and run.\n");   ,
    case 'c': do_codestyle = c_ccode; break;			  ,
    case c_ccode: print_ccode(stdout); break;			    ,
    case c_ccode: run_ccode(); break;                               )

#if (XX == 4) && !defined(DISABLE_RUNC)
    if (do_run <= 0 && do_codestyle == c_default && verbose<3
	&& (cell_length <= UINTBIG_BIT)) {
	do_run = (do_run != 0);
	do_codestyle = c_ccode;
    }
#endif
#else
X(ccode,CCODE,
    printf("   -c   Create C code.\n");				    ,
    case 'c': do_codestyle = c_ccode; break;			    ,
    case c_ccode: print_ccode(stdout); break;			    ,
    Nothing_Here						    )
#endif

#if XX == 4
    if (do_codestyle == c_ccode &&
	    cell_length>UINTBIG_BIT && cell_length != INT_MAX) {
	fprintf(stderr, "The C generator does not support that cell size\n");
	exit(255);
    }
    if (do_codestyle == c_ccode) special_eof = 1;
#endif
#if XX == 9
    {	int f = checkarg_ccode(opt, arg);
	if (f) return f;
    }
#endif
#endif
