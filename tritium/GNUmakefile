# This is compatible with Debian/GCC multiarch and also notices
# the environment variable TARGET_ARCH.
# You can use:
#       export TARGET_ARCH=-m32
# or
#       export TARGET_ARCH=-m64
#
# GNU lightning may be V1 or V2 and is detected; note, however, that V1 does
# NOT work on a 64bit x64. (You can try TARGET_ARCH=-m32 after installing the
# gcc-multilib package with apt-get. The arch flag -mx32 will NOT work on
# Debian kernels because kernel support is disabled. You will need to add
# "syscall.x32=y" to the kernel command line to re-enabled it.
#
# Dasm only has assembler for x86, x86_64 and x86_32 others have not yet been
# written.
# It is normally a little faster that GNU Lightning.
#
# The JIT interpreters will normally get disabled if their requirements don't
# exist but can be manually disabled too.
#
# To manually disable a 'JITter' add one or more of the options below to the
# make command line.
#
# make DO_TCC= DO_LIGHT= DO_LIGHT2= DO_MYLIGHT= DO_LIBDL1= DO_LIBDL2= DO_DYNASM=
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
# DEFS=
# LDFLAGS=

all: $(TARGETFILE)

# for: make DEFS='$(MOREDEFS)'
# Note: This turns on -fsanitize=integer and turns off -fwrapv
#       so expect at least one message.
MOREDEFS=-Wextra -Wfloat-equal -Wundef -Wpointer-arith -Wcast-align \
	 -Wstrict-prototypes -Wstrict-overflow=5 -Wwrite-strings \
	 -Waggregate-return -Wcast-qual -Wswitch-enum -Wconversion \
	 -Wunreachable-code -fno-common -Wlarger-than=512 \
	 -Wmissing-prototypes -Wold-style-definition \
	 -fsanitize=address -fsanitize=undefined \
	 -fno-wrapv -DALLOW_INSANITY

MOREGCC=$(MOREDEFS) -Wstack-usage=512 -Wunsafe-loop-optimizations -Wold-style-declaration
MORECLANG=$(MOREDEFS) -fsanitize=integer -Wpadded

OBJECTS=bfi.o bfi.version.o bfi.ccode.o bfi.nasm.o bfi.bf.o bfi.dc.o \
	bfi.dd.o bfi.runarray.o bfi.runmax.o clock.o taperam.o

CONF=-DCNF $(CONF_DYNASM) $(CONF_LIGHTNING) $(CONF_TCCLIB) $(CONF_BNLIB) $(CONF_LIBDL)
LDLIBS=$(GNUSTK) $(LIBS_LIGHTNING) $(LIBS_TCCLIB) $(LIBS_BNLIB) $(GNUDYN) $(LIBS_LIBDL)

$(TARGETFILE): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGETFILE) $(OBJECTS) $(LDLIBS) $(TARGET_ARCH)

pristine:
	$(MAKE) realclean

realclean: clean
	-rm -f bfi
ifneq ($(TARGETFILE),bfi)
	-rm -f $(TARGETFILE)
endif
# This is generated BEFORE the rules start, so "realclean all" would fail
ifeq ($(MAKECMDGOALS),realclean)
	-rm -f bfi.version.h
endif

clean:
	-rm -f *.o bfi.dasm.c .build-opts.*

install: $(TARGETFILE)
	$(INSTALL) $(TARGETFILE) $(INSTALLDIR)/$(TARGETFILE)$(INSTALLEXT)

bfi.dasm.o:	bfi.dasm.c bfi.tree.h bfi.dasm.h bfi.run.h
	$(CC) $(CFLAGS) -I $(TOOLDIR) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ bfi.dasm.c

bfi.gnulit.o:	bfi.gnulit.c bfi.tree.h bfi.gnulit.h bfi.run.h
	$(CC) $(CFLAGS) $(TYPE_LIGHTNING) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ bfi.gnulit.c

bfi.version.o: bfi.version.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ bfi.version.c

