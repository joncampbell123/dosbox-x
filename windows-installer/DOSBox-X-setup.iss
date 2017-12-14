#define MyAppName "DOSBox-X"
#define MyAppVersion "0.801"
#define MyAppURL "https://github.com/joncampbell123/dosbox-x"
#define MyAppExeName "dosbox-x.exe"
#define MyAppBuildDate GetDateTimeString('yyyymmdd_hhnnss', '', '')

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppVersion={#MyAppVersion}
;AppVerName={#MyAppName} {#MyAppVersion}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DisableProgramGroupPage=yes
LicenseFile=..\COPYING
InfoBeforeFile=setup_preamble.txt
;InfoAfterFile=setup_epilogue.txt
OutputDir=.\
SetupIconFile=.\dosbox.ico
Compression=lzma
SolidCompression=yes
UninstallDisplayIcon={app}\{#MyAppExeName}
WizardSmallImageFile=.\dosbox.bmp

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: files; Name: "{app}\stderr.txt"