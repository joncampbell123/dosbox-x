#!/bin/bash
(cd /usr/src/doslib/tool/dsxmenu && ./make.sh) || exit 1

src_sym="_usr_src_doslib_tool_dsxmenu_dos86s_dsxmenu_exe"
dst_sym="bin_dsxmenu_exe_pc"

cat >dsxmenu_exe_pc.cpp <<_EOF

#include "dos_inc.h"

_EOF
xxd -i /usr/src/doslib/tool/dsxmenu/dos86s/dsxmenu.exe | sed -e 's/unsigned char/static const unsigned char/' | sed -e 's/unsigned int/static const unsigned int/' | sed -e "s/$src_sym/$dst_sym/" | grep -v "$dst_sym"_len >>dsxmenu_exe_pc.cpp || exit 1

cat >>dsxmenu_exe_pc.cpp <<_EOF

struct BuiltinFileBlob bfb_DSXMENU_EXE_PC = {
	/*recommended file name*/	"DSXMENU.EXE",
	/*data*/			$dst_sym,
	/*length*/			sizeof($dst_sym)
};

_EOF

