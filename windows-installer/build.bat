@echo off

cls

for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /format:list') do set DOSBOXDATE=%%I
set DOSBOXDATE=%DOSBOXDATE:~0,8%-%DOSBOXDATE:~8,6%
set DOSBOX32="..\bin\Win32\Release\dosbox-x.exe"
set DOSBOX64="..\bin\x64\Release\dosbox-x.exe"
set ISCC="C:\Program Files (x86)\Inno Setup 5\ISCC.exe"

if not exist %DOSBOX32% (
	echo Couldn't find 32-bit DOSBox-X at %DOSBOX32%
	goto error
)

if not exist %DOSBOX64% (
	echo Couldn't find 64-bit DOSBox-X at %DOSBOX64%
	goto error
)

if not exist %ISCC% (
	echo Couldn't find Inno Setup 5 at %ISCC%
	goto error
)

echo ************************************
echo * Building DOSBox-X standalone ... *
echo ************************************
del dosbox-x-standalone-win*.7z
7za a -mx9 dosbox-x-standalone-win32-%DOSBOXDATE% %DOSBOX32%
7za a -mx9 dosbox-x-standalone-win64-%DOSBOXDATE% %DOSBOX64%

echo ************************************
echo * Building DOSBox-X installers ... *
echo ************************************
del dosbox-x-setup-win*.exe
%ISCC% DOSBox-X-setup-win32.iss
%ISCC% DOSBox-X-setup-win64.iss
ren DOSBox-X-setup-win32.exe DOSBox-X-setup-win32-%DOSBOXDATE%.exe
ren DOSBox-X-setup-win64.exe DOSBox-X-setup-win64-%DOSBOXDATE%.exe
goto success

:error
echo An error has occurred, aborting ...
goto end

:success
echo Success !
goto end

:end
set DOSBOXDATE=
set DOSBOX32=
set DOSBOX64=
set ISCC=