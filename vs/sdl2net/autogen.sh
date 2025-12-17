#!/bin/sh

set -e

"${ACLOCAL:-aclocal}" -I acinclude
"${AUTOMAKE:-automake}" --foreign --include-deps --add-missing --copy
"${AUTOCONF:-autoconf}"

echo "Now you are ready to run ./configure"
