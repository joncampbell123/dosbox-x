rem This script will build Inno-Setup based Windows installer for DOSBox-X (Wengier).
rem All Visual Studio and MinGW builds will be extracted from the Windows ZIP packages
rem located in the directory as specified in %%binpath%% (built with other scripts).
rem One of them will be selected by user as the default Windows version to be executed.

@echo off

rem %rootdir% is the root directory of the repository. "." assumes the current directory.
rem Make sure to surround the directory in quotes (") in case it includes spaces.
set rootdir=.

set isspath=%rootdir%\contrib\windows\installer
set binpath=%rootdir%\release\windows

cls

if not exist %rootdir%\dosbox-x.reference.conf (
	echo Couldn't find %rootdir%\dosbox-x.reference.conf
	goto error
)

if not exist %rootdir%\dosbox-x.reference.full.conf (
	echo Couldn't find %rootdir%\dosbox-x.reference.full.conf
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

if not exist %isspath%\fart.exe (
	echo Couldn't find %isspath%\fart.exe
	goto error
)

if not exist %isspath%\ISCC.exe (
	echo Couldn't find Inno Setup Compiler at %isspath%\ISCC.exe
	goto error
)

set datestr=%1
if "%datestr:~0,2%"=="20" if not "%datestr:~7%"=="" if "%datestr:~8%"=="" goto hasdate
for /f %%i in ('%isspath%\date.exe +%%Y%%m%%d') do set datestr=%%i

if "%datestr%"=="" (
	echo Couldn't set date
	goto error
)

:hasdate
set vwin32zip=
set vwin64zip=
set varm32zip=
set varm64zip=
set mlezip=
set m32zip=
set m64zip=

for %%i in (%binpath%\dosbox-x-vsbuild-win32-%datestr%*.zip) do set vwin32zip=%%i
for %%i in (%binpath%\dosbox-x-vsbuild-win64-%datestr%*.zip) do set vwin64zip=%%i
for %%i in (%binpath%\dosbox-x-vsbuild-arm32-%datestr%*.zip) do set varm32zip=%%i
for %%i in (%binpath%\dosbox-x-vsbuild-arm64-%datestr%*.zip) do set varm64zip=%%i
for %%i in (%binpath%\dosbox-x-mingw-win32-lowend-%datestr%*.zip) do set mlezip=%%i
for %%i in (%binpath%\dosbox-x-mingw-win32-%datestr%*.zip) do set m32zip=%%i
for %%i in (%binpath%\dosbox-x-mingw-win64-%datestr%*.zip) do set m64zip=%%i

if not exist "%vwin32zip%" (
	echo Couldn't find dosbox-x-vsbuild-win32-%datestr%*.zip at %binpath%
	goto error
)

if not exist "%vwin64zip%" (
	echo Couldn't find dosbox-x-vsbuild-win64-%datestr%*.zip at %binpath%
	goto error
)

if not exist "%varm32zip%" (
	echo Couldn't find dosbox-x-vsbuild-arm32-%datestr%*.zip at %binpath%
	goto error
)

if not exist "%varm64zip%" (
	echo Couldn't find dosbox-x-vsbuild-arm64-%datestr%*.zip at %binpath%
	goto error
)

if not exist "%mlezip%" (
	echo Couldn't find dosbox-x-mingw-win32-lowend-%datestr%*.zip at %binpath%
	goto error
)

if not exist "%m32zip%" (
	echo Couldn't find dosbox-x-mingw-win32-%datestr%*.zip at %binpath%
	goto error
)

if not exist "%m64zip%" (
	echo Couldn't find dosbox-x-mingw-win64-%datestr%*.zip at %binpath%
	goto error
)

echo.
echo ***************************************
echo * Extract DOSBox-X executables ...    *
echo ***************************************
if exist %isspath%\Win32_builds\nul rd %isspath%\Win32_builds /s /q
if exist %isspath%\Win64_builds\nul rd %isspath%\Win64_builds /s /q
%isspath%\7za.exe e -y -o%isspath%\Win32_builds\x86_Release %vwin32zip% "bin\Win32\Release\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win32_builds\x86_Release_SDL2 %vwin32zip% "bin\Win32\Release SDL2\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win32_builds\ARM_Release %varm32zip% "bin\ARM\Release\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win32_builds\ARM_Release_SDL2 %varm32zip% "bin\ARM\Release SDL2\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win64_builds\x64_Release %vwin64zip% "bin\x64\Release\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win64_builds\x64_Release_SDL2 %vwin64zip% "bin\x64\Release SDL2\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win64_builds\ARM64_Release %varm64zip% "bin\ARM64\Release\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win64_builds\ARM64_Release_SDL2 %varm64zip% "bin\ARM64\Release SDL2\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win32_builds\mingw-lowend %mlezip% "mingw-build\mingw-lowend\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win32_builds\mingw-lowend-sdl2 %mlezip% "mingw-build\mingw-lowend-sdl2\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win32_builds\mingw %m32zip% "mingw-build\mingw\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win32_builds\mingw-sdl2 %m32zip% "mingw-build\mingw-sdl2\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win64_builds\mingw %m64zip% "mingw-build\mingw\dosbox-x.exe"
%isspath%\7za.exe e -y -o%isspath%\Win64_builds\mingw-sdl2 %m64zip% "mingw-build\mingw-sdl2\dosbox-x.exe"
for %%i in (dosbox-x.reference.conf dosbox-x.reference.full.conf) do (
	copy /y %rootdir%\%%i %isspath%\%%i >nul
	if exist %isspath%\unix2dos.exe %isspath%\unix2dos.exe %isspath%\%%i
	copy /y %isspath%\%%i %isspath%\Win32_builds\x86_Release
	copy /y %isspath%\%%i %isspath%\Win32_builds\x86_Release_SDL2
	copy /y %isspath%\%%i %isspath%\Win32_builds\ARM_Release
	copy /y %isspath%\%%i %isspath%\Win32_builds\ARM_Release_SDL2
	copy /y %isspath%\%%i %isspath%\Win64_builds\x64_Release
	copy /y %isspath%\%%i %isspath%\Win64_builds\x64_Release_SDL2
	copy /y %isspath%\%%i %isspath%\Win64_builds\ARM64_Release
	copy /y %isspath%\%%i %isspath%\Win64_builds\ARM64_Release_SDL2
	copy /y %isspath%\%%i %isspath%\Win32_builds\mingw-lowend
	copy /y %isspath%\%%i %isspath%\Win32_builds\mingw-lowend-sdl2
	copy /y %isspath%\%%i %isspath%\Win32_builds\mingw
	copy /y %isspath%\%%i %isspath%\Win32_builds\mingw-sdl2
	copy /y %isspath%\%%i %isspath%\Win64_builds\mingw
	copy /y %isspath%\%%i %isspath%\Win64_builds\mingw-sdl2
)
if exist %isspath%\PatchPE.exe (
	%isspath%\PatchPE.exe %isspath%\Win32_builds\x86_Release\dosbox-x.exe
	%isspath%\PatchPE.exe %isspath%\Win32_builds\x86_Release_SDL2\dosbox-x.exe
	%isspath%\PatchPE.exe %isspath%\Win64_builds\x64_Release\dosbox-x.exe
	%isspath%\PatchPE.exe %isspath%\Win64_builds\x64_Release_SDL2\dosbox-x.exe
)
echo.
echo ***************************************
echo * Building DOSBox-X installers ...    *
echo ***************************************
if exist %isspath%\dosbox-x-win32-*-setup.exe del %isspath%\dosbox-x-win32-*-setup.exe
copy /y %isspath%\WizModernImage.bmp %isspath%\64bit\WizModernImage.bmp
copy /y %isspath%\DOSBox-X-setup.iss %isspath%\64bit\DOSBox-X-setup.iss
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss "win32-{" "win64-{"
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss ";Privileges" "Privileges"
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss "=lowest" "=admin"
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss "=..\\" "=..\..\\"
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss "=.\\" "=..\\"
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss "\"..\\" "\"..\..\\"
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss "\".\\" "\"..\\"
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss "File=setup_" "File=..\setup_"
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss "(32-bit)" "(64-bit)"
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss "'32-bit " "'64-bit "
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss "inpout32" "inpoutx64"
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss "\"Win32_builds" "\"..\Win64_builds"
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss "Win32_builds" "Win64_builds"
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss "32-bit MinGW" "64-bit MinGW"
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss "x86_Release" "x64_Release"
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss "ARM_Release" "ARM64_Release"
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss "x86 Release" "x64 Release"
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss "ARM Release" "ARM64 Release"
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss "{userappdata}" "{commonappdata}"
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss "HKCU;" "HKA;"
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss "if True then" "if False then"
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss " IsTaskSelected" " WizardIsTaskSelected"
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss "IsAdminLoggedOn" "IsAdmin"
%isspath%\fart.exe %isspath%\64bit\DOSBox-X-setup.iss "Source: \"..\Win64_builds\mingw-lowend" ";Source: \"..\Win64_builds\mingw-lowend"
%isspath%\ISCC.exe %isspath%\DOSBox-X-setup.iss
%isspath%\64bit\ISCC.exe %isspath%\64bit\DOSBox-X-setup.iss
if exist %isspath%\dosbox-x-win32-*-setup.exe if exist %isspath%\dosbox-x-win64-*-setup.exe (
	for %%i in (%isspath%\dosbox-x-win32-*-setup.exe %isspath%\dosbox-x-win64-*-setup.exe) do echo Copying to %binpath%\%%~nxi...
	copy /y %isspath%\dosbox-x-win32-*-setup.exe %binpath%
	copy /y %isspath%\dosbox-x-win64-*-setup.exe %binpath%
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
set binpath=
set vwin32zip=
set vwin64zip=
set varm32zip=
set varm64zip=
set mlezip=
set m32zip=
set m64zip=
