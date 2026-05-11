#!/bin/sh
exec cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$(dirname $0)"/toolchain.cmake $*
