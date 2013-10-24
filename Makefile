
CFLAGS=-O3 -Wall $(DEFS)
LDFLAGS=
CC=gcc

-include make.conf
-include Makefile2

bfi:	bfi.o bfi.jit.o bfi.nasm.o bfi.bf.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o bfi $^ $(LIBS)

clean:
	-rm -f *.o bfi bf2bf make.conf

make.conf:
	sh configure
    
bfi.o:		bfi.c bfi.tree.h bfi.nasm.h bfi.bf.h
bfi.nasm.o:	bfi.nasm.c bfi.tree.h bfi.nasm.h
bfi.bf.o:	bfi.bf.c bfi.tree.h bfi.bf.h

bfi.jit.o:	bfi.jit.c bfi.tree.h bfi.jit.h
	$(CC) $(CFLAGS)   -c -fno-strict-aliasing bfi.jit.c

