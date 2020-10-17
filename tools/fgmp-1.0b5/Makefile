#
# Makefile for Free MP
#
CC=gcc
CFLAGS=-O

libfgmp.a: gmp.o
	ar cq libfgmp.a gmp.o
	ranlib libfgmp.a

clean:
	rm -f gmp.o libfgmp.a core *~
