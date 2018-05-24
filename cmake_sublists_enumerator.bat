REM enumerates files to add to cmake sublists

@echo off

if "%~1"=="" (
	echo Directory not specified !
	goto :eof
)

for /r %1 %%i in (.) do (
	pushd %%i
	cd
	for %%j in (*.*) do @echo "${CMAKE_CURRENT_LIST_DIR}%%~j"
	popd
)