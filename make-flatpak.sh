#!/bin/bash

if [[ ! $(flatpak list|grep org.flatpak.Builder|wc -l) -ge 1 ]]; then 
	echo Please ensure the flatpak builder is installed by running:
	echo flatpak install flathub org.flatpak.Builder
	exit
fi

if [[ ! $(flatpak list|grep org.freedesktop.Sdk|wc -l) -ge 1 ]]; then 
	echo Please ensure the necessary SDK and matching runtime are installed by running:
	echo flatpak install flathub org.freedesktop.Platform//21.08 org.freedesktop.Sdk//21.08
	exit
fi

flatpak run org.flatpak.Builder --repo=myrepo --force-clean build-flatpak contrib/linux/com.dosbox_x.DOSBox-X.yaml
if [ $? -eq 0 ]; then
	echo
	echo You can now install the flatpak by running the following commands:
	echo
	echo  flatpak --user remote-add --no-gpg-verify myrepo myrepo
	echo  flatpak --user install myrepo com.dosbox_x.DOSBox-X
	echo
	echo You can then run the flatpak as follows:
	echo  flatpak run com.dosbox_x.DOSBox-X
	echo
	echo Or you can test it without installing by running:
	echo  flatpak run org.flatpak.Builder --run build-flatpak contrib/linux/com.dosbox_x.DOSBox-X.yaml dosbox-x
fi
