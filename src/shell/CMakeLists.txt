set(SRC_SHELL
	"${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt"
	# "${CMAKE_CURRENT_LIST_DIR}/Makefile.am"
	"${CMAKE_CURRENT_LIST_DIR}/shell.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/shell_batch.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/shell_cmds.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/shell_misc.cpp"
)
source_group("Sources\\shell" FILES ${SRC_SHELL})
