set(SRC
	"${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt"
	"${CMAKE_CURRENT_LIST_DIR}/dosbox.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../contrib/icons/dosbox-x.ico"
	"${CMAKE_CURRENT_LIST_DIR}/dosbox.pkginfo"
	"${CMAKE_CURRENT_LIST_DIR}/dosbox.plist"
	"${CMAKE_CURRENT_LIST_DIR}/../contrib/icons/dosbox-x.png"
	# "${CMAKE_CURRENT_LIST_DIR}/Makefile.am"
	"${CMAKE_CURRENT_LIST_DIR}/resource.h"
	"${CMAKE_CURRENT_LIST_DIR}/winres.h"
	"${CMAKE_CURRENT_LIST_DIR}/winres.rc"
)
source_group("Sources" FILES ${SRC})
