#!/bin/bash
#
# Look for GCC-4.8
x=`which gcc-4.8 2>/dev/null`
if [ -n "$x" ]; then
    gdir=`dirname $x`
elif [ -f "/usr/gcc-4.8/bin/gcc" ]; then
    gdir="/usr/gcc-4.8/bin"
else
    echo Cannot find GCC 4.8
    exit 1
fi

echo GCC is in $gdir

export CC="$gdir/gcc"
export CPP="$gdir/cpp"
export CXX="$gdir/g++"
export PATH="$gdir:$PATH"

echo Starting subshell. Type exit to exit.

$SHELL

