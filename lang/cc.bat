@echo off
setlocal enabledelayedexpansion

if "%1"=="" (
  echo "Usage:cc language_folder sub_language_zone"
  echo "Must provide language folder name."
	exit /b /1
)

if "%2"=="" (
  echo "Usage:cc language_folder sub_language_zone"
  echo Must provide sub language zone name.
	exit /b /1
)

set "config_file=target.cfg"

if not exist "%config_file%" (
  echo "Config file : %config_file%"
  echo Target fold config file not exist, can't go on.
	exit /b 1
)

set /p target_folder=<"%config_file%"
set "target_folder=!target_folder!\%1"

if not exist "%target_folder%\" (
  echo "Target folder : %target_folder%\"
  echo Target folder not exist.
	exit /b /1
)

call "txt2lng" "%1" "%2"
if errorlevel 1 (
  exit /b /1
)

cd /d "%1" 2>nul

if errorlevel 1 (
  echo "Language folder : %1"
	echo "Can't change to language folder."
  exit /b 1
)

set lng=%1_%2.lng
set tmp=%1_%2.tmp
if not exist "%lng%" (
  echo "Origin language file : %lng%"
  echo "Origin Language file not exist."
	exit /b /1
)

set help0=%1_%2.ext
if not exist "%help0%" (
  echo "Extension language file : %help0%"
  echo "Language extension file not exist."
	exit /b /1
)

set "help1=help_%2.lng"
set "help2=sm_%2.lng"
set "help3=seg_%2.lng"
set "help4=off_%2.lng"
set "help5=sl_%2.lng"
set "help6=d_%2.lng"
set "help7=dv_%2.lng"
set "help8=s_%2.lng"
set "help9=l_%2.lng"
set "help10=tl_%2.lng"
set "help11=td_%2.lng"
set "help12=tw_%2.lng"
set "help13=ts_%2.lng"
set "help14=tr_%2.lng"
set "help15=a_%2.lng"
set "help16=r_%2.lng"
set "help17=wr_%2.lng"
set "help18=cls_%2.lng"
set "help19=save_%2.lng"
set "help20=mm_%2.lng"
set "help21=mseg_%2.lng"
set "help22=moff_%2.lng"
set "help23=mw_%2.lng"
set "help24=ma_%2.lng"
set "help25=snap_%2.lng"
set "help26=su_%2.lng"
set "help27=sd_%2.lng"
set "help28=ss_%2.lng"
set "help29=cvt_%2.lng"

copy /y "%lng%" "%tmp%"
for /l %%i in (0, 1, 29) do (
  if defined help%%i (
	  type "!help%%i!" >> "%tmp%" 2>nul
	)
)

copy /y "%tmp%" "%target_folder%\%lng%"

del "%tmp%"

cd ..