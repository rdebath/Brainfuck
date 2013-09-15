
CFLAGS=-O3 -Wall
CC=gcc

all:	bfi neutron hydrogen bf2bf16 bf bf16 bf32

bfi:	bfi.o bfi.nasm.o
	$(CC) $(CFLAGS) -o bfi $^ $(LDFLAGS) -ltcc -ldl

bfi.o:		bfi.c bfi.tree.h bfi.nasm.h
bfi.nasm.o:	bfi.nasm.c bfi.tree.h bfi.nasm.h

neutron: neutron.c
bf:	bf.c

bf32:	bf.c
	$(CC) -DM=-1 $(CFLAGS) -o bf32 bf.c $(LDFLAGS)

bf16:	bf.c
	$(CC) -DM=0xFFFF $(CFLAGS) -o bf16 bf.c $(LDFLAGS)

save:
	zip -o old/bfi-`date +%Y%m%d%H`.zip Makefile bfi*.[ch]
	cp -p bf.c old/bf-`date +%Y%m%d%H`.c
