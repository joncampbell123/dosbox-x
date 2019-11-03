@echo off

cls

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
	echo Uninstallation failed!
	exit /b 1
)

<nul set/p="Searching for registry entry Directory: "
reg query HKCR\Directory\shell\DOSBox-X >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
	echo not found!
	echo.
	goto :uninstall_background
)
echo found!
echo.
reg delete HKCR\Directory\shell\DOSBox-X /f >nul 2>&1

:uninstall_background
<nul set/p="Searching for registry entry Directory\Background: "
reg query HKCR\Directory\Background\shell\DOSBox-X >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
	echo not found!
	echo.
	goto :uninstall_finish
)
echo found!
echo.
reg delete HKCR\Directory\Background\shell\DOSBox-X /f >nul 2>&1

:uninstall_finish
echo Uninstallation successful!
goto :eof


