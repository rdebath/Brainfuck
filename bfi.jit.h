#ifndef _BFI_JIT_H
#define _BFI_JIT_H

#ifdef ENABLE_GNULIGHTNING

void run_jit_asm(void);
extern int jit_lib_ok;
#define BE_JIT

#endif
#endif
