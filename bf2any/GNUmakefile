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
#
# Beware: pkg-config doesn't cope with multiarch.

CC := $(shell ( which 2>/dev/null gcc clang pcc ; echo cc ) | head -1 | sed 's:.*/::')
INSTALL=install -p -s
INSTALLDIR=/usr/local/bin
INSTALLEXT=
ALLEXE=\
	bf2asmjs bf2awk bf2bas bf2bf bf2cgmp bf2clojure bf2cmd bf2crun \
	bf2d bf2dc bf2elf bf2f90 bf2gas32 bf2gas64 bf2go bf2julia bf2ksh \
	bf2lua bf2nav bf2neko bf2pas bf2perl bf2php bf2ps1 bf2py bf2rb \
	bf2rc bf2run bf2semi bf2sh bf2slang bf2tcl bf2tsql bf2oldsh \
	bf2whitespace bf2csh bf2cs

TOOLDIR=../tools
LDLIBS=

ifneq ($(wildcard $(TOOLDIR)/dynasm/dynasm),)
CPUFOUND := $(shell $(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -DDETECT_ONLY \
		-E -dM -x c bf2jit.x86.dasc | grep DASM_FOUND_CPU )
endif
ifneq ($(CPUFOUND),)
ALLEXE+=bf2jit
endif

all:	bf2any.o bf2const.o $(ALLEXE)

clean:
	-rm -f $(ALLEXE)
	-rm -f libtcc.lib-off libtcc.lib-on
	-rm -f libtcl.lib-off libtcl.lib-on
	-rm -f liblua.lib-off liblua.lib-on
	-rm -f libpy.lib-off libpy.lib-on
	-rm -f bf2jit.c
	-rm -f *.o

# Install all we can make, but don't fail if some can't be built.
install:
	-@$(MAKE) -k all
	-@echo Installing: $(ALLEXE) to $(INSTALLDIR)
	-@for i in $(ALLEXE) ; do \
	$(INSTALL) $$i $(INSTALLDIR)/$$i$(INSTALLEXT) ; \
	done

ifneq ($(CC),cc)
CFLAGS=-O3 -Wall -Wshadow -DBOFF=256 $(DEFS)
else
CFLAGS=-O -DBOFF=256 $(DEFS)
endif

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

#-------------------------------------------------------------------------------
# Special cases.
#-------------------------------------------------------------------------------
PKGCONF=pkg-config$(TARGET_ARCH)
#-------------------------------------------------------------------------------
# Clone the two plain assembly variants.
bf2gas32.o: bf2gas.c
	$(CC) $(CFLAGS) -DUSE32 $(TARGET_ARCH) -c -o $@ bf2gas.c

bf2gas64.o: bf2gas.c
	$(CC) $(CFLAGS) -DUSE64 $(TARGET_ARCH) -c -o $@ bf2gas.c

bf2semi.o: bf2semi.c bf2whitespace.c

#-------------------------------------------------------------------------------
# Assemble the dynasm machine code.
ifneq ($(CPUFOUND),)
bf2jit.o: bf2jit.c
	$(CC) $(CFLAGS) -I$(TOOLDIR) $(TARGET_ARCH) -c -o $@ bf2jit.c

ifneq ($(findstring X86_64,$(CPUFOUND)),)
bf2jit.c: bf2jit.x86.dasc
	$(TOOLDIR)/dynasm/dynasm -c -o bf2jit.c bf2jit.x86.dasc
endif

ifneq ($(findstring X86_32,$(CPUFOUND)),)
bf2jit.c: bf2jit.x86.dasc
	$(TOOLDIR)/dynasm/dynasm -c -D I386 -o bf2jit.c bf2jit.x86.dasc
endif
endif

#-------------------------------------------------------------------------------
# Libraries for running C code in process.
HAVE_DL=$(wildcard /usr/include/dlfcn.h)
ifeq ($(HAVE_DL),)
LIBDL=
LIBDLFLAG=-DDISABLE_DLOPEN
else
ifneq ($(findstring Linux,$(shell uname -s)),)
LIBDL=-ldl
endif
endif

HAVE_TCC=$(wildcard /usr/include/libtcc.h)
ifeq ($(HAVE_TCC),)
TCCFLAG=-DDISABLE_LIBTCC
TCCLIBS=
TCCDEP=libtcc.lib-off
else
TCCFLAG=
TCCLIBS=-ltcc
TCCDEP=libtcc.lib-on
endif

bf2crun: bf2crun.o bf2any.o bf2const.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH) $(TCCLIBS) $(LIBDL)

