#!/bin/bash
#
# Look for GCC-8.4
x=`which gcc-8.4 2>/dev/null`
if [ -n "$x" ]; then
    gdir=`dirname $x`
elif [ -f "/usr/gcc-8.4/bin/gcc" ]; then
    gdir="/usr/gcc-8.4/bin"
else
    echo Cannot find GCC 8.4
    exit 1
fi

echo GCC is in $gdir

export CC="$gdir/gcc"
export CPP="$gdir/cpp"
export CXX="$gdir/g++"
export PATH="$gdir:$PATH"

echo Starting subshell. Type exit to exit.

$SHELL

