#!/bin/sh
repodir=$(cat /mingw/msys/1.0/pwd.txt)
cd "${repodir}" || exit
./"${1}"
