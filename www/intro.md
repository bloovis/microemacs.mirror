# Introduction

(**Note**: this document was originally written around 1986 by
either Dave Conroy, the original author of MicroEMACS, or Brian Straight; my memory
of its exact origin is not clear.  I have updated it since then,
but it still retains historical information about computers and
terminals no longer in use.)

This document describes MicroEMACS, a
public-domain[^1] display editor, loosely based on EMACS, that runs on little
computers.
It is intended as a reference manual
for users already familiar with EMACS.

[^1]: The source is now copylefted with the GPL.

We call it Micro *EMACS* to emphasize the fact that
most of the commands are very similar to, if not identical to,
fundamental mode EMACS commands (actually, it implements a set of
rebindings used by a group of users at Digital Equipment
Corporation[^2]).

[^2]: Further modified by me at Digital Research in the late 80s to more closely resemble, but not completely emulate, MINCE..

We call it *Micro* EMACS to emphasize the fact that it is
but a shadow of full EMACS. No attempt has been made to make MicroEMACS
wildly
customizable (without writing code), or to have extensive online documentation.
All of this sophistication was thrown away right at the start, because
it was important that MicroEMACS run on little computers.
In all fairness, is should be stated here and now that the most popular
small computer these days is the MicroVAX[^3]!

[^3]: This appears to be a reference by Conroy to DEC's  attempt to ignore the IBM PC.

## History

MicroEMACS is loosely based on the EMACS display editor written by
Richard Stallman at MIT.  The MicroEMACS described by this document is
Conroy's version 30 of February 1986, distributed to USENET
mod.sources.  Since then it has undergone a fair number of bug fixes
and performance improvements, and a few minor enhancements.

This version of MicroEMACS is not to be confused with other popular
versions that are derived from Conroy's 1986 release, including
the once-popular Daniel Lawrence version, and the Linus Torvalds version.

### 2018 Update

I have lost the original source of Conroy's version of MicroEMACS (including the TeX version
of this document), and I cannot find it on Google's USENET archive.
I have also lost my DOS and OS/2 source
code, though the Windows code (in the `nt` subdirectory) still exists.
This document contains many reference to historical machines and operating
systems, but I have kept them for historical interest.  I maintain
only the Linux version now.

In the 80s, MicroEMACS was small enough to run easily from a floppy disk, but the amount of text that
could be edited was limited by the very small amount of available RAM
(640kb on PCs running MS-DOS).  Nowadays this limit is effectively non-existent, give the huge amount of memory
found in modern computers, but MicroEMACS is still small enough to be run from a floppy disk
(if one could be found); its code size is about the same as `/bin/ls` on a current 64-bit
Linux distribution.

