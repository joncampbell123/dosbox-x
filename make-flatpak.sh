#!/bin/sh

if ! command -v flatpak-builder >/dev/null 2>&1; then
	echo 'Please install flatpak-builder'
	exit 1
fi

if ! flatpak list | grep -q org.freedesktop.Sdk 2>&1; then 
	echo 'Please ensure the necessary SDK and matching runtime are installed by running:'
	echo 'flatpak install flathub org.freedesktop.Platform//21.08 org.freedesktop.Sdk//21.08'
	exit 1
fi

flatpak-builder --repo=myrepo --force-clean build-flatpak contrib/linux/com.dosbox_x.DOSBox-X.yaml
if [ $? -eq 0 ]; then
	cat <<-'_EOF_'
	
	You can now install the flatpak by running the following commands:
	
	 flatpak --user remote-add --no-gpg-verify myrepo myrepo
	 flatpak --user install myrepo com.dosbox_x.DOSBox-X
	
	You can then run the flatpak as follows:
	 flatpak run com.dosbox_x.DOSBox-X
	
	Or you can test it without installing by running:
	 flatpak-builder --run build-flatpak contrib/linux/com.dosbox_x.DOSBox-X.yaml dosbox-x
	_EOF_
fi
