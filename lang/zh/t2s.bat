@echo off
setlocal enabledelayedexpansion

echo Traditional to Simplified Chinese Conversion
echo.

opencc --version >nul 2>&1
if errorlevel 1 (
  echo Error: opencc command not found or not working!
  echo Please ensure OpenCC is installed and in PATH.
  pause
  exit /b 1
)

set CONFIG_PATH=C:\msys64\mingw64\share\opencc

for %%f in (*_TW.txt) do (
  if exist "%%f" (
    set "output=%%f"
    set "output=!output:_TW.txt=_CN.txt!"
    echo Converting: %%f -> !output!
    opencc -i "%%f" -o "!output!" -c t2s.json --path "!CONFIG_PATH!"
    if !errorlevel! equ 0 (
      echo ✓ Success
    ) else (
      echo ✗ Failed
    )
    echo.
  )
)

echo Conversion completed!
pause