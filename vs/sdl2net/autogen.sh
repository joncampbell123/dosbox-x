#!/bin/sh

set -e

# Initialize libtool
if command -v libtoolize >/dev/null 2>&1; then
    libtoolize --force --copy
elif command -v glibtoolize >/dev/null 2>&1; then
    glibtoolize --force --copy
else
    echo "libtoolize not found"
    exit 1
fi

"${ACLOCAL:-aclocal}" -I acinclude
"${AUTOMAKE:-automake}" --foreign --include-deps --add-missing --copy
"${AUTOCONF:-autoconf}"

echo "Now you are ready to run ./configure"