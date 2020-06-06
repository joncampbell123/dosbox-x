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

echo ***************************************
echo * Building DOSBox-X standalone ...    *
echo ***************************************
del dosbox-x-standalone-win*.zip
7za a dosbox-x-standalone-win32-%DOSBOXDATE%.zip %DOSBOX32%
7za a dosbox-x-standalone-win64-%DOSBOXDATE%.zip %DOSBOX64%

echo.

echo ***************************************
echo * Building DOSBox-X installers ...    *
echo ***************************************
del dosbox-x-setup-win*.zip
if not exist %ISCC% (
	echo Inno Setup 5 not found at %ISCC%, skipping ...
	goto success
)
%ISCC% DOSBox-X-setup.iss
ren DOSBox-X-setup.exe DOSBox-X-setup-%DOSBOXDATE%.exe
7za a dosbox-x-setup-%DOSBOXDATE%.zip DOSBox-X-setup-%DOSBOXDATE%.exe
DOSBOXDATE%.exe
del dosbox-x-setup-*.exe

goto success

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
set DOSBOXDATE=
set DOSBOX32=
set DOSBOX64=
set ISCC=