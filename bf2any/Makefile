# TARGET_ARCH=-m32

LDLIBS=

ALLEXE=\
	bf2asmjs bf2awk bf2bas bf2bash bf2bf bf2cgmp bf2clojure bf2dc \
	bf2gas32 bf2gas64 bf2gcc bf2lua bf2neko bf2pas bf2perl bf2php \
	bf2ps1 bf2py bf2rb bf2rc bf2run bf2sh bf2tcc bf2slang bf2jit

HAVE_TCC=$(wildcard /usr/include/libtcc.h)
ifeq ($(HAVE_TCC),)
TCCFLAG=-DNO_LIBTCC
TCCLIBS=
TCCDEP=bf2tcc.lib-off
else
TCCFLAG=
TCCLIBS=-ltcc -ldl
TCCDEP=bf2tcc.lib-on
endif

all:	bf2any.o bf2const.o $(ALLEXE)

clean:
	-rm -f $(ALLEXE)
	-rm -f bf2tcc.lib-off bf2tcc.lib-on bf2jit.amd64.h bf2jit.i686.h
	-rm -f *.o

CFLAGS=-O3 -Wall -DBOFF=256

bf2bf: bf2bf.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2tcc: bf2tcc.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH) $(TCCLIBS)

bf2tcc.o: bf2tcc.c $(TCCDEP)
	$(CC) $(CFLAGS) $(TCCFLAG) -c -o $@ bf2tcc.c $(TARGET_ARCH)

bf2tcc.lib-on:
	-@rm -f bf2tcc.lib-off
	@touch bf2tcc.lib-on

bf2tcc.lib-off:
	-@rm -f bf2tcc.lib-on
	@touch bf2tcc.lib-off

bf2gcc: bf2gcc.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH) -ldl

bf2lua:	bf2lua.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2perl: bf2perl.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2py: bf2py.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2dc: bf2dc.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2bash: bf2bash.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2bas: bf2bas.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2awk: bf2awk.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2run: bf2run.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2gas32: bf2gas32.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2gas64: bf2gas64.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2gas32.o: bf2gas.c
	$(CC) $(CFLAGS) -DUSE32 $(TARGET_ARCH) -c -o $@ bf2gas.c

bf2gas64.o: bf2gas.c
	$(CC) $(CFLAGS) -DUSE64 $(TARGET_ARCH) -c -o $@ bf2gas.c

bf2pas: bf2pas.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2ps1: bf2ps1.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2rb: bf2rb.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2asmjs: bf2asmjs.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2cgmp: bf2cgmp.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2clojure: bf2clojure.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2php: bf2php.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2neko: bf2neko.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2sh: bf2sh.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2rc: bf2rc.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2slang: bf2slang.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2jit: bf2jit.o bf2any.o bf2const.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2jit.o: bf2jit.amd64.h bf2jit.i686.h

bf2jit.amd64.h: bf2jit.x86.dasc
	lua ../tools/dynasm/dynasm.lua -c -o bf2jit.amd64.h bf2jit.x86.dasc

bf2jit.i686.h: bf2jit.x86.dasc
	lua ../tools/dynasm/dynasm.lua -c -D I386 -o bf2jit.i686.h bf2jit.x86.dasc

