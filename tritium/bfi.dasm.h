#ifndef DISABLE_DYNASM
#ifndef _BFI_DYNASM_H
#define _BFI_DYNASM_H

void run_dynasm(void);
int checkarg_dynasm(char * opt, char * arg);
extern int dynasm_ok;
#define BE_DYNASM
#endif

#ifdef XX
X(dynasm,DYNASM,
    printf("   -q   Run using the Dynasm interpreter/assembler.\n");    ,
    case 'q': do_codestyle = c_dynasm; break;                ,
    /* Cannot do print_dynasm() */                                     ,
    case c_dynasm: run_dynasm(); break;                               )
#if XX == 4
    if (do_run == -1 && do_codestyle == c_default &&
	    (cell_length==0 || cell_size>0) &&
	    verbose<3 && !enable_trace && !debug_mode && dynasm_ok) {
	do_run = 1;
	do_codestyle = c_dynasm;
    }
    if (do_codestyle == c_dynasm && do_run == -1) do_run = 1;
    if (do_codestyle == c_dynasm && cell_length>0 && cell_size == 0) {
	fprintf(stderr, "The DynASM generator does not support that cell size\n");
	exit(255);
    }
#endif
#if XX == 6
if (cell_size == 0 && do_codestyle == c_dynasm) set_cell_size(-1);
#endif
#if XX == 9
    {	int f = checkarg_dynasm(opt, arg);
	if (f) return f;
    }
#endif
#endif
#endif
