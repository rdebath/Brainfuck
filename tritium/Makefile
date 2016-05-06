# The real makefile is VERY GNU make specific, so here's a POSIX makefile
# to forward the basic requests.
# But don't actually label it because that will break a really old make.

# If you don't have GNU make you can compile with a simple command:
#     cc -O -o bfi *.c
# 
# but without any of the big options.
# 
# To do a more complex manual build you need:
# 
# cc -DCNF -DVERSION='VS_S(Manual Build)' -o bfi *.c
# 
# Plus maybe one or more of these
#     -DDISABLE_DYNASM
#     -DDISABLE_TCCLIB
#     -DDISABLE_GNULIGHTNING
#     -DDISABLE_DLOPEN
#
# For example:
#
# cc -DCNF -DVERSION='VS_S(Manual Build)' -DDISABLE_DYNASM -o bfi *.c -llightning -ltcc -ldl
#
# The DynASM is the most complex to build manually; see the GNU Makefile.
# The other options should work fine given the necessary libraries,
# which will have to be added to the command line.
#
# Note GNU Lightning V1 needs to be flagged with -DGNULIGHTv1
#

all install clean pristine:
	+@gmake $@
