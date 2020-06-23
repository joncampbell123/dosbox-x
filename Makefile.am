# Main Makefile for DOSBox

EXTRA_DIST = autogen.sh
SUBDIRS = src include

.PHONY: dosbox-x.app

dosbox-x.app: src/dosbox-x src/dosbox.icns
	rm -Rfv dosbox-x.app
	mkdir dosbox-x.app
	mkdir dosbox-x.app/Contents
	mkdir dosbox-x.app/Contents/MacOS
	mkdir dosbox-x.app/Contents/Resources
	cp -v src/dosbox.plist dosbox-x.app/Contents/Info.plist
	cp -v src/dosbox.pkginfo dosbox-x.app/Contents/PkgInfo
	cp -v src/dosbox.icns dosbox-x.app/Contents/Resources/DosBox.icns
	cp -v font/FREECG98.BMP dosbox-x.app/Contents/Resources/freecg98.bmp
	cp -v src/dosbox-x dosbox-x.app/Contents/MacOS/DosBox
# this is where it gets ugly
	otool -L dosbox-x.app/Contents/MacOS/DosBox | grep -E '/usr/local/lib|/usr/local/opt/ffmpeg/lib' | while read X; do Y=`echo "$$X" | sed -E '/^ +/s///' | cut -d ' ' -f 1`; \
		dylib=`basename $$Y`; \
		cp -v $$Y dosbox-x.app/Contents/MacOS/; \
		install_name_tool -change $$Y @executable_path/$$dylib dosbox-x.app/Contents/MacOS/DosBox; \
	done
# and the libs too
	for pass in 1 2 3 4 5; do \
		for dolib in dosbox-x.app/Contents/MacOS/*.dylib; do \
			otool -L "$$dolib" | grep '/usr/local/lib' | while read X; do Y=`echo "$$X" | sed -E '/^ +/s///' | cut -d ' ' -f 1`; \
				dylib=`basename $$Y`; \
				cp -vn $$Y dosbox-x.app/Contents/MacOS/; \
				echo "$$Y"; \
				install_name_tool -change $$Y @executable_path/$$dylib "$$dolib"; \
				install_name_tool -id @executable_path/$$dylib "$$dolib"; \
			done; \
		done; \
	done

src/dosbox.icns: src/dosbox.ico
	rm -Rfv src/dosbox.iconset
	mkdir src/dosbox.iconset
	sips -z 16 16    src/dosbox.png    --out src/dosbox.iconset/icon_16x16.png
	sips -z 32 32    src/dosbox.png    --out src/dosbox.iconset/icon_16x16@2.png
	sips -z 32 32    src/dosbox.png    --out src/dosbox.iconset/icon_32x32.png
	iconutil -c icns -o src/dosbox.icns src/dosbox.iconset
	rm -Rfv src/dosbox.iconset

dox:
	rm -Rfv doxygen
	doxygen

dox-site-update:
	if [[ !( -d ../dosbox-x-gh-pages ) ]]; then git clone -b gh-pages https://github.com/joncampbell123/dosbox-x.git ../dosbox-x-gh-pages; fi
	(cd ../dosbox-x-gh-pages && rm -Rfv doxygen)
	cp -Rf doxygen ../dosbox-x-gh-pages/
	(cd ../dosbox-x-gh-pages && git add doxygen/)
	(cd ../dosbox-x-gh-pages && git commit doxygen/)

compat-site-update:
	if [[ !( -d ../dosbox-x-gh-pages ) ]]; then git clone -b gh-pages https://github.com/joncampbell123/dosbox-x.git ../dosbox-x-gh-pages; fi
	true # ----------
	(cd /mnt/main/emu/demotest && ./make-compat-chart.pl)
	cp /mnt/main/emu/demotest/compat-chart.html ../dosbox-x-gh-pages/demoscene-compat.html
	true # ----------
	(cd /mnt/main/emu/msdostest && ./make-compat-chart.pl)
	cp /mnt/main/emu/msdostest/compat-chart.html ../dosbox-x-gh-pages/msdos-compat.html
	true # ----------
	(cd ../dosbox-x-gh-pages && git add demoscene-compat.html)
	(cd ../dosbox-x-gh-pages && git add msdos-compat.html)
	(cd ../dosbox-x-gh-pages && git commit -m 'more' {msdos,demoscene}-compat.html)

install: src/dosbox-x
	install -D -d $(DESTDIR)$(bindir)
	install -m 755 src/dosbox-x $(DESTDIR)$(bindir)
	install -D -d $(DESTDIR)$(prefix)/share/dosbox-x
	install -m 644 CHANGELOG $(DESTDIR)$(prefix)/share/dosbox-x
	install -m 644 font/FREECG98.BMP $(DESTDIR)$(prefix)/share/dosbox-x
	install -m 644 dosbox-x.reference.conf $(DESTDIR)$(prefix)/share/dosbox-x
	install -D -d $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps
	install -m 644 src/dosbox.png $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps/dosbox-x.png
	install -d $(DESTDIR)$(prefix)/share/applications
	install -m 644 dosbox-x.desktop $(DESTDIR)$(prefix)/share/applications
	install -d $(DESTDIR)$(prefix)/share/metainfo
	install -m 644 dosbox-x.appdata.xml $(DESTDIR)$(prefix)/share/metainfo
	-test -x /usr/sbin/setcap && setcap cap_net_raw=ep $(DESTDIR)$(bindir)/dosbox-x

install_strip: src/dosbox-x install
	install -m 755 -s src/dosbox-x $(DESTDIR)$(bindir)
	-test -x /usr/sbin/setcap && setcap cap_net_raw=ep $(DESTDIR)$(bindir)/dosbox-x

uninstall:
	rm -f $(DESTDIR)$(bindir)/dosbox-x
	rm -f $(DESTDIR)$(prefix)/share/dosbox-x/*
	rmdir $(DESTDIR)$(prefix)/share/dosbox-x
	rm -f $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps/dosbox-x.png
	rm -f $(DESTDIR)$(prefix)/share/applications/dosbox-x.desktop
	rm -f $(DESTDIR)$(prefix)/share/metainfo/dosbox-x.appdata.xml

