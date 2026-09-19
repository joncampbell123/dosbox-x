#!/bin/sh
# Build vbeaiwav.exe with Open Watcom.
#
# Set WATCOM to your Open Watcom directory. On Windows the Watcom tools need
# WATCOM and INCLUDE as native paths even when the shell uses POSIX ones, so
# both forms are derived here when cygpath is available.

if [ -z "$WATCOM" ]; then
    echo "Set WATCOM to your Open Watcom installation directory." >&2
    exit 1
fi

posix="$WATCOM"
native="$WATCOM"
if command -v cygpath >/dev/null 2>&1; then
    posix=$(cygpath -u "$WATCOM")
    native=$(cygpath -w "$WATCOM")
fi

for d in binnt64 binnt binl64 binl; do
    [ -d "$posix/$d" ] && PATH="$posix/$d:$PATH"
done

WATCOM="$native"
INCLUDE="$native/h"
export PATH INCLUDE WATCOM

exec wcl -0 -ml -bcl=dos -fe=vbeaiwav.exe vbeaiwav.c
