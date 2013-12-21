# TARGET_ARCH=-m32

LDLIBS=

EXE1=bf2bf bf2run
EXER=\
	bf2gas32 bf2gas64 bf2asmjs bf2awk bf2bas bf2bash bf2cfish \
	bf2cgmp bf2clojure bf2dc bf2lua bf2neko bf2pas bf2perl \
	bf2php bf2ps1 bf2py bf2rb bf2tcc bf2gcc bf2sh

HAVE_TCC=$(wildcard /usr/include/libtcc.h)
ifeq ($(HAVE_TCC),)
TCCFLAG=-DNO_LIBTCC
TCCLIBS=
else
TCCFLAG=
TCCLIBS=-ltcc -ldl
endif

ALLEXE= $(EXE1) $(EXER)

all:	bf2any.o $(ALLEXE)

clean:
	-rm -f $(EXE1) $(EXER) *.o

CFLAGS=-O3 -Wall -DBOFF=256

bf2bf: bf2bf.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2tcc: bf2tcc.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH) $(TCCLIBS)

bf2tcc.o: bf2tcc.c
	$(CC) $(CFLAGS) $(TCCFLAG) -c -o $@ $^ $(TARGET_ARCH)

bf2gcc: bf2gcc.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH) -ldl

bf2lua:	bf2lua.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2perl: bf2perl.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2py: bf2py.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2dc: bf2dc.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2bash: bf2bash.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2bas: bf2bas.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2awk: bf2awk.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2run: bf2run.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2gas32: bf2gas32.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2gas64: bf2gas64.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2gas32.o: bf2gas.c
	$(CC) $(CFLAGS) -DUSE32 $(TARGET_ARCH) -c -o $@ bf2gas.c

bf2gas64.o: bf2gas.c
	$(CC) $(CFLAGS) -DUSE64 $(TARGET_ARCH) -c -o $@ bf2gas.c

bf2pas: bf2pas.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2cfish: bf2cfish.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2ps1: bf2ps1.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2rb: bf2rb.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2asmjs: bf2asmjs.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2cgmp: bf2cgmp.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2clojure: bf2clojure.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2php: bf2php.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2neko: bf2neko.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2sh: bf2sh.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

