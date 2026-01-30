# Building a MicroEMACS

All versions of MicroEMACS are built from the two sets of
source files. One set is independent of operating system and terminal,
and the other set is dependent.

Compile time options for the independent modules
are selected by setting compilation
switches in `def.h`, and then letting conditional compilation do the
right thing.

The dependent modules are briefly described below.

## Operating System

MicroEMACS runs on several operating systems, including Linux,
BSD, and Windows.  Support code for other operating systems has been lost
(in the distant past, these included CP/M-86 and MS-DOS on the DEC Rainbow,
VMS on the VAX, CP/M-68K, GEMDOS, and FlexOS V60/68K/286).
The following modules contain code dependencies on the operating system:

* `ttyio.c` - low level terminal I/O; independent of terminal type.

* `spawn.c` - subjob creation.

* `fileio.c` - low level file handling.

* `bcopy.s` or `bcopy.asm` - fast byte copy and fill functions.

Adding a new operating system consists mostly of changing these
files, and the header file `sysdef.h`.

## Terminal Support

On Unix-like operating systems, MicroEMACS uses ncurses for terminal support.
On Windows, it uses the native Windows text console API.  In the past, it also
supported the OS/2 text console API, and writing directly to the screen on MS-DOS.
In all cases, MicroEMACS treats the terminal as a memory-mapped character display.
It no longer supports termcap or terminfo, except indirectly via ncurses.

The following modules contain code dependencies on the terminal type:

* `tty.c` - high-level terminal support.

* `ttyio.c` - low-level terminal support.

* `ttykbd.c` - keyboard dependencies and extensions.

Changing terminal type consists mostly of changing these files, and the header file `ttydef.h`
These files are located in separate per-terminal subdirectories of the `tty` directory.

To support a new memory-mapped display, you must provide a `putline` function
for writing lines to the display.  On old DOS-base systems, this code
was written in assembly language, but on modern terminals it is
written in C and placed in `tty.c`.

## Building with GCC

To build MicroEMACs on Linux, BSD, or Windows using Cygwin
or MinGW, use these commands:

    mkdir obj
    cd obj
    ../configure
    make # gmake on BSD

You can supply one or more optional parameters to the `configure` command:

`--enable-debug`

Use this option to compile and build MicroEMACS with debugging information, so that
it can be debugged with gdb.

`--with-ruby`

Use this option to build support for Ruby extensions into MicroEMACS.
This option will not work on Windows.

To get this option to work on BSD, provide a version parameter.
The parameter should be the first two digits of the version printed by
`ruby --version`.  For example, if the Ruby version is 3.2.9,
use this option: `--with-ruby=3.2` .


See the [Ruby Extensions](ruby.md) section for more information.

`--with-pcre2`

Use this option to use the PCRE2 library for Perl-compatible regular expressions,
instead of the default Henry Spencer (circa 1986) regular expressions.

