
# TARGET_ARCH=-m64
# TARGET_ARCH=-m32

CFLAGS=-O3 -Wall -Wshadow $(DEFS)
LDFLAGS=-ldl
CC=gcc

# for: make DEFS='$(MOREDEFS)'
MOREDEFS=-Wextra -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wcast-align \
    -Wstrict-prototypes -Wstrict-overflow=5 -Wwrite-strings -Waggregate-return \
    -Wcast-qual -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code

-include Makefile2

HAVE_TCC=$(wildcard /usr/include/libtcc.h)
ifeq ($(HAVE_TCC),)
DEFS_TCCLIB=-DDISABLE_TCCLIB
LIBS_TCCLIB=
else
DEFS_TCCLIB=
LIBS_TCCLIB=-ltcc
endif

HAVE_LIGHT1=$(wildcard /usr/include/lightning.h)
HAVE_LIGHT2=$(wildcard /usr/include/lightning/asm.h)
ifeq ($(HAVE_LIGHT1),)
DEFS_LIGHTNING=-DDISABLE_GNULIGHTNING
LIBS_LIGHTNING=
else
ifeq ($(HAVE_LIGHT2),)
DEFS_LIGHTNING=
LIBS_LIGHTNING=-llightning
else
DEFS_LIGHTNING=
LIBS_LIGHTNING=
endif
endif

DEFS=$(DEFS_TCCLIB) $(DEFS_LIGHTNING)
LDLIBS=$(LIBS_TCCLIB) $(LIBS_LIGHTNING)

bfi:	bfi.o bfi.ccode.o bfi.jit.o bfi.nasm.o bfi.bf.o bfi.dc.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o bfi $^ $(LDLIBS) $(TARGET_ARCH)

clean:
	-rm -f *.o bfi

bfi.jit.o:	bfi.jit.c bfi.tree.h bfi.jit.h
	$(CC) $(CFLAGS) $(TARGET_ARCH)  -c -fno-strict-aliasing bfi.jit.c

bfi.version.h: \
    bfi.c bfi.tree.h bfi.run.h bfi.be.def bfi.ccode.h bfi.jit.h \
    bfi.nasm.h bfi.bf.h bfi.dc.h \
    bfi.bf.c bfi.ccode.c bfi.dc.c bfi.jit.c bfi.nasm.c
	@sed -n 's/.*VERSION_BUILD\s*\([0-9]*\)\s*$$/\1/p' \
		    < bfi.version.h > bfi.version.tmp
	@expr `cat bfi.version.tmp` + 1 > bfi.build.tmp
	@echo "New build number: `cat bfi.build.tmp`"
	@sed "s/\(^.*VERSION_BUILD\s*\)[0-9]*/\1`cat bfi.build.tmp`/" \
		    < bfi.version.h > bfi.version.tmp
	@mv bfi.version.tmp bfi.version.h
	@rm bfi.build.tmp

bfi.o: \
    bfi.c bfi.tree.h bfi.run.h bfi.be.def bfi.ccode.h bfi.jit.h \
    bfi.nasm.h bfi.bf.h bfi.dc.h bfi.version.h
bfi.bf.o: bfi.bf.c bfi.tree.h
bfi.ccode.o: bfi.ccode.c bfi.tree.h bfi.run.h bfi.ccode.h
bfi.dc.o: bfi.dc.c bfi.tree.h bfi.run.h
bfi.nasm.o: bfi.nasm.c bfi.tree.h bfi.nasm.h
