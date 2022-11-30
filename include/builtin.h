
#include "dos_inc.h"
#include "../src/builtin/zip.h"
#include "../src/builtin/eval.h"
#include "../src/builtin/cdplay.h"
#include "../src/builtin/dosmid.h"
#include "../src/builtin/mpxplay.h"
#include "../src/builtin/ne2000.h"
#include "../src/builtin/glide2x.h"
#include "../src/builtin/emsmagic.h"
#include "../src/builtin/shutdown.h"
#include "../src/builtin/textutil.h"
#include "../src/builtin/4DOS_img.h"

extern char i4dos_data[CONFIG_SIZE];
extern char config_data[CONFIG_SIZE];
extern char autoexec_data[AUTOEXEC_SIZE];

extern struct BuiltinFileBlob bfb_DSXMENU_EXE_PC;		// DSXMENU.EXE
extern struct BuiltinFileBlob bfb_DSXMENU_EXE_PC98;		// DSXMENU.EXE

extern struct BuiltinFileBlob bfb_CWSDPMI_EXE;		// CWSDPMI.EXE
extern struct BuiltinFileBlob bfb_DOS32A_EXE;		// DOS32A.EXE
extern struct BuiltinFileBlob bfb_DOS4GW_EXE;		// DOS4GW.EXE
extern struct BuiltinFileBlob bfb_DOSIDLE_EXE;		// DOSIDLE.EXE
extern struct BuiltinFileBlob bfb_HEXMEM16_EXE;		// HEXMEM16.EXE
extern struct BuiltinFileBlob bfb_HEXMEM32_EXE;		// HEXMEM32.EXE
extern struct BuiltinFileBlob bfb_REPLACE_EXE;		// REPLACE.EXE (FreeDOS)
extern struct BuiltinFileBlob bfb_SORT_EXE;		// SORT.EXE (FreeDOS)
extern struct BuiltinFileBlob bfb_MOVE_EXE;		// MOVE.EXE (FreeDOS)
extern struct BuiltinFileBlob bfb_MEM_EXE;		// MEM.EXE (FreeDOS)
extern struct BuiltinFileBlob bfb_FIND_EXE;		// FIND.EXE (FreeDOS)
extern struct BuiltinFileBlob bfb_DEBUG_EXE;		// DEBUG.EXE (FreeDOS)
extern struct BuiltinFileBlob bfb_PRINT_COM;		// PRINT.COM (FreeDOS)
extern struct BuiltinFileBlob bfb_XCOPY_EXE;		// XCOPY.EXE (FreeDOS)
extern struct BuiltinFileBlob bfb_APPEND_EXE;		// APPEND.EXE (FreeDOS)
extern struct BuiltinFileBlob bfb_EDLIN_EXE;		// EDLIN.EXE (FreeDOS)
extern struct BuiltinFileBlob bfb_EDIT_COM;		// EDIT.COM
extern struct BuiltinFileBlob bfb_DEVICE_COM;		// DEVICE.COM
extern struct BuiltinFileBlob bfb_BUFFERS_COM;		// BUFFERS.COM
extern struct BuiltinFileBlob bfb_LASTDRIV_COM;		// LASTDRIV.COM
extern struct BuiltinFileBlob bfb_FILES_COM;		// FILES.COM
extern struct BuiltinFileBlob bfb_FCBS_COM;		// FCBS.COM
extern struct BuiltinFileBlob bfb_FC_EXE;		// FC.EXE (FreeDOS)
extern struct BuiltinFileBlob bfb_COMP_COM;		// COMP.COM (FreeDOS)
extern struct BuiltinFileBlob bfb_EVAL_EXE;		// EVAL.EXE
extern struct BuiltinFileBlob bfb_EVAL_HLP;		// EVAL.HLP
extern struct BuiltinFileBlob bfb_28_COM;		// 28.COM
extern struct BuiltinFileBlob bfb_28_COM_ega;	// 28.COM
extern struct BuiltinFileBlob bfb_25_COM;		// 25.COM
extern struct BuiltinFileBlob bfb_25_COM_ega;	// 25.COM
extern struct BuiltinFileBlob bfb_25_COM_other;	// 25.COM
extern struct BuiltinFileBlob bfb_50_COM;		// 50.COM
extern struct BuiltinFileBlob bfb_4DOS_COM;		// 4DOS.COM
extern struct BuiltinFileBlob bfb_4DOS_HLP;		// 4DOS.HLP
extern struct BuiltinFileBlob bfb_4HELP_EXE;	// 4HELP.EXE
extern struct BuiltinFileBlob bfb_BATCOMP_EXE;	// BATCOM.EXE
extern struct BuiltinFileBlob bfb_EXAMPLES_BTM;	// EXAMPLES.BTM
extern struct BuiltinFileBlob bfb_LICENSE_TXT;	// LICENSE.TXT
extern struct BuiltinFileBlob bfb_OPTION_EXE;	// OPTION.EXE
extern struct BuiltinFileBlob bfb_CDPLAY_EXE;	// CDPLAY.EXE
extern struct BuiltinFileBlob bfb_DOSMID_EXE;	// DOSMID.EXE
extern struct BuiltinFileBlob bfb_MPXPLAY_EXE;	// MPXPLAY.EXE
extern struct BuiltinFileBlob bfb_NE2000_COM;	// NE2000.COM
extern struct BuiltinFileBlob bfb_GLIDE2X_OVL;	// GLIDE2X.OVL
extern struct BuiltinFileBlob bfb_VGA_COM;	// VGA.COM
extern struct BuiltinFileBlob bfb_EGA_COM;	// EGA.COM
extern struct BuiltinFileBlob bfb_CLR_COM;	// CLR.COM
extern struct BuiltinFileBlob bfb_CGA_COM;	// CGA.COM
extern struct BuiltinFileBlob bfb_EMSMAGIC_COM;	// EMSMAGIC.COM
extern struct BuiltinFileBlob bfb_SHUTDOWN_COM;	// SHUTDOWN.COM
extern struct BuiltinFileBlob bfb_DISKCOPY_EXE;	// DISKCOPY.EXE (FreeDOS)
extern struct BuiltinFileBlob bfb_DEFRAG_EXE;	// DEFRAG.EXE (FreeDOS)
extern struct BuiltinFileBlob bfb_FDISK_EXE;	// FDISK.EXE (FreeDOS)
extern struct BuiltinFileBlob bfb_FORMAT_EXE;	// FORMAT.EXE (FreeDOS)
extern struct BuiltinFileBlob bfb_CHKDSK_EXE;	// CHKDSK.EXE (FreeDOS)
extern struct BuiltinFileBlob bfb_SYS_COM;	// SYS.COM (FreeDOS)

