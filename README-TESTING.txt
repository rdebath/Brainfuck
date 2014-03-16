These programs have been tested on non-x86 machines (emulated) thanks to the
hardworking people of the qemu and Debian projects. In combination they have
made a set of environments that are both functional and very easy to setup.

    https://wiki.debian.org/QEMU

    http://people.debian.org/~aurel32/qemu/

Full testing (home environment)
    i686 running Debian wheezy
    x86_64 running Debian wheezy

Full testing
    i686 running Debian jessy
    x86_64 running Debian jessy
    x86_64 running FreeBSD

Currently 'smoke' tested on

    PowerPC running Debian wheezy
    PowerPC running Debian squeeze
    Sparc running Debian etch
    x86_64 running NetBSD
    i686 running Debian lenny

Testing notes
-------------

I currently only have DynASM assembler for x86 and x86_64, the DynASM jit
routines are safely disabled on other processors. But this still requires
lua or luajit to assemble the code for the supported processors. If a
suitable lua is not installed 'minilua.c' from the luajit distribution
will be compiled and used. If this does not compile remove or rename
the tools/dynasm directory to disable DynASM completely.

FreeBSD x86_64
Use the ports functionality to install gmake or compile manually as the
BSD 'pmake' make version's extensions are incompatible with the gmake
extensions. Ports will also be needed for the gmp library to compile
the output of bf2cgmp. The nasm assembler is in ports but the programs
produce 32bit Linux ELF executables that won't work anyway. The Makefiles
prefer gcc, but adding 'CC=clang' works fine.

