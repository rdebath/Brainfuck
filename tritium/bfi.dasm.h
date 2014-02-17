#ifndef DISABLE_DYNASM
#ifndef _BFI_DYNASM_H
#define _BFI_DYNASM_H

void run_dynasm(void);
int checkarg_dynasm(char * opt, char * arg);
extern int dynasm_ok;
#define BE_DYNASM

#endif
#endif