extern struct BuiltinFileBlob bfb_EGA_CPX;
extern struct BuiltinFileBlob bfb_EGA2_CPX;
extern struct BuiltinFileBlob bfb_EGA3_CPX;
extern struct BuiltinFileBlob bfb_EGA4_CPX;
extern struct BuiltinFileBlob bfb_EGA5_CPX;
extern struct BuiltinFileBlob bfb_EGA6_CPX;
extern struct BuiltinFileBlob bfb_EGA7_CPX;
extern struct BuiltinFileBlob bfb_EGA8_CPX;
extern struct BuiltinFileBlob bfb_EGA9_CPX;
extern struct BuiltinFileBlob bfb_EGA10_CPX;
extern struct BuiltinFileBlob bfb_EGA11_CPX;
extern struct BuiltinFileBlob bfb_EGA12_CPX;
extern struct BuiltinFileBlob bfb_EGA13_CPX;
extern struct BuiltinFileBlob bfb_EGA14_CPX;
extern struct BuiltinFileBlob bfb_EGA15_CPX;
extern struct BuiltinFileBlob bfb_EGA16_CPX;
extern struct BuiltinFileBlob bfb_EGA17_CPX;
extern struct BuiltinFileBlob bfb_EGA18_CPX;
extern struct BuiltinFileBlob bfb_KEYBOARD_SYS;
extern struct BuiltinFileBlob bfb_KEYBRD2_SYS;
extern struct BuiltinFileBlob bfb_KEYBRD3_SYS;
extern struct BuiltinFileBlob bfb_KEYBRD4_SYS;
