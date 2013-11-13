
# TARGET_ARCH=-m32
CFLAGS=-O3 -Wall $(DEFS)
LDFLAGS=
CC=gcc

-include make.conf
-include Makefile2

bfi:	bfi.o bfi.jit.o bfi.nasm.o bfi.bf.o bfi.dc.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o bfi $^ $(LIBS) $(TARGET_ARCH)

clean:
	-rm -f *.o bfi bf2bf make.conf

make.conf: Makefile
	sh configure $(TARGET_ARCH)
    
bfi.o:		bfi.c bfi.tree.h bfi.nasm.h bfi.bf.h bfi.dc.h make.conf
bfi.nasm.o:	bfi.nasm.c bfi.tree.h bfi.nasm.h make.conf
bfi.bf.o:	bfi.bf.c bfi.tree.h bfi.bf.h make.conf
bfi.dc.o:	bfi.dc.c bfi.tree.h bfi.dc.h make.conf

bfi.jit.o:	bfi.jit.c bfi.tree.h bfi.jit.h make.conf
	$(CC) $(CFLAGS) $(TARGET_ARCH)  -c -fno-strict-aliasing bfi.jit.c

