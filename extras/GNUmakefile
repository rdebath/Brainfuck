
INSTALL=install -p -s
INSTALLDIR=/usr/local/bin
INSTALLEXT=
CLIST=$(filter-out cdowhile.c lightning.c,$(wildcard *.c))
EXELIST := $(patsubst %.c,%,$(CLIST))
OBJECTS := $(patsubst %.c,%.o,$(CLIST)) ook.o
ILIST=qtable.h qtable.tmp byte2byte.tmp
EXTRAS=bf1k bf0k bfas ook hydrogen16 hydrogen32 hydrogen64

ALLEXE = $(EXELIST) $(MOREEXE) $(EXTRAS) $(LIBDL_EXE) $(LIGHTNINGEXE)

all: build

clean:
	-rm -f $(OBJECTS) $(LIBOBJ) $(ILIST)
	-rm -f ook-tmp.c $(ALLEXE) $(patsubst %,%.o,$(MOREEXE))

# Install all we can make, but don't fail if some can't be built.
install:
	-@$(MAKE) -k all
	-@echo Installing: $(ALLEXE) to $(INSTALLDIR)
	-@for i in $(ALLEXE) ; do \
	if [ "$(INSTALLEXT)" != '' -a -f "$$i$(INSTALLEXT)" ] ; then \
	    $(INSTALL) "$$i$(INSTALLEXT)" $(INSTALLDIR)/$$i$(INSTALLEXT) ; \
	    rm "$$i$(INSTALLEXT)" ; \
	else \
	    $(INSTALL) $$i $(INSTALLDIR)/$$i$(INSTALLEXT) ; \
	fi ; \
	done

# Delete the direct to exe rule to make sure %.o files are used for ccache.
% : %.c

bf1k: hydrogen.c
	$(CC) -DMASK=999 $(CFLAGS) $(LDFLAGS) $(TARGET_ARCH) $^ -o $@

bf0k: hydrogen.c
	$(CC) -DMASK=256 $(CFLAGS) $(LDFLAGS) $(TARGET_ARCH) $^ -o $@

bfas: hydrogen.c
	$(CC) -DMASK=126 $(CFLAGS) $(LDFLAGS) $(TARGET_ARCH) $^ -o $@

hydrogen16: hydrogen.c
	$(CC) -DMASK=2 $(CFLAGS) $(LDFLAGS) $(TARGET_ARCH) $^ -o $@

hydrogen32: hydrogen.c
	$(CC) -DMASK=4 $(CFLAGS) $(LDFLAGS) $(TARGET_ARCH) $^ -o $@

hydrogen64: hydrogen.c
	$(CC) -DMASK=8 $(CFLAGS) $(LDFLAGS) $(TARGET_ARCH) $^ -o $@

cdowhile: cdowhile.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(TARGET_ARCH) $^ -o $@ $(LIBS_LIBDL)

ook: ook.l
	$(LEX) $(LFLAGS) -t $^ > ook-tmp.c || rm ook-tmp.c
	$(CC) $(CFLAGS) $(NOSGN) $(LDFLAGS) $(TARGET_ARCH) ook-tmp.c -o $@
	-@rm -f ook-tmp.c

lightning: lightning.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(TARGET_ARCH) $^ $(LIBS_LIGHTNING) -o $@

qtable.h: byte2byte.tmp
	./byte2byte.tmp -d > qtable.tmp
	mv qtable.tmp qtable.h

byte2byte.tmp: byte2byte.c
	$(HOSTCC) -O -o $@ $^

txtbf: txtbf.c
	$(CC) $(CFLAGS) $(QTABLEDEF) $(LDFLAGS) $(TARGET_ARCH) txtbf.c -o $@

################################################################################
ifneq ($(MAKECMDGOALS),clean)
LOGDEV=/dev/null
MARK := $(shell echo Configuration start >$(LOGDEV))
################################################################################

HOSTCC := $(shell ( which 2>/dev/null clang gcc pcc ; echo /cc ) | sed -n '/^\// s:.*/::p' | head -1)
CC := $(HOSTCC)

# LEX := $(shell ( which 2>/dev/null flex ; echo lex ) | head -1 | sed 's:.*/::')
LEX := flex

ifneq ($(CC),cc)
CFLAGS=-O2 $(FUNSAFE) -Wall -Wshadow $(DEFS)
else
CFLAGS=-O $(DEFS)
endif

################################################################################
# Check gmake version
echo = $(1)
ifeq ($(call echo,ok),) # This checks that gmake is new enough (gmake 3.79+)
$(OBJECTS): a_newer_version_of_gnu_make
else
CALLOK := yes
endif

