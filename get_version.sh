#!/bin/sh

srcdir=$1
[ -n "$srcdir" ]  || exit 1
echo "#define DATE \"`date -I`\"" >rev.h
if [ -f $srcdir/.fslckout ] ; then
  fossil info | sed -n 's/checkout: *\(..........\).*/#define REV "\1"/p' >>rev.h
else
  git rev-parse HEAD  | sed -n 's/^\(.......\).*/#define REV "\1"/p' >>rev.h
fi