bfi.o: \
    bfi.c bfi.tree.h bfi.run.h bfi.be.def bfi.ccode.h bfi.gnulit.h \
    bfi.nasm.h bfi.bf.h bfi.dc.h bfi.dd.h clock.h ov_int.h \
    bfi.runarray.h bfi.runmax.h
bfi.bf.o: bfi.bf.c bfi.tree.h
bfi.ccode.o: bfi.ccode.c bfi.tree.h bfi.run.h bfi.ccode.h
bfi.dc.o: bfi.dc.c bfi.tree.h bfi.run.h
bfi.nasm.o: bfi.nasm.c bfi.tree.h bfi.nasm.h
bfi.dd.o: bfi.dd.c bfi.tree.h

taperam.o: bfi.tree.h bfi.run.h

clock.o: clock.h

################################################################################
# Don't configure if we're just cleaning.
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),realclean)

ifneq ($(findstring clean,$(MAKECMDGOALS)),)
# But clean must be done in sequence.
.NOTPARALLEL:
endif

################################################################################
LOGDEV=/dev/null
MARK := $(shell echo Configuration start >$(LOGDEV))
#
# Always running the version check script.
VERSTRING := $(shell sh version.sh bfi.version.h $(VERSION) $(VERSIONID))
#
CC := $(shell ( which 2>/dev/null clang gcc pcc ; echo /cc ) | sed -n '/^\// s:.*/::p' | head -1)
*/::p' | head -1)

ifneq ($(CC),cc)
OPTFLG=-O2
CFLAGS=$(OPTFLG) -Wall -Wshadow $(FUNSAFE) $(DEFS) $(CONF)
CVER := $(shell $(CC) --version 2>/dev/null)
else
OPTFLG=-O
CFLAGS=$(OPTFLG) $(DEFS) $(CONF)
CVER :=
endif
LDFLAGS=

