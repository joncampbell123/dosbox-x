#!/usr/bin/env bash
#
# Look for GCC-4.8
x=$(which gcc-4.8 2>/dev/null)
if [ -n "${x}" ]; then
    gdir=$(dirname "${x}")
elif [ -f "/usr/gcc-4.8/bin/gcc" ]; then
    gdir="/usr/gcc-4.8/bin"
else
    echo "Cannot find GCC 4.8"
    exit 1
fi

echo "GCC is in ${gdir}"

CC="${gdir}/gcc"
CPP="${gdir}/cpp"
CXX="${gdir}/g++"
PATH="${gdir}:${PATH}"
export CC CPP CXX PATH

echo "Starting subshell. Type exit to exit."

${SHELL}
