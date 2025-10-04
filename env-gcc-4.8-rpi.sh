#!/usr/bin/env bash
#
# Look for GCC-4.8
x=$(which gcc-4.8)
gdir=$(dirname "${x}")

CC="${gdir}/gcc-4.8"
CPP="${gdir}/cpp-4.8"
CXX="${gdir}/g++-4.8"
export CC CPP CXX

echo "Starting subshell. Type exit to exit."

${SHELL}
