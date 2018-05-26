set(SRC_CPU_CORE_NORMAL
	"${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt"
	"${CMAKE_CURRENT_LIST_DIR}/helpers.h"
	# "${CMAKE_CURRENT_LIST_DIR}/Makefile.am"
	"${CMAKE_CURRENT_LIST_DIR}/prefix_0f.h"
	"${CMAKE_CURRENT_LIST_DIR}/prefix_0f_mmx.h"
	"${CMAKE_CURRENT_LIST_DIR}/prefix_66.h"
	"${CMAKE_CURRENT_LIST_DIR}/prefix_66_0f.h"
	"${CMAKE_CURRENT_LIST_DIR}/prefix_none.h"
	"${CMAKE_CURRENT_LIST_DIR}/string.h"
	"${CMAKE_CURRENT_LIST_DIR}/support.h"
	"${CMAKE_CURRENT_LIST_DIR}/table_ea.h"
	"${CMAKE_CURRENT_LIST_DIR}/table_ea_8086.h"
)
source_group("Sources\\cpu\\core_normal" FILES ${SRC_CPU_CORE_NORMAL})
