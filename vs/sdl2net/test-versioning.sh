#!/bin/sh
# Copyright 2022 Collabora Ltd.
# SPDX-License-Identifier: Zlib

set -eu

# Needed so sed doesn't report illegal byte sequences on macOS
export LC_CTYPE=C

ref_major=$(sed -ne 's/^#define SDL_NET_MAJOR_VERSION  *//p' SDL_net.h)
ref_minor=$(sed -ne 's/^#define SDL_NET_MINOR_VERSION  *//p' SDL_net.h)
ref_micro=$(sed -ne 's/^#define SDL_NET_PATCHLEVEL  *//p' SDL_net.h)
ref_version="${ref_major}.${ref_minor}.${ref_micro}"
ref_sdl_req=$(sed -ne 's/^SDL_VERSION=//p' configure.ac)

tests=0
failed=0

ok () {
    tests=$(( tests + 1 ))
    echo "ok - $*"
}

not_ok () {
    tests=$(( tests + 1 ))
    echo "not ok - $*"
    failed=1
}

major=$(sed -Ene 's/^m4_define\(\[MAJOR_VERSION_MACRO\], \[([0-9]*)\]\)$/\1/p' configure.ac)
minor=$(sed -Ene 's/^m4_define\(\[MINOR_VERSION_MACRO\], \[([0-9]*)\]\)$/\1/p' configure.ac)
micro=$(sed -Ene 's/^m4_define\(\[MICRO_VERSION_MACRO\], \[([0-9]*)\]\)$/\1/p' configure.ac)
version="${major}.${minor}.${micro}"

if [ "$ref_version" = "$version" ]; then
    ok "configure.ac $version"
else
    not_ok "configure.ac $version disagrees with SDL_net.h $ref_version"
fi

major=$(sed -ne 's/^MAJOR_VERSION *= *//p' Makefile.os2)
minor=$(sed -ne 's/^MINOR_VERSION *= *//p' Makefile.os2)
micro=$(sed -ne 's/^MICRO_VERSION *= *//p' Makefile.os2)
version="${major}.${minor}.${micro}"

if [ "$ref_version" = "$version" ]; then
    ok "Makefile.os2 $version"
else
    not_ok "Makefile.os2 $version disagrees with SDL_net.h $ref_version"
fi

for rcfile in version.rc VisualC/Version.rc; do
    tuple=$(sed -ne 's/^ *FILEVERSION *//p' "$rcfile" | tr -d '\r')
    ref_tuple="${ref_major},${ref_minor},${ref_micro},0"

    if [ "$ref_tuple" = "$tuple" ]; then
        ok "$rcfile FILEVERSION $tuple"
    else
        not_ok "$rcfile FILEVERSION $tuple disagrees with SDL_net.h $ref_tuple"
    fi

    tuple=$(sed -ne 's/^ *PRODUCTVERSION *//p' "$rcfile" | tr -d '\r')

    if [ "$ref_tuple" = "$tuple" ]; then
        ok "$rcfile PRODUCTVERSION $tuple"
    else
        not_ok "$rcfile PRODUCTVERSION $tuple disagrees with SDL_net.h $ref_tuple"
    fi

    tuple=$(sed -Ene 's/^ *VALUE "FileVersion", "([0-9, ]+)\\0"\r?$/\1/p' "$rcfile" | tr -d '\r')
    ref_tuple="${ref_major}, ${ref_minor}, ${ref_micro}, 0"

    if [ "$ref_tuple" = "$tuple" ]; then
        ok "$rcfile FileVersion $tuple"
    else
        not_ok "$rcfile FileVersion $tuple disagrees with SDL_net.h $ref_tuple"
    fi

    tuple=$(sed -Ene 's/^ *VALUE "ProductVersion", "([0-9, ]+)\\0"\r?$/\1/p' "$rcfile" | tr -d '\r')

    if [ "$ref_tuple" = "$tuple" ]; then
        ok "$rcfile ProductVersion $tuple"
    else
        not_ok "$rcfile ProductVersion $tuple disagrees with SDL_net.h $ref_tuple"
    fi
done

version=$(sed -Ene '/CFBundleShortVersionString/,+1 s/.*<string>(.*)<\/string>.*/\1/p' Xcode/Info-Framework.plist)

