# MicroEMACS (Dave Conroy version with enhancements)

(*Note*: If you are reading this on Github, you can find the
original Fossil repository [here](https://www.bloovis.com/fossil/home/marka/fossils/pe/home)).

This is an enhanced version of the MicroEMACS that was
originally posted by Dave Conroy to USENET in 1986.  It
predates the very popular Daniel Lawrence version of MicroEMACS, which
is much larger.

I have added support for many new features over the years:

* etags
* cscope
* ispell
* undo
* UTF-8
* regular expression search and replace
* optional PCRE2 (Perl-compatible regular expressions)
* extensions written in Ruby

In the past I ported it to
numerous operating systems, but currently this source tree supports
only Linux, Android (using termux), OpenBSD, FreeBSD, and Windows (using MinGW).  I seem to have lost the source
for the MS-DOS and OS/2 versions, but they are unlikely to be useful in
the future.

MicroEMACS is still very "micro", even with all the new features I've
added.  Back in the 80s, as a 16-bit MS-DOS executable, it contained
about 57K of code.  Now, as a 64-bit Linux executable with all of the
new features mentioned above, it contains about 110K of code.  By
comparison, vim-tiny contains about 1.4MB of code, and nano contains
about 262K of code.

To build a non-debug version with no Ruby support on Linux, Android (termux),
FreeBSD, or Windows with MinGW or Cygwin:

    mkdir obj
    cd obj
    ../configure
    make # gmake on FreeBSD or OpenBSD

Dave Conroy released his source code into the public domain.  I have
changed my version to use the GNU General Public License Version 3.

There is a web version of the MicroEMACS manual [here](https://www.bloovis.com/meguide/).
If you are viewing this README in a Fossil repository, click
on the Docs link above (or click [here](../trunk/www/index.md)) to view the manual.


To clone this repository:

```
fossil clone https://www.bloovis.com/fossil/home/marka/fossils/pe
```

--Mark Alexander (marka@pobox.com)