ifeq ($(CALLOK),yes)
TMP :=/tmp/_test-$$$$
################################################################################
# Current GCC policy seems to be to do any unsafe optimisation they can think
# of and let everyone else pick up the pieces. In addition v4.7 of GCC does
# it wrong, doing it's 'optimisation' on calculations that are defined by the
# C standard.
ifneq ($(CC),cc)
# Check for GCC options c/o the Linux kernel makefiles
cc-option = $(shell set -e;               \
	echo Checking $(CC) "$(1)" > $(LOGDEV); \
	if ($(CC) $(1) -c -x c /dev/null -o $(TMP) ) >$(LOGDEV) 2>&1;      \
	then echo "$(2)";               \
	else echo "$(3)";               \
	fi;                             \
	rm -f $(TMP) $(TMP).o $(TMP).c  )

WERROR :=$(call cc-option,-Werror,-Werror)
UNSAFE +=$(call cc-option,$(WERROR) -Wextra,-Wextra)
UNSAFE +=$(call cc-option,$(WERROR) -fno-delete-null-pointer-checks,-fno-delete-null-pointer-checks)

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

ifeq ($(findstring -march,$(TARGET_ARCH))$(findstring -mcpu,$(TARGET_ARCH)),)
MARCH =$(call cc-option,$(WERROR) -march=native,-march=native)
UNSAFE += $(MARCH)
endif

# GCC complains about flex generated code.
NOSGN := $(call cc-option,$(WERROR) -Wno-sign-compare,-Wno-sign-compare)

# This appears to be an amplifier not itself unsafe.
# UNSAFE += $(call cc2-option,-fno-aggressive-loop-optimizations)

# Only check the flags once.
FUNSAFE := $(UNSAFE)

endif

################################################################################
try-run = $(shell set -e;               \
	echo Trying "$(1)" > $(LOGDEV); \
	if ($(1)) >$(LOGDEV) 2>&1;      \
	then echo "$(2)";               \
	else echo "$(3)";               \
	fi;                             \
	rm -f $(TMP) $(TMP).o $(TMP).c  )

TRYCC=$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(TMP).c
TRYCC2=$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(TARGET_ARCH) $(TMP).c

################################################################################
# Test for lightning.h V2 ?
DO_LIGHT2 := $(call try-run,\
	{ echo '\#include <lightning.h>';                               \
	  echo 'int main(){jit_state_t*_jit=0;jit_emit();return 0;}';   \
	  } > $(TMP).c ; $(TRYCC2) -o $(TMP) -llightning,1)

ifneq ($(DO_LIGHT2),)
LIBS_LIGHTNING :=-llightning
LIGHTNINGEXE := lightning
else
# As a static library it needs -lm
DO_LIGHT2 := $(call try-run,\
        { echo '\#include <lightning.h>';                               \
          echo 'int main(){jit_state_t*_jit=0;jit_emit();return 0;}';   \
          } > $(TMP).c ; $(TRYCC2) -o $(TMP) -llightning -lm,1)

ifneq ($(DO_LIGHT2),)
LIBS_LIGHTNING :=-llightning -lm
LIGHTNINGEXE := lightning
endif

endif

HAVE_GDLM := $(call try-run,\
	{ echo '\#define _GNU_SOURCE'; \
	  echo '\#include <stdio.h>'; \
	  echo 'int main (int argc, char *argv[]) {'; \
	  echo 'char *b=0; size_t r=0;'; \
	  echo 'FILE *fp=argc>1?fopen(argv[1], "r"):0;'; \
	  echo 'if(!fp || getdelim(&b,&r,33,fp)<0) perror(argv[1]);'; \
	  echo 'return 0;}'; \
	  } > $(TMP).c ; $(TRYCC2) -o $(TMP),1)

ifeq ($(HAVE_GDLM),)
CFLAGS += -include lib/getdelim.c
endif

LOW_MEM := $(call try-run,\
            [ "`ulimit -v 2>$(LOGDEV)`" = unlimited ] || \
            [ "`ulimit -v 2>$(LOGDEV)`" -lt 40960 ] ,1)

ifeq ($(LOW_MEM),)
QTABLEDEF = -DNOQTABLE
else
txtbf: qtable.h
endif

GNUC99 := $(call try-run,\
        { echo '\#include <stdio.h>'; \
          echo 'int main (int argc, char **argv)'; \
          echo '{for(int i=1; i<10;i++) printf("Hello World!");}'; \
          } > $(TMP).c ; $(TRYCC2) -std=gnu99 -o $(TMP),-std=gnu99)

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

ifeq ($(DO_LIBDL1)$(DO_LIBDL2),)
LIBDL_EXE=
LIBS_LIBDL=
else
ifeq ($(DO_LIBDL2),)
LIBDL_EXE=cdowhile
LIBS_LIBDL=-ldl
else
LIBDL_EXE=cdowhile
LIBS_LIBDL=
endif
endif

################################################################################
endif
else
# When just cleaning...
LIGHTNINGEXE := lightning
LIBDL_EXE := cdowhile
endif

################################################################################
# This rule needs to be after all the tests.
build:	$(ALLEXE)

