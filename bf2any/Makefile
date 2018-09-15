# The real makefile is VERY GNU make specific, so here's a POSIX makefile
# to forward the basic requests.
# But don't actually label it because that will break a really old make.

all install clean pristine bf2jit $(ALLEXE):
	+@gmake $@

ALLEXE=\
	bf2ada bf2asmjs bf2awk bf2bas bf2bf bf2bn bf2cgmp bf2clojure \
	bf2cmd bf2cobol bf2crun bf2cs bf2csh bf2d bf2dc bf2dump bf2elf \
	bf2f90 bf2gas32 bf2gas64 bf2go bf2java bf2julia bf2ksh bf2ksh88 \
	bf2lua bf2nav bf2neko bf2oldsh bf2pas bf2perl bf2php bf2ps1 \
	bf2py bf2rb bf2rc bf2semi bf2sh bf2slang bf2swift bf2tcl bf2tomm \
	bf2tsql bf2whitespace
