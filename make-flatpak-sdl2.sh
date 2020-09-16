#!/bin/sh

# flatpak build will fail immediately if it cannot find the appdata.xml file
# But we have a dosbox-x.appdata.xml.in file that first needs processing by autotools
./autogen.sh
./configure

cd contrib/linux
flatpak-builder --repo=../../repo --force-clean ../../build-flatpak com.dosbox_x.DOSBox-X-sdl2.yaml
if [ $? -eq 0 ]; then
	echo If the build was successful, you can now install the flatpak by running the following commands:
	echo
	echo flatpak --user remote-add --no-gpg-verify myrepo repo
	echo flatpak --user install myrepo com.dosbox_x.DOSBox-X-sdl2
	echo
	echo Or you can test it without installing by running:
	echo flatpak-builder --run build-flatpak contrib/linux/com.dosbox_x.DOSBox-X-sdl2.yaml dosbox-x-sdl2
fi

cd ../..
