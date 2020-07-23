#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NO_INTERFACE_EXT
#define be_interface (*(interfaces[be_id].be))
#include "bf2any.h"

#define TAPELEN 30000

extern struct be_interface_s
    int_bf2bf,
    int_bf2gas,
    int_bf2elf,
    int_bf2oldsh,
    int_bf2rc;

static struct interface_s {
    const char * name;
    struct be_interface_s *be;
    const char * desc;
} interfaces[] = {
    { "bf2bf", &int_bf2bf, "Backend with various small translations" },
    { "bf2gas", &int_bf2gas, "Backend to translate to assembler" },
    { "bf2elf", &int_bf2elf, "Backend to translate to an ELF executable" },
    { "bf2oldsh", &int_bf2oldsh, "Backend for simple Bourne shell" },
    { "bf2rc", &int_bf2rc, "Backend for the Plan9 rc shell" },
    { 0, 0, 0 }
};

static int be_id = -1;

int
be_option(const char * arg)
{
    if (strcmp(arg, "-h") == 0) {
	int i;
	fprintf(stderr, "\n");
	for(i=0; interfaces[i].be; i++) {
	    if (interfaces[i].be->gen_code == 0) continue;

	    fprintf(stderr, "    -%-11s   %s\n",
		interfaces[i].name,
		interfaces[i].desc);
	    if (interfaces[i].be->check_arg) {
		if ((*interfaces[i].be->check_arg)(arg))
		    fprintf(stderr, "\n");
	    }
	}
	return 1;
    }

    if (be_id < 0 || interfaces[be_id].be->gen_code == 0) {
	int i;
	if (arg[0] != '-') return 0;

	for(i=0; interfaces[i].be; i++) {
	    if (strcmp(arg+1, interfaces[i].name) == 0) {
		be_id = i;
		interfaces[be_id].be->tape_len = TAPELEN;
		interfaces[be_id].be->fe_enable_optim = 1;
		return 1;
	    }
	}
	return 0;
    }

    if (be_id < 0) return 0;

    if (strncmp(arg, "-M", 2) == 0 && arg[2] != 0) {
	int tapelen = strtoul(arg+2, 0, 10);
	if (tapelen < 1) tapelen = TAPELEN;
	tapesz = tapelen + tapeinit;
	return 1;
    }

    if (interfaces[be_id].be->check_arg == 0) return 0;
    return (*interfaces[be_id].be->check_arg)(arg);
}

static void
outcmd(int ch, int count)
{
    if (be_id < 0 || interfaces[be_id].be->gen_code == 0) return;

    (*interfaces[be_id].be->gen_code)(ch, count, 0);
}

void be_outcmd(int mov, int cmd, int arg)
{
    if (mov > 0 )
	outcmd('>', mov);
    else if (mov < 0)
	outcmd('<', -mov);

    if (cmd == 0) {
	be_option("+init");
	outcmd('!', 0);
	return;
    }
    else if (cmd == '[' || cmd == ']')
	outcmd(cmd, 0);
    else if (cmd == '!') {
	outcmd('[', 0);
	outcmd('X', 1);
	outcmd(']', 0);
    }
    else if (cmd == '.' || cmd == ',')
	outcmd(cmd, 0);
    else if (cmd == '+' && arg < 0)
	outcmd('-', -arg);
    else if (cmd == '>' && arg < 0)
	outcmd('<', -arg);
    else
	outcmd(cmd, arg);
}
