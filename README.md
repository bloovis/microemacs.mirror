# MicroEMACS (Dave Conroy version with enhancements)

This is an enhanced version of the MicroEMACS that was
originally posted by Dave Conroy to USENET in 1986.  It
predates the very popular Daniel Lawrence version of MicroEMACS, which
is much larger.

I have added support for many new features over the years:

* etags
* scope
* ispell
* undo
* UTF-8
* regular expression search and replace
* optional PCRE2 (Perl-compatible regular expressions)
* extensions written in Ruby

In the past I ported it to
numerous operating systems, but currently this source tree supports
only Linux and Windows (using MinGW).  I seem to have lost the source
for the MS-DOS and OS/2 versions, but they are unlikely to be useful in
the future.

MicroEMACS is still very "micro", even with all the new features I've
added.  Back in the 80s, as a 16-bit MS-DOS executable, it contained
about 57K of code.  Now, as a 64-bit Linux executable, it contains
about 145K of code.  By comparison, vim-tiny contains about 1.4MB of
code, and nano contains about 262K of code.

To build a non-debug version with no Ruby support on Linux,
FreeBSD, or Windows with MinGW or Cygwin:

    mkdir obj
    cd obj
    ../configure
    make # gmake on FreeBSD

Dave Conroy released his source code into the public domain.  I have
changed my version to use the GNU General Public License Version 3.

There is a web version of the MicroEMACS manual [here](https://www.bloovis.com/meguide/).
If you are viewing this README in a Fossil repository, click
on the Docs link to view the manual.

--Mark Alexander (marka@pobox.com)
