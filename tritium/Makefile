# This is compatible with Debian/GCC multiarch and also notices
# the environment variable TARGET_ARCH.
# You can use:
#       export TARGET_ARCH=-m32
# or
#       export TARGET_ARCH=-m64
#
# GNU lightning may be V1 or V2 and is detected; note, however, that V1 does
# NOT work on a 64bit x64. (You can try TARGET_ARCH=-m32 after installing the
# gcc-multilib package with apt-get. The arch flag -mx32 will NOT work because
# default Debian kernels do not include the required support.)
#
# Dasm only has assembler for x86 and x86_64 others have not yet been written.
# It is normally a little faster that GNU Lightning.
#
# The JIT interpreters will normally get disabled if their requirements don't
# exist but as the tests are very simple they can be confused. Especially the
# Dynasm which needs either luajit or a normal lua with the 'bitops' package.
#
# To manually disable a 'JITter' add one or more of the options below to the
# make command line.
#
# make DO_TCC= DO_LIGHT= DO_LIBDL= DO_DYNASM=

# Always run the version check script.
VERSTRING := $(shell sh version.sh bfi.version.h)

all: bfi

CC=gcc
CFLAGS=-O3 -Wall -Wshadow -idirafter $(TOOLDIR) $(DEFS) $(CONF)
LDFLAGS=
TOOLDIR=../tools

# for: make DEFS='$(MOREDEFS)'
MOREDEFS=-Wextra -Wfloat-equal -Wundef -Wpointer-arith -Wcast-align \
	 -Wstrict-prototypes -Wstrict-overflow=5 -Wwrite-strings \
	 -Waggregate-return -Wcast-qual -Wswitch-enum -Wconversion \
	 -Wunreachable-code

bfi:	bfi.o bfi.ccode.o bfi.gnulit.o bfi.nasm.o bfi.bf.o bfi.dc.o bfi.dasm.o \
	bfi.version.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o bfi $^ $(LDLIBS) $(TARGET_ARCH)

pristine: clean
	-rm -f bfi

clean:
	-rm -f *.o bfi.dasm.amd64.h bfi.dasm.i686.h bfi.version.h

bfi.gnulit.o:	bfi.gnulit.c bfi.tree.h bfi.gnulit.h bfi.run.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ -fno-strict-aliasing -Wno-unused-value bfi.gnulit.c

bfi.o: \
    bfi.c bfi.tree.h bfi.run.h bfi.be.def bfi.ccode.h bfi.gnulit.h \
    bfi.nasm.h bfi.bf.h bfi.dc.h
bfi.version.o: bfi.version.h
bfi.bf.o: bfi.bf.c bfi.tree.h
bfi.ccode.o: bfi.ccode.c bfi.tree.h bfi.run.h bfi.ccode.h
bfi.dc.o: bfi.dc.c bfi.tree.h bfi.run.h
bfi.nasm.o: bfi.nasm.c bfi.tree.h bfi.nasm.h

bfi.dasm.o: bfi.dasm.h bfi.tree.h bfi.run.h

bfi.dasm.amd64.h: bfi.dasm.x86.dasc
	$(TOOLDIR)/dynasm/dynasm -c -o bfi.dasm.amd64.h bfi.dasm.x86.dasc

bfi.dasm.i686.h: bfi.dasm.x86.dasc
	$(TOOLDIR)/dynasm/dynasm -c -D I386 -o bfi.dasm.i686.h bfi.dasm.x86.dasc

# Auto configure

DO_TCC=$(wildcard /usr/include/libtcc.h)
DO_LIBDL=$(wildcard /usr/include/dlfcn.h)
DO_DYNASM=$(wildcard $(TOOLDIR)/dynasm/dynasm)
DO_MYLIGHT=$(wildcard $(TOOLDIR)/lightning.h)
DO_LIGHT=$(wildcard /usr/include/lightning.h)

ifeq ($(DO_LIBDL),)
CONF_LIBDL=-DDISABLE_DLOPEN
LIBS_LIBDL=
CONF_TCCLIB=-DDISABLE_TCCLIB
LIBS_TCCLIB=
else
CONF_LIBDL=
ifneq ($(wildcard /usr/include/bits/dlfcn.h),)
LIBS_LIBDL=-ldl
endif

ifeq ($(DO_TCC),)
CONF_TCCLIB=-DDISABLE_TCCLIB
LIBS_TCCLIB=
else
CONF_TCCLIB=
LIBS_TCCLIB=-ltcc
endif
endif

ifeq ($(DO_LIGHT),)
ifeq ($(DO_MYLIGHT),)
CONF_LIGHTNING=-DDISABLE_GNULIGHTNING
LIBS_LIGHTNING=
endif
else
DO_LIGHT2=$(wildcard /usr/include/lightning/jit_*.h)
ifneq ($(DO_LIGHT2),)
CONF_LIGHTNING=
LIBS_LIGHTNING=-llightning
endif
endif

ifeq ($(DO_DYNASM),)
CONF_DYNASM=-DDISABLE_DYNASM
else
bfi.dasm.o: bfi.dasm.amd64.h bfi.dasm.i686.h
endif

CONF=$(CONF_DYNASM) $(CONF_LIGHTNING) $(CONF_TCCLIB) $(CONF_LIBDL)
LDLIBS=$(LIBS_LIGHTNING) $(LIBS_TCCLIB) $(LIBS_LIBDL)
