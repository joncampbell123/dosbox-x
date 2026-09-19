#!/bin/sh
# Build and run the host-side white-box check of the VBE/AI provider.
#
# The provider is staged into a build directory next to the stub headers
# because its own quoted #includes resolve against src/ints/ first, which
# would pull in the real DOSBox-X headers and the whole tree with them.

set -e
here=$(cd "$(dirname "$0")" && pwd)
src="$here/../../../src/ints/int10_vesa_ai.cpp"
fm="$here/../../../src/ints/vbeai_fm.cpp"
fmh="$here/../../../src/ints/vbeai_fm.h"
build="$here/build"

[ -f "$src" ] || { echo "cannot find $src" >&2; exit 1; }
[ -f "$fm" ]  || { echo "cannot find $fm"  >&2; exit 1; }

rm -rf "$build"
mkdir -p "$build"
mkdir -p "$build/hardware"
cp "$here"/*.h "$here/hostcheck.cpp" "$src" "$fm" "$fmh" "$build/"
cp "$here"/hardware/*.h "$build/hardware/"

${CXX:-g++} -std=c++14 -Wall -Wextra -I"$build" -o "$build/hostcheck" "$build/hostcheck.cpp"
exec "$build/hostcheck"
