#!/bin/bash
tar="dosbox-x-0.82.3.tar.xz"

tar -cvf "$tar" --exclude=\*.git --exclude=\*.tar.gz --exclude=\*.o -C .. dosbox-x || exit 1
cp -vf "$tar" ~/rpmbuild/SOURCES/ || exit 1
rpmbuild -ba dosbox-x.spec || exit 1
rm -v "$tar" || exit 1

