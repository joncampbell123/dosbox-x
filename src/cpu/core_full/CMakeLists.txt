set(SRC_CPU_CORE_FULL
	"${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt"
	"${CMAKE_CURRENT_LIST_DIR}/ea_lookup.h"
	"${CMAKE_CURRENT_LIST_DIR}/load.h"
	"${CMAKE_CURRENT_LIST_DIR}/loadwrite.h"
	# "${CMAKE_CURRENT_LIST_DIR}/Makefile.am"
	"${CMAKE_CURRENT_LIST_DIR}/op.h"
	"${CMAKE_CURRENT_LIST_DIR}/optable.h"
	"${CMAKE_CURRENT_LIST_DIR}/save.h"
	"${CMAKE_CURRENT_LIST_DIR}/string.h"
	"${CMAKE_CURRENT_LIST_DIR}/support.h"
)
source_group("Sources\\cpu\\core_full" FILES ${SRC_CPU_CORE_FULL})
