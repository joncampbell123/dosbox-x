#include "DOSBox-X-setup.iss"

[Setup]
AppId={{ABAE5874-8973-4FE0-A356-DCB5F5DE6912}
AppName={#MyAppName}
OutputBaseFilename=DOSBox-X-setup-win64
UninstallDisplayName={#MyAppName} {#MyAppVersion} 64-bit
DefaultDirName={sd}\{#MyAppName} {#MyAppVersion} 64-bit

[Files]
Source: "..\bin\x64\Release\dosbox-x.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{commonprograms}\{#MyAppName}\{#MyAppName} {#MyAppVersion} 64-bit"; Filename: "{app}\{#MyAppExeName}"
Name: "{commondesktop}\{#MyAppName} {#MyAppVersion} 64-bit"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
