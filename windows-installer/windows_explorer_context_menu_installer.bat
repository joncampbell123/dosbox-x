@echo off & cls & echo DOSBox-X context menu for Windows Explorer installer & echo.
call :initialize
set DOSBOX_X_DIR=
set DOSBOX_X_PATH=

<nul set/p="Searching DOSBox-X executable: "
ver >nul
if not exist %DOSBOX_X_EXE% if exist ..\%DOSBOX_X_EXE% (
	pushd ..
	where %DOSBOX_X_EXE% /q
	if %ERRORLEVEL% EQU 0 for /f "usebackq delims=" %%a in (`where %DOSBOX_X_EXE%`) do set DOSBOX_X_PATH=%%~a
	popd
)
if "%DOSBox-X_PATH%"=="" if not exist %DOSBOX_X_EXE% if exist C:\DOSBox-X\%DOSBOX_X_EXE% (
	pushd C:\DOSBox-X
	where %DOSBOX_X_EXE% /q
	if %ERRORLEVEL% EQU 0 for /f "usebackq delims=" %%a in (`where %DOSBOX_X_EXE%`) do set DOSBOX_X_PATH=%%~a
	popd
)
if "%DOSBOX_X_PATH%"=="" (
	where %DOSBOX_X_EXE% /q
	if %ERRORLEVEL% EQU 0 for /f "usebackq delims=" %%a in (`where %DOSBOX_X_EXE%`) do set DOSBOX_X_PATH=%%~a
)
if "%DOSBOX_X_PATH%"=="" echo not found! & echo. & call :failed & exit /b 1
echo found! & echo.
for %%a in (%DOSBOX_X_PATH%) do set DOSBOX_X_DIR=%%~dpa
for %%a in ("%HKCU_DIR_FRNT%" "%HKCU_DIR_BACK%") do (
	reg add %%a /f /ve /d "Open with DOSBox-X" >nul 2>&1
	if %ERRORLEVEL% NEQ 0 call :uninstall & exit /b 1
	reg add %%a /f /v Icon /d "\"%DOSBOX_X_PATH%\",0" >nul 2>&1
	if %ERRORLEVEL% NEQ 0 call :uninstall & exit /b 1
	reg add %%a\command /f /ve /d "\"%DOSBOX_X_PATH%\" -defaultdir \"%DOSBOX_X_DIR% \" \"%%v \"" >nul 2>&1
	if %ERRORLEVEL% NEQ 0 call :uninstall & exit /b 1
)
for %%a in ("%HKCU_EXE_OPEN%" "%HKCU_COM_OPEN%" "%HKCU_BAT_OPEN%") do (
	reg add %%a /f /v Icon /d "\"%DOSBOX_X_PATH%\",0" >nul 2>&1
	if %ERRORLEVEL% NEQ 0 call :uninstall & exit /b 1
	reg add %%a\command /f /ve /d "\"%DOSBOX_X_PATH%\" -fastlaunch -defaultdir \"%DOSBOX_X_DIR% \" \"%%1\"" >nul 2>&1
	if %ERRORLEVEL% NEQ 0 call :uninstall & exit /b 1
)
call :success & exit /b 1

:uninstall
echo Error during registration, cleaning up...
for %%a in ("%HKCU_DIR_FRNT%" "%HKCU_DIR_BACK%" "%HKCU_EXE_OPEN%" "%HKCU_COM_OPEN%" "%HKCU_BAT_OPEN%") do reg delete /f %%a >nul 2>&1
call :failed & goto :eof

:failed
echo Installation failed!
call :cleanup & goto :eof

:success
echo Installation complete.
call :cleanup & goto :eof

:initialize
set DOSBOX_X_EXE=dosbox-x.exe
set HKCU_DIR_FRNT=HKCU\Software\Classes\Directory\shell\DOSBox-X
set HKCU_DIR_BACK=HKCU\Software\Classes\Directory\Background\shell\DOSBox-X
set HKCU_EXE_OPEN=HKCU\Software\Classes\SystemFileAssociations\.exe\shell\Open with DOSBox-X
set HKCU_COM_OPEN=HKCU\Software\Classes\SystemFileAssociations\.com\shell\Open with DOSBox-X
set HKCU_BAT_OPEN=HKCU\Software\Classes\SystemFileAssociations\.bat\shell\Open with DOSBox-X
goto :eof

:cleanup
set DOSBOX_X_EXE=
set DOSBOX_X_DIR=
set DOSBOX_X_PATH=
set HKCU_DIR_FRNT=
set HKCU_DIR_BACK=
set HKCU_EXE_OPEN=
set HKCU_COM_OPEN=
set HKCU_BAT_OPEN=
goto :eof
