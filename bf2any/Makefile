# Makefile for BF2ANY
# Note this is compatible with Debian/GCC multiarch and also notices
# the environment variable TARGET_ARCH.
# You can use:
#	export TARGET_ARCH=-m32
# or
#	export TARGET_ARCH=-m64
#
# Both bf2crun and bf2jit are multiarch aware on i386 vs amd64. (The kernel
# must be 64bit for a 64bit application, but the userspace works both ways)

INSTALLDIR=/usr/local/bin
ALLEXE=\
	bf2asmjs bf2awk bf2bas bf2bash bf2bf bf2cgmp bf2clojure \
	bf2cmd bf2crun bf2d bf2dc bf2elf bf2gas32 bf2gas64 bf2go \
	bf2julia bf2lua bf2nav bf2neko bf2pas bf2perl bf2php bf2ps1 \
	bf2py bf2rb bf2rc bf2run bf2sh bf2slang bf2tcl bf2v7sh

TOOLDIR=../tools
LDLIBS=
ifneq ($(wildcard $(TOOLDIR)/dynasm/dynasm),)
ALLEXE+=bf2jit
endif

HAVE_TCC=$(wildcard /usr/include/libtcc.h)
ifeq ($(HAVE_TCC),)
TCCFLAG=-DNO_LIBTCC
TCCLIBS=
TCCDEP=libtcc.lib-off
else
TCCFLAG=
TCCLIBS=-ltcc
TCCDEP=libtcc.lib-on
endif
ifeq ($(findstring BSD,$(shell uname -s)),)
LIBDL=-ldl
endif

HAVE_TCL=$(wildcard /usr/include/tcl/tcl.h)
ifeq ($(HAVE_TCL),)
TCLFLAG=-DDISABLE_LIBTCL
TCLLIBS=
TCLDEP=libtcl.lib-off
else
TCLFLAG=
TCLLIBS=-ltcl
TCLDEP=libtcl.lib-off
endif

all:	bf2any.o bf2const.o $(ALLEXE)

clean:
	-rm -f $(ALLEXE)
	-rm -f libtcc.lib-off libtcc.lib-on libtcl.lib-off libtcl.lib-on
	-rm -f bf2jit.amd64.h bf2jit.i686.h
	-rm -f *.o

install: all
	install -p -s $(ALLEXE) $(INSTALLDIR)

CFLAGS=-O3 -Wall -Wshadow -DBOFF=256 $(DEFS)

# for: make DEFS='$(MOREDEFS)'
MOREDEFS=-Wextra -Wfloat-equal -Wundef -Wpointer-arith -Wcast-align \
         -Wstrict-prototypes -Wstrict-overflow=5 -Wwrite-strings \
	 -Waggregate-return -Wcast-qual -Wswitch-enum -Wconversion \
	 -Wunreachable-code

# Delete the direct to exe rule to make sure %.o files are used.
% : %.c

# Keep the .o files so we can relink.
.PRECIOUS: %.o

$(ALLEXE): bf2any.o bf2const.o

# Special cases.
bf2gas32.o: bf2gas.c
	$(CC) $(CFLAGS) -DUSE32 $(TARGET_ARCH) -c -o $@ bf2gas.c

bf2gas64.o: bf2gas.c
	$(CC) $(CFLAGS) -DUSE64 $(TARGET_ARCH) -c -o $@ bf2gas.c

bf2jit.o: bf2jit.c bf2jit.amd64.h bf2jit.i686.h
	$(CC) $(CFLAGS) -I$(TOOLDIR) $(TARGET_ARCH) -c -o $@ bf2jit.c

bf2jit.amd64.h: bf2jit.x86.dasc
	$(TOOLDIR)/dynasm/dynasm -c -o bf2jit.amd64.h bf2jit.x86.dasc

bf2jit.i686.h: bf2jit.x86.dasc
	$(TOOLDIR)/dynasm/dynasm -c -D I386 -o bf2jit.i686.h bf2jit.x86.dasc

bf2crun: bf2crun.o bf2any.o bf2const.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH) $(TCCLIBS) $(LIBDL)

bf2crun.o: bf2crun.c $(TCCDEP)
	$(CC) $(CFLAGS) $(TCCFLAG) -c -o $@ bf2crun.c $(TARGET_ARCH)

libtcc.lib-on:
	-@rm -f libtcc.lib-off
	@touch libtcc.lib-on

libtcc.lib-off:
	-@rm -f libtcc.lib-on
	@touch libtcc.lib-off

bf2tcl: bf2tcl.o bf2any.o bf2const.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH) $(TCLLIBS)

bf2tcl.o: bf2tcl.c $(TCLDEP)
	$(CC) $(CFLAGS) $(TCLFLAG) -c -o $@ bf2tcl.c $(TARGET_ARCH)

libtcl.lib-on:
	-@rm -f libtcl.lib-off
	@touch libtcl.lib-on

libtcl.lib-off:
	-@rm -f libtcl.lib-on
	@touch libtcl.lib-off

