These programs have been tested on non-x86 machines (emulated) thanks to the
hardworking people of the qemu and Debian projects. In combination they have
made a set of environments that are both functional and very easy to setup.

    https://wiki.debian.org/QEMU

    http://people.debian.org/~aurel32/qemu/

Full testing (home environment)
    i686 running Debian jessie
    x86_64 running Debian jessie

Full testing
    i686 running Debian wheezy
    x86_64 running Debian wheezy
    x86_64 running FreeBSD

Currently 'smoke' tested on

    PowerPC running Debian wheezy
    PowerPC running Debian squeeze
    Sparc running Debian etch
    x86_64 running NetBSD
    x86_64 running OpenBSD
    x86_64 running macosx
    x86_32 running Debian
    i686 running Debian lenny
    i686 running Debian etch
    i686 running Debian woody (GNU make is too old)
    i686 running Debian bo (GNU make is too old)
    i686 running Minux
    armhf running Debian wheezy
    mips32 running Debian wheezy
    sh4 running Debian
    arm nslu2 running Debian woody
    arm nslu2 running Debian jessie
    Windows 32bit and 64bit.

Testing notes
-------------

I currently only have DynASM assembler for x86 and x86_64, the DynASM jit
routines are safely disabled on other processors.  A version of lua or
luajit is needed to assemble the code for the supported processors. If a
suitable lua is not installed 'minilua.c' from the luajit distribution
will be compiled and used. If this does not compile you may remove or
rename the tools/dynasm directory to disable DynASM completely.

FreeBSD x86_64 and others
-------------------------

Use the ports functionality to install gmake or compile manually as
the BSD 'pmake' make version's extensions are incompatible with the
gmake extensions. Ports will also be needed for the gmp library to
compile the output of bf2cgmp.  The the nasm assembler output in link
mode compiles and runs successfully when linked to libc by clang. The
'gas' assembler output does not.

The 'ports' version of GNU Lightning V2 is (of course) not auto detected
by Tritium.  It can be pulled in using this ...

$ export LIBRARY_PATH=/usr/local/lib
$ export CPATH=/usr/local/include

If these environment variables aren't used by your tools you can add
the changes to the command line:

$ make LDFLAGS=-L/usr/local/lib DEFS=-I/usr/local/include

Windows
-------
Cross compile of Tritium from Debian Linux using:
    make CC=i686-w64-mingw32-gcc
or
    make CC=x86_64-w64-mingw32-gcc

The Dynasm JIT works so it's nice and quick. (cmd.exe, however, is slow!)
The nasm/gas output cross compiles and runs with the -fwin32 option.

The majority of the bf2any cross compile to Windows happily as they're
plain C, but the special features, embedded interpreters etc don't
usually work on non-Posix operating systems.
