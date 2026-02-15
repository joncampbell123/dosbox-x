#!/bin/sh

# Run this script from the root of $srcdir to touch the
# autotools generated files, so that the build procedure
# doesn't attempt to regenerate them.

touch aclocal.m4 configure Makefile.in
