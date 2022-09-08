#!/usr/bin/env bash
#
# Look for GCC-4.8
x=`which gcc-4.8`
gdir=`dirname $x`

export CC="$gdir/gcc-4.8"
export CPP="$gdir/cpp-4.8"
export CXX="$gdir/g++-4.8"

echo Starting subshell. Type exit to exit.

$SHELL

