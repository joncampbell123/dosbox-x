set(SRC_HARDWARE_PARPORT
	"${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt"
	"${CMAKE_CURRENT_LIST_DIR}/directlpt_linux.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/directlpt_linux.h"
	"${CMAKE_CURRENT_LIST_DIR}/directlpt_win32.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/directlpt_win32.h"
	"${CMAKE_CURRENT_LIST_DIR}/filelpt.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/filelpt.h"
	# "${CMAKE_CURRENT_LIST_DIR}/Makefile.am"
	"${CMAKE_CURRENT_LIST_DIR}/parport.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/printer.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/printer.h"
	"${CMAKE_CURRENT_LIST_DIR}/printer_charmaps.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/printer_charmaps.h"
	"${CMAKE_CURRENT_LIST_DIR}/printer_if.h"
	"${CMAKE_CURRENT_LIST_DIR}/printer_redir.h"
)
source_group("Sources\\hardware\\parport" FILES ${SRC_HARDWARE_PARPORT})