if [ "$ref_version" = "$version" ]; then
    ok "Info-Framework.plist CFBundleShortVersionString $version"
else
    not_ok "Info-Framework.plist CFBundleShortVersionString $version disagrees with SDL_net.h $ref_version"
fi

version=$(sed -Ene '/CFBundleVersion/,+1 s/.*<string>(.*)<\/string>.*/\1/p' Xcode/Info-Framework.plist)

if [ "$ref_version" = "$version" ]; then
    ok "Info-Framework.plist CFBundleVersion $version"
else
    not_ok "Info-Framework.plist CFBundleVersion $version disagrees with SDL_net.h $ref_version"
fi

# For simplicity this assumes we'll never break ABI before SDL 3.
dylib_compat=$(sed -Ene 's/.*DYLIB_COMPATIBILITY_VERSION = (.*);$/\1/p' Xcode/SDL_net.xcodeproj/project.pbxproj)
ref='1.0.0
1.0.0'

if [ "$ref" = "$dylib_compat" ]; then
    ok "project.pbxproj DYLIB_COMPATIBILITY_VERSION is consistent"
else
    not_ok "project.pbxproj DYLIB_COMPATIBILITY_VERSION is inconsistent"
fi

dylib_cur=$(sed -Ene 's/.*DYLIB_CURRENT_VERSION = (.*);$/\1/p' Xcode/SDL_net.xcodeproj/project.pbxproj)

case "$ref_minor" in
    (*[02468])
        major="$(( ref_minor * 100 + 1 ))"
        minor="$ref_micro"
        ;;
    (*)
        major="$(( ref_minor * 100 + ref_micro + 1 ))"
        minor="0"
        ;;
esac

ref="${major}.${minor}.0
${major}.${minor}.0"

if [ "$ref" = "$dylib_cur" ]; then
    ok "project.pbxproj DYLIB_CURRENT_VERSION is consistent"
else
    not_ok "project.pbxproj DYLIB_CURRENT_VERSION is inconsistent, expected $ref"
fi

major=$(sed -ne 's/^set(MAJOR_VERSION \([0-9]*\))$/\1/p' CMakeLists.txt)
minor=$(sed -ne 's/^set(MINOR_VERSION \([0-9]*\))$/\1/p' CMakeLists.txt)
micro=$(sed -ne 's/^set(MICRO_VERSION \([0-9]*\))$/\1/p' CMakeLists.txt)
sdl_req=$(sed -ne 's/^set(SDL_REQUIRED_VERSION \([0-9.]*\))$/\1/p' CMakeLists.txt)
dylib_cmake=$(sed -ne 's/^set(DYLIB_COMPATIBILITY_VERSION "\([0-9]*\)")$/\1/p' CMakeLists.txt)
ref='1'
version="${major}.${minor}.${micro}"

if [ "$ref_version" = "$version" ]; then
    ok "CMakeLists.txt $version"
else
    not_ok "CMakeLists.txt $version disagrees with SDL_image.h $ref_version"
fi

if [ "$ref_sdl_req" = "$sdl_req" ]; then
    ok "CMakeLists.txt SDL_REQUIRED_VERSION is consistent"
else
    not_ok "CMakeLists.txt SDL_REQUIRED_VERSION=$sdl_req disagrees with configure.ac SDL_VERSION=$ref_sdl_req"
fi

if [ "$ref" = "$dylib_cmake" ]; then
    ok "CMakeLists.txt DYLIB_COMPATIBILITY_VERSION is consistent"
else
    not_ok "CMakeLists.txt DYLIB_COMPATIBILITY_VERSION is inconsistent"
fi

sdl_req=$(sed -ne 's/\$sdl2_version = "\([0-9.]*\)"$/\1/p' .github/fetch_sdl_vc.ps1)

if [ "$ref_sdl_req" = "$sdl_req" ]; then
    ok ".github/fetch_sdl_vc.ps1 $sdl_req"
else
    not_ok ".github/fetch_sdl_vc.ps1 sdl2_version=$sdl_req disagrees with configure.ac SDL_VERSION=$ref_sdl_req"
fi

echo "1..$tests"
exit "$failed"
