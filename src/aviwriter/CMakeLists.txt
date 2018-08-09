set(SRC_AVIWRITER
	"${CMAKE_CURRENT_LIST_DIR}/avi.h"
	"${CMAKE_CURRENT_LIST_DIR}/avi_rw_iobuf.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/avi_rw_iobuf.h"
	"${CMAKE_CURRENT_LIST_DIR}/avi_writer.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/avi_writer.h"
	"${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt"
	"${CMAKE_CURRENT_LIST_DIR}/guid.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/guid.h"
	"${CMAKE_CURRENT_LIST_DIR}/ksdataformat.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/ksdataformat.h"
	# "${CMAKE_CURRENT_LIST_DIR}/Makefile.am"
	"${CMAKE_CURRENT_LIST_DIR}/riff.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/riff.h"
	"${CMAKE_CURRENT_LIST_DIR}/riff_wav_writer.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/riff_wav_writer.h"
)
source_group("Sources\\aviwriter" FILES ${SRC_AVIWRITER})
