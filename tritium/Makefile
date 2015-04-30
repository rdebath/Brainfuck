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
# exist but as the tests are very simple they can be confused.
#
# To manually disable a 'JITter' add one or more of the options below to the
# make command line.
#
# make DO_TCC= DO_LIGHT= DO_MYLIGHT= DO_LIBDL= DO_DYNASM=
#
# The internal GNU lightning and Dynasm can also be removed by deleting the
# relevent parts of the tools directory.
#
# There are VERSION and VERSIONID strings for you to override eg:
#
#       make VERSION=v1.2.3.4 VERSIONID=Your_Name_or_ORG

# More knobs to twiddle (the makefile uses INSTALL like the cp(1) command)
TARGETFILE=bfi
INSTALL=install -p -s
INSTALLDIR=/usr/local/bin
INSTALLEXT=

all: $(TARGETFILE)

# for: make DEFS='$(MOREDEFS)'
MOREDEFS=-Wextra -Wfloat-equal -Wundef -Wpointer-arith -Wcast-align \
	 -Wstrict-prototypes -Wstrict-overflow=5 -Wwrite-strings \
	 -Waggregate-return -Wcast-qual -Wswitch-enum -Wconversion \
	 -Wunreachable-code -fno-common -Wpadded \
	 -Wlarger-than=512 -Wstack-usage=512 -Wunsafe-loop-optimizations

OBJECTS=bfi.o bfi.version.o bfi.ccode.o bfi.nasm.o bfi.bf.o bfi.dc.o \
	clock.o taperam.o

CONF=$(CONF_DYNASM) $(CONF_LIGHTNING) $(CONF_TCCLIB) $(CONF_LIBDL)
LDLIBS=$(GNUSTK) $(LIBS_LIGHTNING) $(LIBS_TCCLIB) $(GNUDYN) $(LIBS_LIBDL)

$(TARGETFILE): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGETFILE) $(OBJECTS) $(LDLIBS) $(TARGET_ARCH)

pristine: clean
	-rm -f $(TARGETFILE)

clean:
	-rm -f *.o bfi.version.h bfi.dasm.c

install: $(TARGETFILE)
	$(INSTALL) $(TARGETFILE) $(INSTALLDIR)/$(TARGETFILE)$(INSTALLEXT)

bfi.gnulit.o:	bfi.gnulit.c bfi.tree.h bfi.gnulit.h bfi.run.h
	$(CC) $(CFLAGS) $(GNULGT) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ bfi.gnulit.c

bfi.version.o: bfi.version.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ bfi.version.c

bfi.o: \
    bfi.c bfi.tree.h bfi.run.h bfi.be.def bfi.ccode.h bfi.gnulit.h \
    bfi.nasm.h bfi.bf.h bfi.dc.h clock.h ov_int.h
bfi.bf.o: bfi.bf.c bfi.tree.h
bfi.ccode.o: bfi.ccode.c bfi.tree.h bfi.run.h bfi.ccode.h
bfi.dc.o: bfi.dc.c bfi.tree.h bfi.run.h
bfi.nasm.o: bfi.nasm.c bfi.tree.h bfi.nasm.h

taperam.o: bfi.tree.h bfi.run.h

clock.o: clock.h

################################################################################
LOGDEV=/dev/null
MARK := $(shell echo Configuration start >$(LOGDEV))
#
# Always running the version check script.
VERSTRING := $(shell sh version.sh bfi.version.h $(VERSION) $(VERSIONID))
#
CC := $(shell ( which gcc clang pcc ; echo cc ) | head -1 | sed 's:.*/::')

ifneq ($(CC),cc)
OPTFLG=-O2
CFLAGS=$(OPTFLG) -Wall -Wshadow $(FUNSAFE) $(INCLTOOLS) $(DEFS) $(CONF)
else
OPTFLG=-O
CFLAGS=$(OPTFLG) $(INCLTOOLS) $(DEFS) $(CONF)
endif
LDFLAGS=

