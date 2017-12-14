#include "DOSBox-X-setup.iss"

[Setup]
AppId={{A8B56D99-2ECA-4D82-B161-958006195B58}
AppName={#MyAppName}
OutputBaseFilename=DOSBox-X-setup-win32
UninstallDisplayName={#MyAppName} {#MyAppVersion} 32-bit
DefaultDirName={sd}\{#MyAppName} {#MyAppVersion} 32-bit

[Files]
Source: "..\bin\Win32\Release\dosbox-x.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{commonprograms}\{#MyAppName}\{#MyAppName} {#MyAppVersion} 32-bit"; Filename: "{app}\{#MyAppExeName}"
Name: "{commondesktop}\{#MyAppName} {#MyAppVersion} 32-bit"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
