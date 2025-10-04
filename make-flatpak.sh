#!/usr/bin/env bash

if [[ ! -x $(which flatpak-builder) ]]; then
	echo "Please install flatpak-builder"
	exit
fi

if [[ ! $(flatpak list|grep -c org.freedesktop.Sdk) -ge 1 ]]; then 
	echo "Please ensure the necessary SDK and matching runtime are installed by running:"
	echo "flatpak install flathub org.freedesktop.Platform//22.08 org.freedesktop.Sdk//22.08"
	exit
fi

if flatpak-builder --repo=myrepo --force-clean build-flatpak contrib/linux/com.dosbox_x.DOSBox-X.yaml
then
	echo
	echo "You can now install the flatpak by running the following commands:"
	echo
	echo " flatpak --user remote-add --no-gpg-verify myrepo myrepo"
	echo " flatpak --user install myrepo com.dosbox_x.DOSBox-X"
	echo
	echo "You can then run the flatpak as follows:"
	echo " flatpak --user run com.dosbox_x.DOSBox-X"
	echo
	echo "Or you can test it without installing by running:"
	echo " flatpak-builder --run build-flatpak contrib/linux/com.dosbox_x.DOSBox-X.yaml dosbox-x"
fi
