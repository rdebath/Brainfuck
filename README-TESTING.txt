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

Currently 'smoke' tested on

    PowerPC running Debian wheezy
    PowerPC running Debian squeeze
    Sparc running Debian etch

Testing notes

I currently only have DynASM assembler for x86 and x86_64, the DynASM jit
routines are safely disabled on other processors. But this still requires
lua or luajit to assemble the code for the supported processors.  To remove
this dependancy (and not compile use DynASM at all) remove or rename
the tools/dynasm directory.

