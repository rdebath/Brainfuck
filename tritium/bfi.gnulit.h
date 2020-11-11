#ifndef DISABLE_GNULIGHTNING

#ifndef _BFI_GNULIGHTNING_H
#define _BFI_GNULIGHTNING_H

void run_gnulightning(void);
extern int gnulightning_ok;
#define BE_GNULIGHTNING
#endif

#ifdef XX
X(gnulightning,GNULIGHTNING,
    printf("   -j   Run using the GNU lightning interpreter/assembler.\n");    ,
    case 'j': do_codestyle = c_gnulightning; break;                ,
    /* Cannot do print_gnulightning() */                                     ,
    case c_gnulightning: run_gnulightning(); break;                               )
#if XX == 4
    if (do_run == -1 && do_codestyle == c_default &&
	    (cell_length==0 || cell_size>0) &&
	    verbose<3 && !enable_trace && !debug_mode && gnulightning_ok) {
	do_run = 1;
	do_codestyle = c_gnulightning;
    }
    if (do_codestyle == c_gnulightning && do_run == -1) do_run = 1;
    if (do_codestyle == c_gnulightning && cell_length>0 && cell_size == 0) {
	fprintf(stderr, "The GNU Lightning generator does not support that cell size\n");
	exit(255);
    }
#endif
#if XX == 6
if (cell_size == 0 && do_codestyle == c_gnulightning) set_cell_size(-1);
#endif
#endif
#endif
