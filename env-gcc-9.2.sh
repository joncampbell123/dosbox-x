#!/bin/bash
#
# Look for GCC-9.2
x=`which gcc-9.2 2>/dev/null`
if [ -n "$x" ]; then
    gdir=`dirname $x`
elif [ -f "/usr/gcc-9.2/bin/gcc" ]; then
    gdir="/usr/gcc-9.2/bin"
else
    echo Cannot find GCC 9.2
    exit 1
fi

echo GCC is in $gdir

export CC="$gdir/gcc"
export CPP="$gdir/cpp"
export CXX="$gdir/g++"
export PATH="$gdir:$PATH"

echo Starting subshell. Type exit to exit.

$SHELL

