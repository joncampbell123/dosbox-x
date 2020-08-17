@echo off & cls & echo DOSBox-X context menu for Windows Explorer uninstaller & echo.
set HKCU_DIR_FRNT=HKCU\Software\Classes\Directory\shell\DOSBox-X
set HKCU_DIR_BACK=HKCU\Software\Classes\Directory\Background\shell\DOSBox-X
set HKCU_EXE_OPEN=HKCU\Software\Classes\SystemFileAssociations\.exe\shell\Open with DOSBox-X
set HKCU_COM_OPEN=HKCU\Software\Classes\SystemFileAssociations\.com\shell\Open with DOSBox-X
set HKCU_BAT_OPEN=HKCU\Software\Classes\SystemFileAssociations\.bat\shell\Open with DOSBox-X
for %%a in ("%HKCU_DIR_FRNT%" "%HKCU_DIR_BACK%" "%HKCU_EXE_OPEN%" "%HKCU_COM_OPEN%" "%HKCU_BAT_OPEN%") do reg delete %%a /f >nul 2>&1
set HKCU_DIR_FRNT=
set HKCU_DIR_BACK=
set HKCU_EXE_OPEN=
set HKCU_COM_OPEN=
set HKCU_BAT_OPEN=
echo Uninstallation complete.
