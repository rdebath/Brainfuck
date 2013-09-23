#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <lightning.h>

#include "bfi.tree.h"
#include "bfi.run.h"

/*
 *
 */

/* GNU lightning's macros upset GCC a little ... */
#pragma GCC diagnostic ignored "-Wunused-value"

static jit_insn codeBuffer[1024*2048];

/*
 * For i386 JIT_V0 == ebx, JIT_V1 == esi, JIT_V2 == edi
 *          JIT_R0 == eax, JIT_R1 == ecx, JIT_R2 == edx
 *
 *  So use the zeros (as expected).
 */

#define REG_P   JIT_V0
#define REG_ACC JIT_R0
#define REG_A1	JIT_R1

#define TS  sizeof(int)

static void (*codeptr)();
static int acc_loaded = 0;
static int acc_offset = 0;
static int acc_dirty = 0;

static void
clean_acc(void)
{
    if (acc_loaded && acc_dirty) {
	if (acc_offset)
	    jit_stxi_i(acc_offset * TS, REG_P, REG_ACC); 
	else
	    jit_str_i(REG_P, REG_ACC); 

	acc_dirty = 0;
    }
}

static void
set_acc_offset(int offset)
{
    if (acc_loaded && acc_dirty && acc_offset != offset)
	clean_acc();

    acc_offset = offset;
    acc_loaded = 1;
    acc_dirty = 1;
}

static void
load_acc_offset(int offset)
{
    if (acc_loaded) {
	if (acc_offset == offset) return;
	if (acc_dirty)
	    clean_acc();
    }

    acc_offset = offset;
    if (acc_offset)
	jit_ldxi_i(REG_ACC, REG_P, acc_offset * TS); 
    else
	jit_ldr_i(REG_ACC, REG_P); 

    acc_loaded = 1;
    acc_dirty = 0;
}

