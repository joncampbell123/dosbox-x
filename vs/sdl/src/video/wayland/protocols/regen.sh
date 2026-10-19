#!/bin/sh
# DOSBox-X: regenerate the Wayland protocol code for the SDL 1.x Wayland driver.
# The generated files are kept in the source tree so that building SDL does
# not need wayland-scanner. The XML files are copies from wayland-protocols.
cd "`dirname "$0"`" || exit 1
for xml in *.xml; do
    name=`basename "$xml" .xml`
    wayland-scanner client-header "$xml" "$name-client-protocol.h" || exit 1
    wayland-scanner private-code "$xml" "$name-protocol.c" || exit 1
done
