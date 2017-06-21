#
# This little makefile compiles the subprojects and installs their binaries
# into the bin directory.

all:    sub_tritium sub_bf2any sub_extras

clean:
	+-@$(MAKE) -C tritium pristine TARGETFILE=tritium
	+-@$(MAKE) -C bf2any clean
	+-@$(MAKE) -C extras clean
	+-@rm -rf bin

.PHONY:  sub_tritium sub_bf2any sub_extras bin

sub_tritium: bin
	@$(MAKE) -C tritium install TARGETFILE=tritium INSTALLDIR=../bin
	@ln -fs tritium bin/bfi
	@ln -fs tritium bin/bf

sub_bf2any: bin
	@$(MAKE) -C bf2any install INSTALLDIR=../bin

sub_extras: bin
	@$(MAKE) -C extras install INSTALLDIR=../bin

bin:
	@mkdir -p bin
