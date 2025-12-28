@echo off
setlocal enabledelayedexpansion

where cvt >nul 2>&1
if errorlevel 1 (
  echo "No convert program in you system or not in PATH directory, it's named cvt.exe."
	exit /b /1
)

cd /d "%1" 2>nul

set "processed_count=0"

for %%f in (*_%2.txt) do (
  attrib "%%~nxf" | findstr /r "\<A\>" >nul
    
  if !errorlevel! equ 0 (
    echo Processing modified file: %%~nxf
    cvt "%%~nf"
        
    attrib -a "%%~nxf"
    set /a processed_count+=1
        
    if exist "%%~nf.lng" (
      echo Created: %%~nf.lng
    )
  )
)

echo Processed !processed_count! files.
cd ..
endlocal