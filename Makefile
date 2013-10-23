
CFLAGS=-O3 -Wall -DTCC0925 $(DEFS)
CC=gcc

bfi:	bfi.o bfi.jit.o bfi.nasm.o bfi.bf.o
	$(CC) $(CFLAGS) -o bfi $^ $(LDFLAGS) -ltcc -ldl

all:	bfi neutron hydrogen \
	bf2bf \
	bf.bin/bf bf.bin/bf32 bf.bin/bf16 bf.bin/bf.tcc \
	bf.bin/bfi.tcc \
	macro/macro

clean:
	-rm -f *.o bfi bf2bf
    
bfi.o:		bfi.c bfi.tree.h bfi.nasm.h bfi.bf.h
bfi.nasm.o:	bfi.nasm.c bfi.tree.h bfi.nasm.h
bfi.bf.o:	bfi.bf.c bfi.tree.h bfi.bf.h
bfi.jit.o:	bfi.jit.c bfi.tree.h bfi.jit.h
	$(CC) $(CFLAGS)   -c -fno-strict-aliasing bfi.jit.c

hydrogen: bf.bin/proton bf.bin/protium bf.bin/deuterium bf.bin/tritium
	ln -fs bf.bin/tritium hydrogen

bf.bin/proton: hydrogen.c
	$(CC) -DMASK=1 -DNO_XTRA $(CFLAGS) -o $@ $^ $(LDFLAGS)

bf.bin/protium: hydrogen.c
	$(CC) -DMASK=1 $(CFLAGS) -o $@ $^ $(LDFLAGS)

bf.bin/deuterium: hydrogen.c
	$(CC) -DMASK=2 $(CFLAGS) -o $@ $^ $(LDFLAGS)

bf.bin/tritium: hydrogen.c
	$(CC) -DMASK=4 $(CFLAGS) -o $@ $^ $(LDFLAGS)

neutron: neutron.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	cp -p neutron bf.bin/.

bf.bin/bf:	bf.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

bf.bin/bf32:	bf.c
	$(CC) -DM=-1 $(CFLAGS) -o $@ $^ $(LDFLAGS)

bf.bin/bf16:	bf.c
	$(CC) -DM=2 $(CFLAGS) -o $@ $^ $(LDFLAGS)

bf.bin/bf.tcc:	bf.c
	tcc -DM=2 $(CFLAGS) -o $@ $^ $(LDFLAGS)

bf.bin/bfi.tcc: bfi.c bfi.tree.h
	tcc -DNO_EXT_OUT -o $@ bfi.c -ltcc -ldl

save:	old/bf.c old/hydrogen.c
	zip -o old/bfi-`date +%Y%m%d%H`.zip Makefile bfi*.[ch] configure

old/bf.c:   bf.c
	cp -p bf.c old/bf.c
	cp -p bf.c old/bf-`date +%Y%m%d%H`.c

old/hydrogen.c:   hydrogen.c
	cp -p hydrogen.c old/hydrogen.c
	cp -p hydrogen.c old/hydrogen-`date +%Y%m%d%H`.c

macro/macro: macro/macro.c
	make -C macro