# The CFLAGS used here does not include FUNSAFE or INCLTOOLS
CPUFOUND := $(shell $(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -E -dM bfi.version.c | grep BFI_FOUND_CPU )

ifneq ($(CPUFOUND),)
# The CPUs acceptable to the tools dir are all mentioned in bfi.version.c
TOOLDIR=../tools

ifneq ($(TOOLDIR),)
ifeq ($(filter cc tcc,$(CC)),)
INCLTOOLS=-idirafter $(TOOLDIR)
else
INCLTOOLS=-I $(TOOLDIR)
endif
endif
endif

################################################################################
# Current GCC policy seems to be to do any unsafe optimisation they can think
# of and let everyone else pick up the pieces. In addition v4.7 of GCC does
# it wrong, doing it's 'optimisation' on calculations that are defined by the
# C standard.
ifneq ($(CC),cc)
# Check for GCC options c/o the Linux kernel makefiles
cc-option = $(shell set -e;               \
	TMP="tmp.$$$$.tmp";		\
	TMPO="tmp.$$$$.o";		\
	echo Checking $(CC) "$(1)" > $(LOGDEV); \
	if ($(CC) $(1) -c -x c /dev/null -o "$$TMP") >$(LOGDEV) 2>&1;      \
	then echo "$(2)";               \
	else echo "$(3)";               \
	fi;                             \
	rm -f "$$TMP" "$$TMPO" )

WERROR := $(call cc-option,-Werror,-Werror)
UNSAFE += $(call cc-option,$(WERROR) -Wextra,-Wextra)

UNSAFE += $(call cc-option,$(WERROR) -fwrapv,-fwrapv)
UNSAFE += $(call cc-option,$(WERROR) -fno-strict-overflow,-fno-strict-overflow)
UNSAFE += $(call cc-option,$(WERROR) -fno-delete-null-pointer-checks,-fno-delete-null-pointer-checks)

# Clang doesn't error with unused options then gets noisy.
# But old GCC's have the same problem with -Qunused-arguments
UNSAFE += $(call cc-option,$(WERROR) -fno-strict-overflow -Qunused-arguments,-Qunused-arguments)

# This appears to be an amplifier not itself unsafe.
#   -fno-aggressive-loop-optimizations

# Try to turn off the warning nicely first.
GNULGT := $(call cc-option,-Wno-unused-value,-Wno-unused-value,-w)
GNULGT += $(call cc-option,-fno-strict-aliasing,-fno-strict-aliasing)

echo = $(1)
ifeq ($(call echo,ok),)	# This checks that the above is working (gmake 3.79+)
$(OBJECTS): a_newer_version_of_gnu_make

INSTALL=cp
GNULGT := -w
else
# Only check the flags once.
FUNSAFE := $(UNSAFE)
endif

################################################################################

try-run = $(shell set -e;               \
        TMP="tmp.$$$$.tmp";             \
        TMPO="tmp.$$$$.o";              \
        if ($(1)) >$(LOGDEV) 2>&1;      \
        then echo "$(2)";               \
        else echo "$(3)";               \
        fi;                             \
        rm -f "$$TMP" "$$TMPO" )

# Does libtcc.h exist ?
DO_TCC := $(call try-run,\
	echo '\#include <libtcc.h>' | $(CC) -c -x c - -o "$$TMP",1)

# How about lightning.h ?
DO_LIGHT := $(call try-run,\
	{ echo '\#include <lightning.h>'; \
	  echo '\#ifdef jit_addi'; echo '\#error Not V1'; echo '\#endif'; \
	  } | $(CC) -c -x c - -o "$$TMP",1)

DO_LIGHT2 := $(call try-run,\
	{ echo '\#include <lightning.h>'; \
	  echo '\#ifdef jit_addi_i'; echo '\#error Not V2'; echo '\#endif'; \
	  } | $(CC) -c -x c - -o "$$TMP",1)

endif

################################################################################

ifneq ($(STATIC_TOOLS),)
# These are used to link specified libaries statically.
# Other libraries (eg: libc) remain normally dynamic.
GNUSTK=-Wl,-Bstatic
GNUDYN=-Wl,-Bdynamic
endif

################################################################################
# Auto configure
DO_LIBDL=$(wildcard /usr/include/dlfcn.h)

ifeq ($(DO_LIBDL),)
CONF_LIBDL=-DDISABLE_DLOPEN
LIBS_LIBDL=
CONF_TCCLIB=-DDISABLE_TCCLIB
LIBS_TCCLIB=
else
CONF_LIBDL=
ifneq ($(findstring Linux,$(shell uname -s)),)
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

# 'My' lightning is V1 and the CPUs known by version.c except amd64.
ifneq ($(CPUFOUND),)
ifeq ($(findstring X86_64,$(CPUFOUND)),)
DO_MYLIGHT=$(wildcard $(TOOLDIR)/lightning.h)
endif
endif

ifeq ($(DO_LIGHT)$(DO_LIGHT2)$(DO_MYLIGHT),)
CONF_LIGHTNING=-DDISABLE_GNULIGHTNING
LIBS_LIGHTNING=
else
ifneq ($(DO_LIGHT2),)
CONF_LIGHTNING=
LIBS_LIGHTNING=-llightning
LIGHTNING_OBJ=bfi.gnulit.o
else
CONF_LIGHTNING=-DGNULIGHTv1
LIBS_LIGHTNING=
LIGHTNING_OBJ=bfi.gnulit.o
endif
$(TARGETFILE): $(LIGHTNING_OBJ)
OBJECTS += $(LIGHTNING_OBJ)
endif

ifneq ($(findstring X86,$(CPUFOUND)),)
DO_DYNASM=$(wildcard $(TOOLDIR)/dynasm/dynasm)

ifneq ($(DO_DYNASM),)
DYNASM_OBJ=bfi.dasm.o

OBJECTS += $(DYNASM_OBJ)
$(TARGETFILE): $(DYNASM_OBJ)
$(DYNASM_OBJ): bfi.dasm.h bfi.tree.h bfi.run.h

ifneq ($(findstring X86_64,$(CPUFOUND)),)
bfi.dasm.c: bfi.dasm.x86.dasc
	$(TOOLDIR)/dynasm/dynasm -c -o bfi.dasm.c bfi.dasm.x86.dasc
endif

ifneq ($(findstring X86_32,$(CPUFOUND)),)
bfi.dasm.c: bfi.dasm.x86.dasc
	$(TOOLDIR)/dynasm/dynasm -c -D I386 -o bfi.dasm.c bfi.dasm.x86.dasc
endif

else
CONF_DYNASM=-DDISABLE_DYNASM
endif
else
CONF_DYNASM=-DDISABLE_DYNASM
endif

################################################################################
EOFMARK := $(shell echo Configuration complete >$(LOGDEV))
