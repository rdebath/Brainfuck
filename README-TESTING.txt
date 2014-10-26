These programs have been tested on non-x86 machines (emulated) thanks to the
hardworking people of the qemu and Debian projects. In combination they have
made a set of environments that are both functional and very easy to setup.

    https://wiki.debian.org/QEMU

    http://people.debian.org/~aurel32/qemu/

Full testing (home environment)
    i686 running Debian wheezy
    x86_64 running Debian wheezy

Full testing
    i686 running Debian jessie
    x86_64 running Debian jessie
    x86_64 running FreeBSD

Currently 'smoke' tested on

    PowerPC running Debian wheezy
    PowerPC running Debian squeeze
    Sparc running Debian etch
    x86_64 running NetBSD
    i686 running Debian lenny
    i686 running Debian etch
    i686 running Debian woody (Use make CC=cc)
    Windows i386 and amd64.

Testing notes
-------------

I currently only have DynASM assembler for x86 and x86_64, the DynASM jit
routines are safely disabled on other processors. But this still requires
lua or luajit to assemble the code for the supported processors. If a
suitable lua is not installed 'minilua.c' from the luajit distribution
will be compiled and used. If this does not compile remove or rename
the tools/dynasm directory to disable DynASM completely.

FreeBSD x86_64
--------------
Use the ports functionality to install gmake or compile manually as the
BSD 'pmake' make version's extensions are incompatible with the gmake
extensions. Ports will also be needed for the gmp library to compile
the output of bf2cgmp. The nasm assembler is in ports but the programs
produce 32bit Linux ELF executables that won't work anyway. The Makefiles
prefer gcc, but adding 'CC=clang' works fine.

The 'ports' version of GNU Lightning V2 is (of course) not auto detected
by Tritium.  It can be pulled in using this ...

$ gmake CC=clang DO_LIGHT=1 DO_LIGHT2=1 LDFLAGS=-L/usr/local/lib DEFS=-I/usr/local/include

Windows
-------
Cross compile of Tritium from Debian Wheezy (stable) Linux using:
    make CC=i686-w64-mingw32-gcc DO_LIBDL=
or
    make CC=x86_64-w64-mingw32-gcc DO_LIBDL=

The Dynasm JIT works so it's nice and quick. (cmd.exe, however, is slow!)

The majority of the bf2any cross compile to Windows happily as they're
plain C, but most of the special ones haven't been altered to run on
non-Posix operating systems.
