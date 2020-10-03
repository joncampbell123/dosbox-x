rem This script will build Inno-Setup based Windows installer for DOSBox-X (Wengier).
rem All Visual Studio and MinGW builds will be extracted from the Windows ZIP packages
rem located in %%vsbinpath%% and %%mgbinpath%% directories (built with other scripts).
rem One of them will be selected by user as the default Windows version to be executed.

@echo off

rem %rootdir% is the root directory of the respository. "." assumes the current directory.
rem Make sure to surround the directory in quotes (") in case it includes spaces.
set rootdir=.

set isspath=%rootdir%\contrib\windows\installer
set vsbinpath=%rootdir%\release\windows
set mgbinpath=%rootdir%\..

cls

if not exist %rootdir%\dosbox-x.reference.conf (
	echo Couldn't find %rootdir%\dosbox-x.reference.conf
	goto error
)

if not exist %isspath%\date.exe (
	echo Couldn't find %isspath%\date.exe
	goto error
)

if not exist %isspath%\7za.exe (
	echo Couldn't find %isspath%\7za.exe
	goto error
)

if not exist %isspath%\ISCC.exe (
	echo Couldn't find Inno Setup Compiler at %isspath%\ISCC.exe
	goto error
)

set datestr=
for /f %%i in ('%isspath%\date.exe +%%Y%%m%%d') do set datestr=%%i

if "%datestr%"=="" (
	echo Couldn't set date
	goto error
)

set winzip=
set m32zip=
set m64zip=

for %%i in (%vsbinpath%\dosbox-x-windows-vsbin-%datestr%*.zip) do set winzip=%%i
for %%i in (%mgbinpath%\dosbox-x-mingw-win32-%datestr%*.zip) do set m32zip=%%i
for %%i in (%mgbinpath%\dosbox-x-mingw-win64-%datestr%*.zip) do set m64zip=%%i

if not exist "%winzip%" (
	echo Couldn't find dosbox-x-windows-vsbin-%datestr%*.zip at %vsbinpath%
	goto error
)

if not exist "%m32zip%" (
	echo Couldn't find dosbox-x-mingw-win32-%datestr%*.zip at %mgbinpath%
	goto error
)

if not exist "%m64zip%" (
	echo Couldn't find dosbox-x-mingw-win64-%datestr%*.zip at %mgbinpath%
	goto error
)

echo.
echo ***************************************
echo * Extract DOSBox-X executables ...    *
echo ***************************************
if exist %isspath%\Win32_builds\nul rd %isspath%\Win32_builds /s /q
if exist %isspath%\Win64_builds\nul rd %isspath%\Win64_builds /s /q
%isspath%\7za.exe e -y -o%isspath%\Win32_builds\x86_Release %winzip% "bin\Win32\Release\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win32_builds\x86_Release_SDL2 %winzip% "bin\Win32\Release SDL2\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win32_builds\ARM_Release %winzip% "bin\ARM\Release\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win32_builds\ARM_Release_SDL2 %winzip% "bin\ARM\Release SDL2\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win64_builds\x64_Release %winzip% "bin\x64\Release\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win64_builds\x64_Release_SDL2 %winzip% "bin\x64\Release SDL2\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win64_builds\ARM64_Release %winzip% "bin\ARM64\Release\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win64_builds\ARM64_Release_SDL2 %winzip% "bin\ARM64\Release SDL2\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win32_builds\mingw %m32zip% "mingw-build\mingw\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win32_builds\mingw-lowend %m32zip% "mingw-build\mingw-lowend\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win32_builds\mingw-sdl2 %m32zip% "mingw-build\mingw-sdl2\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win32_builds\mingw-sdldraw %m32zip% "mingw-build\mingw-sdldraw\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win64_builds\mingw %m64zip% "mingw-build\mingw\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win64_builds\mingw-lowend %m64zip% "mingw-build\mingw-lowend\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win64_builds\mingw-sdl2 %m64zip% "mingw-build\mingw-sdl2\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win64_builds\mingw-sdldraw %m64zip% "mingw-build\mingw-sdldraw\dosbox-x.exe"
copy /y %rootdir%\dosbox-x.reference.conf %isspath%\dosbox-x.reference.conf >nul
if exist %isspath%\unix2dos.exe %isspath%\unix2dos.exe %isspath%\dosbox-x.reference.conf
copy /y %isspath%\dosbox-x.reference.conf %isspath%\Win32_builds\x86_Release
copy /y %isspath%\dosbox-x.reference.conf %isspath%\Win32_builds\x86_Release_SDL2
copy /y %isspath%\dosbox-x.reference.conf %isspath%\Win32_builds\ARM_Release
copy /y %isspath%\dosbox-x.reference.conf %isspath%\Win32_builds\ARM_Release_SDL2
copy /y %isspath%\dosbox-x.reference.conf %isspath%\Win64_builds\x64_Release
copy /y %isspath%\dosbox-x.reference.conf %isspath%\Win64_builds\x64_Release_SDL2
copy /y %isspath%\dosbox-x.reference.conf %isspath%\Win64_builds\ARM64_Release
copy /y %isspath%\dosbox-x.reference.conf %isspath%\Win64_builds\ARM64_Release_SDL2
copy /y %isspath%\dosbox-x.reference.conf %isspath%\Win32_builds\mingw
copy /y %isspath%\dosbox-x.reference.conf %isspath%\Win32_builds\mingw-lowend
copy /y %isspath%\dosbox-x.reference.conf %isspath%\Win32_builds\mingw-sdl2
copy /y %isspath%\dosbox-x.reference.conf %isspath%\Win32_builds\mingw-sdldraw
copy /y %isspath%\dosbox-x.reference.conf %isspath%\Win64_builds\mingw
copy /y %isspath%\dosbox-x.reference.conf %isspath%\Win64_builds\mingw-lowend
copy /y %isspath%\dosbox-x.reference.conf %isspath%\Win64_builds\mingw-sdl2
copy /y %isspath%\dosbox-x.reference.conf %isspath%\Win64_builds\mingw-sdldraw

echo.
echo ***************************************
echo * Building DOSBox-X installers ...    *
echo ***************************************
if exist %isspath%\DOSBox-X-setup*.exe del %isspath%\DOSBox-X-setup*.exe
%isspath%\ISCC.exe %isspath%\DOSBox-X-setup.iss
if exist %isspath%\DOSBox-X-setup.exe ( 
	echo Copying to %vsbinpath%\DOSBox-X-setup-%datestr%.exe ..
	copy /y %isspath%\DOSBox-X-setup.exe %vsbinpath%\DOSBox-X-setup-%datestr%-windows.exe
	goto success
)

:error
echo.
echo ***************************************
echo * An error has occurred, aborting ... *
echo ***************************************
goto end

:success
echo.
echo ***************************************
echo * Success !                           *
echo ***************************************
goto end

:end
set datestr=
set rootdir=
set isspath=
set vsbinpath=
set mgbinpath=
set winzip=
set m32zip=
set m64zip=
