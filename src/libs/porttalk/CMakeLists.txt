set(SRC_LIBS_PORTTALK
	"${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt"
	# "${CMAKE_CURRENT_LIST_DIR}/Makefile.am"
	"${CMAKE_CURRENT_LIST_DIR}/porttalk.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/porttalk.h"
	"${CMAKE_CURRENT_LIST_DIR}/PortTalk_IOCTL.h"
)
source_group("Sources\\libs\\porttalk" FILES ${SRC_LIBS_PORTTALK})
