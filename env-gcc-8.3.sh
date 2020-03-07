#!/bin/bash
#
# Look for GCC-8.3
x=`which gcc-8.3 2>/dev/null`
if [ -n "$x" ]; then
    gdir=`dirname $x`
elif [ -f "/usr/gcc-8.3/bin/gcc" ]; then
    gdir="/usr/gcc-8.3/bin"
else
    echo Cannot find GCC 8.3
    exit 1
fi

echo GCC is in $gdir

export CC="$gdir/gcc"
export CPP="$gdir/cpp"
export CXX="$gdir/g++"
export PATH="$gdir:$PATH"

echo Starting subshell. Type exit to exit.

$SHELL

