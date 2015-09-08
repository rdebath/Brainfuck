# The real makefile is VERY GNU make specific, so here's a POSIX makefile
# to forward the basic requests.
# But don't actually label it because that will break a really old make.

all install clean pristine $(ALLEXE):
	+@gmake $@

ALLEXE=\
	bf2jit \
	bf2asmjs bf2awk bf2bas bf2bf bf2cgmp bf2clojure bf2cmd bf2crun \
	bf2d bf2dc bf2elf bf2f90 bf2gas32 bf2gas64 bf2go bf2julia bf2ksh \
	bf2lua bf2nav bf2neko bf2pas bf2perl bf2php bf2ps1 bf2py bf2rb \
	bf2rc bf2run bf2semi bf2sh bf2slang bf2tcl bf2tsql bf2v7sh \
	bf2whitespace