void 
run_jit_asm(void)
{
    struct bfi * n = bfprog;

    jit_insn** loopstack = 0;
    int maxstack = 0, stackptr = 0;

    codeptr = jit_set_ip(codeBuffer).vptr;
    void * startptr = jit_get_ip().ptr;
    /* Function call prolog */
    jit_prolog(0);
    /* Get the data area pointer */
    jit_movi_p(REG_P, map_hugeram());

    while(n)
    {
	switch(n->type)
	{
	case T_MOV:
	    clean_acc();
	    acc_loaded = 0;

	    jit_addi_i(REG_P, REG_P, n->count * TS);
	    break;

	case T_ADD:
	    load_acc_offset(n->offset);
	    set_acc_offset(n->offset);

	    jit_addi_i(REG_ACC, REG_ACC, n->count);
	    break;

	case T_SET:
	    set_acc_offset(n->offset);
	    jit_movi_i(REG_ACC, n->count);
	    break;

	case T_CALC:
	    if (n->offset == n->offset2 && n->count2 == 1) {
		load_acc_offset(n->offset);
		set_acc_offset(n->offset);
		if (n->count)
		    jit_addi_i(REG_ACC, REG_ACC, n->count);
	    } else if (n->count2 != 0) {
		load_acc_offset(n->offset2);
		set_acc_offset(n->offset);
		if (n->count2 == -1)
		    jit_negr_i(REG_ACC, REG_ACC);
		else if (n->count2 != 1)
		    jit_muli_i(REG_ACC, REG_ACC, n->count2);
		if (n->count)
		    jit_addi_i(REG_ACC, REG_ACC, n->count);
	    } else {
		clean_acc();
		set_acc_offset(n->offset);

		jit_movi_i(REG_ACC, n->count);
		if (n->count2 != 0) {
		    jit_ldxi_i(REG_A1, REG_P, n->offset2 * TS); 
		    if (n->count2 == -1)
			jit_negr_i(REG_A1, REG_A1);
		    else if (n->count2 != 1)
			jit_muli_i(REG_A1, REG_A1, n->count2);
		    jit_addr_i(REG_ACC, REG_ACC, REG_A1);
		}
	    }

	    if (n->count3 != 0) {
		jit_ldxi_i(REG_A1, REG_P, n->offset3 * TS); 
		if (n->count3 == -1)
		    jit_negr_i(REG_A1, REG_A1);
		else if (n->count3 != 1)
		    jit_muli_i(REG_A1, REG_A1, n->count3);
		jit_addr_i(REG_ACC, REG_ACC, REG_A1);
	    }
	    break;

	case T_IF: case T_MULT: case T_CMULT:
	case T_MFIND: case T_ZFIND: case T_ADDWZ: case T_FOR:
	case T_WHL:
	    load_acc_offset(n->offset);
	    clean_acc();

	    if (stackptr >= maxstack) {
		loopstack = realloc(loopstack,
			    ((maxstack+=32)+2)*sizeof(jit_insn*));
		if (loopstack == 0) {
		    perror("loop stack realloc failure");
		    exit(1);
		}
	    }

	    if (cell_mask > 0) {
		if (cell_mask == 0xFF)
		    jit_extr_uc_i(REG_ACC,REG_ACC);
		else
		    jit_andi_i(REG_ACC, REG_ACC, cell_mask);
	    }

	    loopstack[stackptr] = jit_beqi_i(jit_forward(), REG_ACC, 0);
	    loopstack[stackptr+1] = jit_get_label();
	    stackptr += 2;
	    break;

	case T_END:
	    load_acc_offset(n->offset);
	    clean_acc();

	    stackptr -= 2;
	    if (stackptr < 0) {
		fprintf(stderr, "Code gen failure: Stack pointer negative.\n");
		exit(1);
	    }

	    if (cell_mask > 0) {
		if (cell_mask == 0xFF)
		    jit_extr_uc_i(REG_ACC,REG_ACC);
		else
		    jit_andi_i(REG_ACC, REG_ACC, cell_mask);
	    }

	    jit_bnei_i(loopstack[stackptr+1], REG_ACC, 0);
	    jit_patch(loopstack[stackptr]);
	    break;

	case T_ENDIF:
	    clean_acc();
	    acc_loaded = 0;

	    stackptr -= 2;
	    if (stackptr < 0) {
		fprintf(stderr, "Code gen failure: Stack pointer negative.\n");
		exit(1);
	    }
	    jit_patch(loopstack[stackptr]);
	    break;

	case T_PRT:
	    clean_acc();
	    if (n->count == -1) {
		load_acc_offset(n->offset);
		acc_loaded = 0;

		if (cell_mask > 0) {
		    if (cell_mask == 0xFF)
			jit_extr_uc_i(REG_ACC,REG_ACC);
		    else
			jit_andi_i(REG_ACC, REG_ACC, cell_mask);
		}

		jit_prepare_i(1);
		jit_pusharg_i(REG_ACC);
		jit_finish(putch);
		break;
	    }

	    /* TODO: Strings */
	    jit_movi_i(REG_ACC, n->count);
	    jit_prepare_i(1);
	    jit_pusharg_i(REG_ACC);
	    jit_finish(putch);
	    break;

	case T_INP:
	    load_acc_offset(n->offset);
	    set_acc_offset(n->offset);

	    jit_prepare_i(1);
	    jit_pusharg_i(REG_ACC);
	    jit_finish(getch);
	    jit_retval_i(REG_ACC);     
	    break;

	default:
	    fprintf(stderr, "Code gen error: "
		    "%s\t"
		    "%d,%d,%d,%d,%d,%d\n",
		    tokennames[n->type],
		    n->offset, n->count,
		    n->offset2, n->count2,
		    n->offset3, n->count3);
	    exit(1);
	    break;
	}
	n=n->next;
    }

    jit_ret();
    jit_flush_code(startptr, jit_get_ip().ptr);

    if (verbose)
	fprintf(stderr, "Generated %d bytes of machine code, running\n",
		jit_get_ip().ptr - (char*)startptr);

    codeptr();

#if 0
  /* This code writes the generated instructions to a file
     so we can disassemble it using ndisasm. */
    {
      char * p = startptr;
      int s = jit_get_ip().ptr - p;
      FILE * fp = fopen("code.bin", "w");
      int i;
      for (i = 0; i < s; ++i) {
	fputc(p[i], fp);
      }
      fclose(fp);
    }
#endif
}
