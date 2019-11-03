@echo off & cls & echo DOSBox-X context menu for Windows Explorer uninstaller & echo.
set HKCU_DIR_FRNT=HKCU\Software\Classes\Directory\shell\DOSBox-X
set HKCU_DIR_BACK=HKCU\Software\Classes\Directory\Background\shell\DOSBox-X
for %%a in (%HKCU_DIR_FRNT% %HKCU_DIR_BACK%) do reg delete %%a /f >nul 2>&1
set HKCU_DIR_FRNT=
set HKCU_DIR_BACK=
echo Uninstallation complete.
