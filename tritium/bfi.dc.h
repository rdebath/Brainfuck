#ifndef _BFI_DC_H
#define _BFI_DC_H

void print_dc(void);
int checkarg_dc(char * opt, char * arg);
#define BE_DC
#endif

#ifdef XX
X(dc,DC,
    printf("   -D   Create code for dc(1).\n");
    printf("   -fv7 Enable Unix V7 dc(1) support.\n");              ,
    case 'D': do_codestyle = c_dc; break;                           ,
    case c_dc: print_dc(); break;                                   ,
    Nothing_Here						    )
#if XX == 4
    if (do_codestyle == c_dc && iostyle != 3) {
	if (eofcell == 0 && input_string == 0)
	    eofcell = 6;
	if (!default_io && iostyle != 2) {
	    fprintf(stderr,
		    "The -D option only supports integer or binary I/O.\n");
	    exit(255);
	}
	iostyle = 2;
    }
#endif
#if XX == 9
    {	int f = checkarg_dc(opt, arg);
	if (f) return f;
    }
#endif
#endif
