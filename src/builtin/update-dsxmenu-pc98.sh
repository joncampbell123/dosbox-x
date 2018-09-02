#!/bin/bash
(cd /usr/src/doslib/tool/dsxmenu && ./make.sh) || exit 1

filename="DSXMENU.EXE"
bfb_sym="bfb_DSXMENU_EXE_PC98"
src_sym="_usr_src_doslib_tool_dsxmenu_d9886s_dsxmenu_exe"
dst_sym="bin_dsxmenu_exe_pc98"
cppfile="dsxmenu_exe_pc98.cpp"

cat >$cppfile <<_EOF

#include "dos_inc.h"

_EOF
xxd -i /usr/src/doslib/tool/dsxmenu/d9886s/dsxmenu.exe | sed -e 's/unsigned char/static const unsigned char/' | sed -e 's/unsigned int/static const unsigned int/' | sed -e "s/$src_sym/$dst_sym/" | grep -v "$dst_sym"_len >>$cppfile || exit 1

cat >>$cppfile <<_EOF

struct BuiltinFileBlob $bfb_sym = {
    /*recommended file name*/	"$filename",
	/*data*/			$dst_sym,
	/*length*/			sizeof($dst_sym)
};

_EOF