bf2crun.o: bf2crun.c $(TCCDEP)
	$(CC) $(CFLAGS) $(TCCFLAG) $(LIBDLFLAG) -c -o $@ bf2crun.c $(TARGET_ARCH)

libtcc.lib-on:
	-@rm -f libtcc.lib-off
	@touch libtcc.lib-on

libtcc.lib-off:
	-@rm -f libtcc.lib-on
	@touch libtcc.lib-off

#-------------------------------------------------------------------------------
# TCL doesn't use pkg-config and so is usually installed sanely, but this may
# change as pkgconf (the svelte version) is available.
HAVE_TCL=$(shell ($(PKGCONF) --cflags --libs tcl ) >/dev/null 2>&1 && echo tcl)
ifneq ($(HAVE_TCL),)
TCLFLAG=$(shell $(PKGCONF) --cflags $(HAVE_TCL))
TCLLIBS=$(shell $(PKGCONF) --libs $(HAVE_TCL))
TCLDEP=libtcl.lib-on
else
HAVE_TCL_H=$(wildcard /usr/include/tcl/tcl.h)
ifeq ($(HAVE_TCL_H),)
TCLFLAG=-DDISABLE_LIBTCL
TCLLIBS=
TCLDEP=libtcl.lib-off
else
TCLFLAG=-I/usr/include/tcl
TCLLIBS=-ltcl
TCLDEP=libtcl.lib-on
endif
endif

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

#-------------------------------------------------------------------------------
# This uses 'pkg-config' to lookup the weird flags your version
# of libpython-dev needs. If either is not available the interface
# is disabled.
HAVE_PYLIB=$(shell ($(PKGCONF) --list-all ) 2>/dev/null | \
	sed -n 's/^\(python-[^ ]*\).*/\1/p' | \
	sort -r | head -1)
ifeq ($(HAVE_PYLIB),)
PYLIBFLAG=-DDISABLE_LIBPY
PYLIBLIBS=
PYLIBDEP=libpy.lib-off
else
PYLIBFLAG=$(shell $(PKGCONF) --cflags $(HAVE_PYLIB))
PYLIBLIBS=$(shell $(PKGCONF) --libs $(HAVE_PYLIB))
PYLIBDEP=libpy.lib-on
endif

bf2py: bf2py.o bf2any.o bf2const.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH) $(PYLIBLIBS)

bf2py.o: bf2py.c $(PYLIBDEP)
	$(CC) $(CFLAGS) $(PYLIBFLAG) -c -o $@ bf2py.c $(TARGET_ARCH)

libpy.lib-on:
	-@rm -f libpy.lib-off
	@touch libpy.lib-on

libpy.lib-off:
	-@rm -f libpy.lib-on
	@touch libpy.lib-off

#-------------------------------------------------------------------------------
# This uses 'pkg-config' to lookup the weird flags your version
# of the lua library needs. If either is not available the interface
# is disabled.
HAVE_LUALIB=$(shell ($(PKGCONF) --list-all ) 2>/dev/null | \
	sed -n -e 's/^\(luajit\) .*/\1/p' -e 's/^\(lua5\..\) .*/\1/p' | \
	sort -r | head -1)
ifeq ($(HAVE_LUALIB),)
LUALIBFLAG=-DDISABLE_LIBLUA
LUALIBLIBS=
LUALIBDEP=liblua.lib-off
else
LUALIBFLAG=$(shell $(PKGCONF) --cflags $(HAVE_LUALIB))
LUALIBLIBS=$(shell $(PKGCONF) --libs $(HAVE_LUALIB))
LUALIBDEP=liblua.lib-on
endif

bf2lua: bf2lua.o bf2any.o bf2const.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH) $(LUALIBLIBS)

bf2lua.o: bf2lua.c $(LUALIBDEP)
	$(CC) $(CFLAGS) $(LUALIBFLAG) -c -o $@ bf2lua.c $(TARGET_ARCH)

liblua.lib-on:
	-@rm -f liblua.lib-off
	@touch liblua.lib-on

liblua.lib-off:
	-@rm -f liblua.lib-on
	@touch liblua.lib-off

#-------------------------------------------------------------------------------