# The CFLAGS used here does not include FUNSAFE or TOOLDIR
CPUFOUND := $(shell $(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -E -dM bfi.version.c 2>$(LOGDEV)| grep BFI_FOUND_CPU )

ifneq ($(CPUFOUND),)
# The CPUs acceptable to the tools dir are all mentioned in bfi.version.c
TOOLDIR=../tools
endif

echo = $(1)
ifeq ($(call echo,ok),)	# This checks that gmake is new enough (gmake 3.79+)
$(OBJECTS): a_newer_version_of_gnu_make
else
CALLOK := yes
endif

ifeq ($(CALLOK),yes)
################################################################################
# Current GCC policy seems to be to do any unsafe optimisation they can think
# of and let everyone else pick up the pieces. In addition v4.7 of GCC does
# it wrong, doing it's 'optimisation' on calculations that are defined by the
# C standard.
ifeq ($(CC),cc)
MARK := $(shell echo Main configuration disabled for cc >$(LOGDEV))
else
# Check for GCC options c/o the Linux kernel makefiles
TMP :=/tmp/_test-$$$$
cc-option = $(shell set -e;               \
	echo Checking $(CC) "$(1)" > $(LOGDEV); \
	if ($(CC) $(1) -c -x c /dev/null -o $(TMP) ) >$(LOGDEV) 2>&1;      \
	then echo "$(2)";               \
	else echo "$(3)";               \
	fi;                             \
	rm -f $(TMP) $(TMP).o $(TMP).c  )

WERROR := $(call cc-option,-Werror,-Werror)
UNSAFE +=$(call cc-option,$(WERROR) -Wextra,-Wextra)
UNSAFE +=$(call cc-option,$(WERROR) -fno-delete-null-pointer-checks,-fno-delete-null-pointer-checks)
UNSAFE +=$(call cc-option,$(WERROR) -Wno-null-pointer-arithmetic,-Wno-null-pointer-arithmetic)

# -fwrapv and -fno-strict-overflow may conflict (clang)
ifeq ($(call cc-option,$(WERROR) -fwrapv,ok,fail),ok)
ifeq ($(call cc-option,$(WERROR) -fwrapv -fno-strict-overflow,ok,fail),ok)
UNSAFE += -fwrapv -fno-strict-overflow
else
UNSAFE += -fwrapv
endif
else
UNSAFE += $(call cc-option,$(WERROR) -fno-strict-overflow,-fno-strict-overflow)
endif

# This appears to be an amplifier not itself unsafe.
#   -fno-aggressive-loop-optimizations

# Try to turn off the warning nicely first.
GNULGT := $(call cc-option,-Wno-unused-value,-Wno-unused-value,-w)
GNULGT += $(call cc-option,-fno-strict-aliasing,-fno-strict-aliasing)
GNULGT += $(call cc-option,-Wno-sign-compare,-Wno-sign-compare,-w)

################################################################################

try-run = $(shell set -e;               \
	echo Trying "$(1)" > $(LOGDEV); \
	if ($(1)) >$(LOGDEV) 2>&1;      \
	then echo "$(2)";               \
	else echo "$(3)";               \
	fi;                             \
	rm -f $(TMP) $(TMP).o $(TMP).c  )

################################################################################

MNATIVE := $(call try-run,\
        { echo 'int main(){return 0;}'; } > $(TMP).c ; \
	$(CC) $(TARGET_ARCH) -march=native $(TMP).c -o $(TMP),1)

ifneq ($(MNATIVE),)
# Optimise for this machine, set MARCH if you want different.
MARCH = -march=native
UNSAFE += $(MARCH)
endif

################################################################################

# Only check the flags once.
FUNSAFE := $(UNSAFE)

################################################################################

TRYCC=$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(TMP).c
TRYCC2=$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(TARGET_ARCH) $(TMP).c

# Do libtcc.h and -ltcc exist ?
DO_TCC := $(call try-run,\
	{ echo '\#include <libtcc.h>';			\
	  echo 'int main(){tcc_new();return 0;}';	\
	  } > $(TMP).c ; $(TRYCC2) -o $(TMP) -ltcc -ldl,1)

# How about lightning.h V2 ?
DO_LIGHT2 := $(call try-run,\
	{ echo '\#include <lightning.h>'; 				\
	  echo 'int main(){jit_state_t*_jit=0;jit_emit();return 0;}';	\
	  } > $(TMP).c ; $(TRYCC2) -o $(TMP) -llightning,1)

ifeq ($(DO_LIGHT2),)
# How about lightning.h V2 with a static library, needs -lm ?
DO_LIGHT3 := $(call try-run,\
	{ echo '\#include <lightning.h>'; 				\
	  echo 'int main(){jit_state_t*_jit=0;jit_emit();return 0;}';	\
	  } > $(TMP).c ; $(TRYCC2) -o $(TMP) -llightning -lm,1)

ifeq ($(DO_LIGHT3),)
# No? How about lightning.h V1 ?
DO_LIGHT := $(call try-run,\
	{ echo '\#include <lightning.h>';			\
	  echo 'int main(){char * startptr[2048];';		\
	  echo 'jit_flush_code(startptr, jit_get_ip().ptr);';	\
	  echo 'return 0;}';					\
	  } > $(TMP).c ; $(TRYCC2) -w -o $(TMP),1)

ifeq ($(DO_LIGHT),)
ifeq ($(findstring WINDOWS,$(CPUFOUND)),)
# Alright, does my copy compile ?
ifneq ($(TOOLDIR),)
DO_MYLIGHT := $(call try-run,\
	{ echo '\#include <lightning.h>';			\
	  echo 'int main(){char * startptr[2048];';		\
	  echo 'jit_flush_code(startptr, jit_get_ip().ptr);';	\
	  echo 'return 0;}';					\
	  } > $(TMP).c ; $(TRYCC2) -I $(TOOLDIR) -w -o $(TMP),1)
endif
endif
endif
endif
endif

# And shared libraries ?
ifneq ($(wildcard /usr/include/dlfcn.h),)

DO_LIBDL2 := $(call try-run,\
	{ echo '\#include <dlfcn.h>'; \
	  echo 'int main(){dlerror();return 0;}'; \
	  } > $(TMP).c ; $(TRYCC2) -o $(TMP),1)

ifeq ($(DO_LIBDL2),)
# Maybe it needs -ldl
DO_LIBDL1 := $(call try-run,\
	{ echo '\#include <dlfcn.h>'; \
	  echo 'int main(){dlerror();return 0;}'; \
	  } > $(TMP).c ; $(TRYCC2) -o $(TMP) -ldl,1)
endif
endif

# Open SSL big numbers ?
DO_BN := $(call try-run,\
	{ echo '\#include <openssl/bn.h>'; 				\
	  echo 'int main(){BIGNUM*t1=BN_new();BN_set_word(t1,1);return(0);}'; \
	  } > $(TMP).c ; $(TRYCC2) $(WERROR) -o $(TMP) -lcrypto,1)

endif	# CC
else
INSTALL=cp
GNULGT := -w
endif	# CALLOK

################################################################################

ifneq ($(STATIC_TOOLS),)
# These are used to link specified libaries statically.
# Other libraries (eg: libc) remain normally dynamic.
GNUSTK=-Wl,-Bstatic
GNUDYN=-Wl,-Bdynamic
endif

################################################################################
# Auto configure

ifeq ($(DO_LIBDL1)$(DO_LIBDL2),)
CONF_LIBDL=-DDISABLE_DLOPEN
LIBS_LIBDL=
CONF_TCCLIB=-DDISABLE_TCCLIB
LIBS_TCCLIB=
else
ifeq ($(DO_LIBDL2),)
CONF_LIBDL=
LIBS_LIBDL=-ldl
else
CONF_LIBDL=
LIBS_LIBDL=
endif

ifeq ($(DO_TCC),)
CONF_TCCLIB=-DDISABLE_TCCLIB
LIBS_TCCLIB=
else
CONF_TCCLIB=
LIBS_TCCLIB=-ltcc
endif
endif

ifeq ($(DO_LIGHT)$(DO_LIGHT2)$(DO_LIGHT3)$(DO_MYLIGHT),)
CONF_LIGHTNING=-DDISABLE_GNULIGHTNING
LIBS_LIGHTNING=
else
ifneq ($(DO_LIGHT2),)
LIBS_LIGHTNING=-llightning
TYPE_LIGHTNING=
else
ifneq ($(DO_LIGHT3),)
LIBS_LIGHTNING=-llightning -lm
TYPE_LIGHTNING=
else
LIBS_LIGHTNING=
ifneq ($(DO_LIGHT),)
TYPE_LIGHTNING=-DGNULIGHTv1 $(GNULGT)
else
TYPE_LIGHTNING=-DGNULIGHTv1 $(GNULGT) -I $(TOOLDIR)
endif
endif
endif

CONF_LIGHTNING=
LIGHTNING_OBJ=bfi.gnulit.o
$(TARGETFILE): $(LIGHTNING_OBJ)
OBJECTS += $(LIGHTNING_OBJ)
endif

ifeq ($(DO_BN),)
CONF_BNLIB=-DDISABLE_BN
LIBS_BNLIB=
else
CONF_BNLIB=
LIBS_BNLIB=-lcrypto
endif

ifneq ($(findstring X86,$(CPUFOUND)),)
DO_DYNASM=$(wildcard $(TOOLDIR)/dynasm/dynasm)

ifneq ($(DO_DYNASM),)
DYNASM_OBJ=bfi.dasm.o

OBJECTS += $(DYNASM_OBJ)
$(TARGETFILE): $(DYNASM_OBJ)

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

endif
endif

################################################################################
BUILDOPT=$(CC) $(CFLAGS) $(LDFLAGS) $(LDLIBS) $(TARGET_ARCH) $(CVER)
BUILDFLG :=.build-opts.$(shell echo '$(BUILDOPT)' | sum | tr -d '\040\055' )
$(OBJECTS) bfi.dasm.c: $(BUILDFLG)
$(BUILDFLG):
	-@rm -f .build-opts.*
	-@echo '$(BUILDOPT)' > $(BUILDFLG)
