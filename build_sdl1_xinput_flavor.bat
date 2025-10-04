@echo off
pushd
cd vs
set SDL1AdditionalOptions=/DSDL_JOYSTICK_XINPUT
msbuild dosbox-x.sln /p:Configuration=Release /p:Platform=Win32
msbuild dosbox-x.sln /p:Configuration=Release /p:Platform=x64
set SDL1AdditionalOptions=
popd