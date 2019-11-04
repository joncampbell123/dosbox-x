@echo off & cls & echo DOSBox-X context menu for Windows Explorer installer & echo.
call :initialize

<nul set/p="Searching DOSBox-X executable: "
where %DOSBOX_X_EXE% /q
if %ERRORLEVEL% NEQ 0 echo not found! & echo. & call :failed & exit /b 1
for /f "usebackq delims=" %%a in (`where %DOSBOX_X_EXE%`) do set DOSBOX_X_PATH=%%~a
echo found! & echo.

for %%a in (%HKCU_DIR_FRNT% %HKCU_DIR_BACK%) do (
	reg add %%a /f /ve /d "Open with DOSBox-X" >nul 2>&1
	if %ERRORLEVEL% NEQ 0 call :uninstall & exit /b 1
	reg add %%a /f /v Icon /d "\"%DOSBOX_X_PATH%\",0" >nul 2>&1
	if %ERRORLEVEL% NEQ 0 call :uninstall & exit /b 1
	reg add %%a\command /f /ve /d "\"%DOSBOX_X_PATH%\" -c \"@mount c \\\"%%v\\\"\" -c \"@c:"" >nul 2>&1
	if %ERRORLEVEL% NEQ 0 call :uninstall & exit /b 1
)

call :success & exit /b 1

:uninstall
echo Error during registration, cleaning up...
for %%a in (%HKCU_DIR_FRNT% %HKCU_DIR_BACK%) do reg delete /f %%a >nul 2>&1
call :failed & goto :eof

:failed
echo Installation failed!
call :cleanup & goto :eof

:success
echo Installation complete.
call :cleanup & goto :eof

:initialize
set DOSBOX_X_EXE=dosbox-x.exe
set DOSBOX_X_PATH=
set HKCU_DIR_FRNT=HKCU\Software\Classes\Directory\shell\DOSBox-X
set HKCU_DIR_BACK=HKCU\Software\Classes\Directory\Background\shell\DOSBox-X
goto :eof

:cleanup
set DOSBOX_X_EXE=
set DOSBOX_X_PATH=
set HKCU_DIR_FRNT=
set HKCU_DIR_BACK=
goto :eof
