#/usr/bin/bash
#
# dosbox-x-sdl2 should be master-sdl2 branch
# dosbox-x should be master branch
diff -N -w -u -x '.git*' -x 'autom4te.cache' -x '*.m4' -x 'build-*' -x 'merge-*' -x 'config.*' -x 'configure' -x '*.in' -x 'Makefile' -x 'depcomp' -x 'git*.sh' -x 'install-sh' -x '.deps' -r dosbox-x-sdl2 dosbox-x >dosbox-sdl2-diff.patch
echo Patch written to dosbox-sdl2-diff.patch
