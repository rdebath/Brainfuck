# TARGET_ARCH=-m32

LDLIBS=
TCCLIBS=-ltcc -ldl

ALLEXE= bf2bf bf2run \
	bf2tcc bf2as32 bf2as64 bf2lua bf2perl bf2py bf2dc bf2bash \
	bf2bas bf2awk bf2pas bf2cfish bf2ps1 bf2rb bf2asmjs bf2cgmp \
	bf2clojure bf2php bf2neko

all:	$(ALLEXE)

clean:
	-rm -f $(ALLEXE) *.o

CFLAGS=-O3 -Wall -DBOFF=256

bf2bf: bf2any.o bf2bf.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2tcc: bf2tcc.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH) $(TCCLIBS)

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

bf2as32: bf2as32.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2as64: bf2as64.o bf2any.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(TARGET_ARCH)

bf2as32.o: bf2gas.c
	$(CC) $(CFLAGS) -DUSE32 $(TARGET_ARCH) -c -o $@ bf2gas.c

bf2as64.o: bf2gas.c
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



