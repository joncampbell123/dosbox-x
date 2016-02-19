# Main Makefile for DOSBox

EXTRA_DIST = autogen.sh
SUBDIRS = src include

.PHONY: dosbox.app

dosbox.app: src/dosbox src/dosbox.icns
	rm -Rfv dosbox.app
	mkdir dosbox.app
	mkdir dosbox.app/Contents
	mkdir dosbox.app/Contents/MacOS
	mkdir dosbox.app/Contents/Resources
	cp -v src/dosbox.plist dosbox.app/Contents/Info.plist
	cp -v src/dosbox.pkginfo dosbox.app/Contents/PkgInfo
	cp -v src/dosbox.icns dosbox.app/Contents/Resources/DosBox.icns
	cp -v src/dosbox dosbox.app/Contents/MacOS/DosBox
# this is where it gets ugly
	otool -L dosbox.app/Contents/MacOS/DosBox | grep '/usr/local/lib' | while read X; do Y=`echo "$$X" | sed -E '/^ +/s///' | cut -d ' ' -f 1`; \
		dylib=`basename $$Y`; \
		cp -v $$Y dosbox.app/Contents/MacOS/; \
		install_name_tool -change $$Y @executable_path/$$dylib dosbox.app/Contents/MacOS/DosBox; \
	done

src/dosbox.icns: src/dosbox.ico
	rm -Rfv src/dosbox.iconset
	mkdir src/dosbox.iconset
	sips -z 16 16    src/dosbox.png    --out src/dosbox.iconset/icon_16x16.png
	sips -z 32 32    src/dosbox.png    --out src/dosbox.iconset/icon_16x16@2.png
	sips -z 32 32    src/dosbox.png    --out src/dosbox.iconset/icon_32x32.png
	iconutil -c icns -o src/dosbox.icns src/dosbox.iconset
	rm -Rfv src/dosbox.iconset

