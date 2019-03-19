BuildRequires: libX11 libX11-devel libXext libXext-devel libpng libpng-devel alsa-lib alsa-lib-devel ncurses ncurses-devel zlib zlib-devel mesa-libGL mesa-libGL-devel pulseaudio-libs pulseaudio-libs-devel make, libtool, gcc
Requires: libX11 libXext libpng alsa-lib ncurses zlib mesa-libGL pulseaudio-libs
Name: dosbox-x
Version: 0.82.17
Release: 0%{?dist}
Summary: @PACKAGE_DESCRIPTION@
License: GPL
URL: http://www.dosbox-x.com
Group: Applications/Emulators
Source0: dosbox-x-0.82.17.tar.xz

%description

%prep
%autosetup -n dosbox-x

%build
./build-debug-no-avcodec

%check

%install
%make_install

%files
%{_bindir}/*
%{_datadir}/dosbox-x/*

