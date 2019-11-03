@echo off
cls

call :cleanup

echo ----------------------------------------------
echo ^| DOSBox-X context menu for Windows Explorer ^|
echo ----------------------------------------------
echo.

net session >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
	echo This script requires administrative rights, you can either:
	echo - run it in an elevated command prompt
	echo - in Windows Explorer, right-click it and choose "Run as administrator"
	echo.
	call :failed
	exit /b 1
)

<nul set/p="Searching DOSBox-X executable: "
set DOSBOX-X-EXE=dosbox-x.exe

where %DOSBOX-X-EXE% /q

if %ERRORLEVEL% NEQ 0 (
	echo not found!
	echo.
	call :failed
	exit /b 1
)

for /f "usebackq delims=" %%a in (`where %DOSBOX-X-EXE%`) do (
	set DOSBOX-X-PATH=%%~a
)

echo found!
echo.

reg add HKCR\Directory\shell\DOSBox-X /ve /d "Open with DOSBox-X" /f >nul 2>&1
if %ERRORLEVEL% NEQ 0 call :uninstall & exit /b 1

reg add HKCR\Directory\shell\DOSBox-X /v Icon /d "\"%DOSBOX-X-PATH%\",0" /f >nul 2>&1
if %ERRORLEVEL% NEQ 0 call :uninstall & exit /b 1

reg add HKCR\Directory\shell\DOSBox-X\command /ve /d "\"%DOSBOX-X-PATH%\" -c \"@mount c \\\"%%v\\\"\" -c \"@c:"" /f >nul 2>&1
if %ERRORLEVEL% NEQ 0 call :uninstall & exit /b 1

reg add HKCR\Directory\Background\shell\DOSBox-X /ve /d "Open with DOSBox-X" /f >nul 2>&1
if %ERRORLEVEL% NEQ 0 call :uninstall & exit /b 1

reg add HKCR\Directory\Background\shell\DOSBox-X /v Icon /d "\"%DOSBOX-X-PATH%\",0" /f >nul 2>&1
if %ERRORLEVEL% NEQ 0 call :uninstall & exit /b 1

reg add HKCR\Directory\Background\shell\DOSBox-X\command /ve /d "\"%DOSBOX-X-PATH%\" -c \"@mount c \\\"%%v\\\"\" -c \"@c:"" /f >nul 2>&1
if %ERRORLEVEL% NEQ 0 call :uninstall & exit /b 1

echo Installation succesful!
goto :cleanup

:uninstall
echo Error during registration, cleaning up...
call :failed
reg query HKCR\Directory\shell\DOSBox-X >nul 2>&1 || goto :uninstall_background
reg delete HKCR\Directory\shell\DOSBox-X /f >nul 2>&1
:uninstall_background
reg query HKCR\Directory\Background\shell\DOSBox-X >nul 2>&1 || goto :uninstall__
reg delete HKCR\Directory\Background\shell\DOSBox-X /f >nul 2>&1
:uninstall__
goto :eof

:failed
echo Installation failed!
call :cleanup
goto :eof

:cleanup
set DOSBOX-X-EXE=
set DOSBOX-X-PATH=
goto :eof