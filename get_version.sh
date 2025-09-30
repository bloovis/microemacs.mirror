#!/bin/sh

# set -x

# Helper function.
available() { command -v "${1:?}" >/dev/null; }

srcdir=$1
[ -n "$srcdir" ]  || exit 1
echo "#define DATE \"`date -I`\"" >rev.h
if [ -f $srcdir/.fslckout ] ; then
  fossil info | sed -n 's/checkout: *\(..........\).*/#define REV "fossil-\1"/p' >>rev.h
elif available git ; then
  git rev-parse HEAD  | sed -n 's/^\(.......\).*/#define REV "git-\1"/p' >>rev.h
else
  echo "#define REV \"unknown\"" >>rev.h
fi
