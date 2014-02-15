# Note this is compatible with Debian/GCC multiarch and also notices
# the environment variable TARGET_ARCH.
# You can use:
#       export TARGET_ARCH=-m32
# or
#       export TARGET_ARCH=-m64
#
# GNU lightning may be V1 or V2 and is detected; note, however, that V1 does
# NOT work on a 64bit x64. (You can try TARGET_ARCH=-m32)
#
# Dasm only has assembler for x86 and x86_64 others have not yet been written.
# It is normally a little faster that GNU Lightning.

CFLAGS=-O3 -Wall -Wshadow -I../tools $(DEFS) $(CONF)
LDFLAGS=-ldl
CC=gcc

# for: make DEFS='$(MOREDEFS)'
MOREDEFS=-Wextra -Wfloat-equal -Wundef -Wpointer-arith -Wcast-align \
	 -Wstrict-prototypes -Wstrict-overflow=5 -Wwrite-strings \
	 -Waggregate-return -Wcast-qual -Wswitch-enum -Wconversion \
	 -Wunreachable-code

HAVE_TCC=$(wildcard /usr/include/libtcc.h)
ifeq ($(HAVE_TCC),)
CONF_TCCLIB=-DDISABLE_TCCLIB
LIBS_TCCLIB=
else
CONF_TCCLIB=
LIBS_TCCLIB=-ltcc
endif

HAVE_LIGHT1=$(wildcard /usr/include/lightning.h)
HAVE_LIGHT2=$(wildcard /usr/include/lightning/asm.h)
ifeq ($(HAVE_LIGHT1),)
CONF_LIGHTNING=-DDISABLE_GNULIGHTNING
LIBS_LIGHTNING=
else
ifeq ($(HAVE_LIGHT2),)
CONF_LIGHTNING=
LIBS_LIGHTNING=-llightning
else
CONF_LIGHTNING=
LIBS_LIGHTNING=
endif
endif

CONF=$(CONF_TCCLIB) $(CONF_LIGHTNING)
LDLIBS=$(LIBS_TCCLIB) $(LIBS_LIGHTNING)

bfi:	bfi.o bfi.ccode.o bfi.gnulit.o bfi.nasm.o bfi.bf.o bfi.dc.o bfi.dasm.o \
	bfi.version.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o bfi $^ $(LDLIBS) $(TARGET_ARCH)

pristine: clean
	-rm -f bfi

clean:
	-rm -f *.o bfi.dasm.amd64.h bfi.dasm.i686.h

bfi.gnulit.o:	bfi.gnulit.c bfi.tree.h bfi.gnulit.h bfi.run.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ -fno-strict-aliasing bfi.gnulit.c

bfi.o: \
    bfi.c bfi.tree.h bfi.run.h bfi.be.def bfi.ccode.h bfi.gnulit.h \
    bfi.nasm.h bfi.bf.h bfi.dc.h
bfi.version.o: bfi.version.h
bfi.bf.o: bfi.bf.c bfi.tree.h
bfi.ccode.o: bfi.ccode.c bfi.tree.h bfi.run.h bfi.ccode.h
bfi.dc.o: bfi.dc.c bfi.tree.h bfi.run.h
bfi.nasm.o: bfi.nasm.c bfi.tree.h bfi.nasm.h

bfi.dasm.o: bfi.dasm.amd64.h bfi.dasm.i686.h bfi.dasm.h bfi.tree.h bfi.run.h

bfi.dasm.amd64.h: bfi.dasm.x86.dasc
	../tools/dynasm/dynasm -c -o bfi.dasm.amd64.h bfi.dasm.x86.dasc

bfi.dasm.i686.h: bfi.dasm.x86.dasc
	../tools/dynasm/dynasm -c -D I386 -o bfi.dasm.i686.h bfi.dasm.x86.dasc

bfi.version.h: \
    bfi.c bfi.tree.h bfi.run.h bfi.be.def bfi.ccode.h bfi.gnulit.h bfi.nasm.h \
    bfi.bf.h bfi.dc.h bfi.bf.c bfi.ccode.c bfi.dc.c bfi.gnulit.c bfi.nasm.c
	@sed -n 's/.*VERSION_BUILD\s*\([0-9]*\)\s*$$/\1/p' \
		    < bfi.version.h > bfi.version.tmp
	@expr `cat bfi.version.tmp` + 1 > bfi.build.tmp
	@echo "New build number: `cat bfi.build.tmp`"
	@sed -e "s/\(^.*VERSION_BUILD\s*\)[0-9]*/\1`cat bfi.build.tmp`/" \
	     -e "s/\(^.*VERSION_DATE\s*\)[^\s].*/\1\"`date --rfc-3339=seconds`\"/" \
		    < bfi.version.h > bfi.version.tmp
	@mv bfi.version.tmp bfi.version.h
	@rm bfi.build.tmp
