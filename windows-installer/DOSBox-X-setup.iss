#define MyAppName "DOSBox-X"
#define MyAppVersion "0.83.2"
#define MyAppPublisher "joncampbell123"
#define MyAppURL "http://dosbox-x.com/"
#define MyAppExeName "dosbox-x.exe"
#define MyAppBuildDate GetDateTimeString('yyyymmdd_hhnnss', '', '')

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{63E5D76D-0092-415C-B97C-E0D2F4F6D2EC}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
;AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={sd}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
InfoBeforeFile=setup_preamble.txt
InfoAfterFile=setup_epilogue.txt
OutputDir=.\
OutputBaseFilename=DOSBox-X-setup
SetupIconFile=.\dosbox-x.ico
Compression=lzma
SolidCompression=yes
UsePreviousAppDir=yes
DisableStartupPrompt=yes
DisableWelcomePage=no
DisableDirPage=no
DisableReadyPage=no
AlwaysShowDirOnReadyPage=yes
AlwaysShowGroupOnReadyPage=yes
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=lowest
UninstallDisplayIcon={app}\{#MyAppExeName}
WizardSmallImageFile=.\dosbox-x.bmp

[Messages]
InfoBeforeLabel=Please read the general information about DOSBox-X below.
InfoAfterClickLabel=You have now installed DOSBox-X. Please note that you can customize DOSBox-X settings in dosbox-x.conf. Also, when in the DOSBox-X command line, you can type HELP for DOSBox-X help, or EXIT to close the DOSBox-X window.

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; OnlyBelowVersion: 0,6.1

[Types]
Name: "full"; Description: "Full installation"
Name: "compact"; Description: "Compact installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "compact"; Description: "Install default build";   Types: full compact
Name: "full"; Description: "Copy all builds to subdirectories";   Types: full

[Files]
Source: ".\readme.txt"; DestDir: "{app}"; DestName: "README.txt"; Flags: ignoreversion; Components: full compact
Source: "..\CHANGELOG"; DestDir: "{app}"; DestName: "changelog.txt"; Flags: ignoreversion; Components: full compact
Source: "..\COPYING"; DestDir: "{app}"; DestName: "COPYING.txt"; Flags: ignoreversion; Components: full compact
Source: "..\dosbox-x.reference.conf"; DestDir: "{app}"; Flags: ignoreversion; Components: full compact
Source: "..\font\FREECG98.BMP"; DestDir: "{app}"; Flags: ignoreversion; Components: full compact
Source: "..\shaders\*"; DestDir: "{app}\shaders"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: full compact
Source: "Win32_builds\x86_Release\dosbox-x.exe"; DestDir: "{app}"; Flags: ignoreversion; Check: CheckDirName('Win32_builds\x86_Release'); Components: full compact
Source: "Win32_builds\x86_Release_SDL2\dosbox-x.exe"; DestDir: "{app}"; Flags: ignoreversion; Check: CheckDirName('Win32_builds\x86_Release SDL2'); Components: full compact
Source: "Win32_builds\ARM_Release\dosbox-x.exe"; DestDir: "{app}"; Flags: ignoreversion; Check: CheckDirName('Win32_builds\ARM_Release'); Components: full compact
Source: "Win32_builds\ARM_Release_SDL2\dosbox-x.exe"; DestDir: "{app}"; Flags: ignoreversion; Check: CheckDirName('Win32_builds\ARM_Release_SDL2'); Components: full compact
Source: "Win32_builds\mingw\dosbox-x.exe"; DestDir: "{app}"; Flags: ignoreversion; Check: CheckDirName('Win32_builds\mingw'); Components: full compact
Source: "Win32_builds\mingw-lowend\dosbox-x.exe"; DestDir: "{app}"; Flags: ignoreversion; Check: CheckDirName('Win32_builds\mingw-lowend'); Components: full compact
Source: "Win32_builds\mingw-sdl2\dosbox-x.exe"; DestDir: "{app}"; Flags: ignoreversion; Check: CheckDirName('Win32_builds\mingw-sdl2'); Components: full compact
Source: "Win32_builds\mingw-sdldraw\dosbox-x.exe"; DestDir: "{app}"; Flags: ignoreversion; Check: CheckDirName('Win32_builds\mingw-sdldraw'); Components: full compact
Source: "Win64_builds\x64_Release\dosbox-x.exe"; DestDir: "{app}"; Flags: ignoreversion; Check: CheckDirName('Win64_builds\x64_Release'); Components: full compact
Source: "Win64_builds\x64_Release_SDL2\dosbox-x.exe"; DestDir: "{app}"; Flags: ignoreversion; Check: CheckDirName('Win64_builds\x64_Release_SDL2'); Components: full compact
Source: "Win64_builds\ARM64_Release\dosbox-x.exe"; DestDir: "{app}"; Flags: ignoreversion; Check: CheckDirName('Win64_builds\ARM64_Release'); Components: full compact
Source: "Win64_builds\ARM64_Release_SDL2\dosbox-x.exe"; DestDir: "{app}"; Flags: ignoreversion; Check: CheckDirName('Win64_builds\ARM64_Release_SDL2'); Components: full compact
Source: "Win64_builds\mingw\dosbox-x.exe"; DestDir: "{app}"; Flags: ignoreversion; Check: CheckDirName('Win64_builds\mingw'); Components: full compact
Source: "Win64_builds\mingw-lowend\dosbox-x.exe"; DestDir: "{app}"; Flags: ignoreversion; Check: CheckDirName('Win64_builds\mingw-lowend'); Components: full compact
Source: "Win64_builds\mingw-sdl2\dosbox-x.exe"; DestDir: "{app}"; Flags: ignoreversion; Check: CheckDirName('Win64_builds\mingw-sdl2'); Components: full compact
Source: "Win64_builds\mingw-sdldraw\dosbox-x.exe"; DestDir: "{app}"; Flags: ignoreversion; Check: CheckDirName('Win64_builds\mingw-sdldraw'); Components: full compact
Source: "Win32_builds\*"; DestDir: "{app}\Win32_builds"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: full
Source: "Win64_builds\*"; DestDir: "{app}\Win64_builds"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: IsWin64; Components: full
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\View or edit dosbox-x.conf"; Filename: "{app}\dosbox-x.conf"
Name: "{group}\View DOSBox-X README file"; Filename: "{app}\README.TXT"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: quicklaunchicon

[Run]
Filename: "{app}\readme.txt"; Description: "View README.txt"; Flags: waituntilterminated runascurrentuser postinstall shellexec skipifsilent
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: files; Name: "{app}\stderr.txt"

[Code]
var
  msg: string;
  build64: Boolean;    
  Page: TInputOptionWizardPage;
procedure InitializeWizard();
begin
    msg:='The selected build will be the default build when you run DOSBox-X from the Windows Start Menu or the desktop. ';
    Page:=CreateInputOptionPage(wpSelectDir, 'Default DOSBox-X build', 'Select the default DOSBox-X build to run', msg, True, False);
    Page.Add('Windows Release SDL1');
    Page.Add('Windows Release SDL2');
    Page.Add('Windows ARM SDL1 (ARM platform only)');
    Page.Add('Windows ARM SDL2 (ARM platform only)');
    Page.Add('MinGW build SDL1');
    Page.Add('MinGW build for lowend systems');
    Page.Add('MinGW build SDL2');
    Page.Add('MinGW build with custom drawn menu');
    Page.Values[0] := True;
    if IsX86 or IsX64 then
    begin
      Page.CheckListBox.ItemEnabled[2] := False;
      Page.CheckListBox.ItemEnabled[3] := False;
    end
end;
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID=wpSelectDir) then
  begin
    build64:=False;
    if IsWin64 then
    begin
      if (MsgBox('You are running 64-bit Windows. Do you want to install 64-bit version of DOSBox-X, which is recommended?' #13#13 'If you select No, then 32-bit version of DOSBox-X will be installed.', mbInformation, MB_YESNO) = IDYES) then
        build64:=True;
    end;
  end;
end;
procedure CurPageChanged(CurPageID: Integer);
begin
  if (CurPageID=wpSelectDir) then
  begin
    if (IsAdminLoggedOn or IsPowerUserLoggedOn) and (WizardDirValue=ExpandConstant('{localappdata}\{#MyAppName}')) then
    begin
      WizardForm.DirEdit.Text:=ExpandConstant('{commonappdata}\{#MyAppName}');
      MsgBox('You had previously installed DOSBox-X in a standard user directory, but you are currently running as an administrator or privileged user.' #13#13 'The installer will automatically change the default install directory.', mbConfirmation, MB_OK);
    end
    else if not (IsAdminLoggedOn or IsPowerUserLoggedOn) and (WizardDirValue=ExpandConstant('{pf}\{#MyAppName}')) then
    begin
      WizardForm.DirEdit.Text:=ExpandConstant('{commonappdata}\{#MyAppName}');
      MsgBox('You had installed DOSBox-X in ' + ExpandConstant('{pf}\{#MyAppName}') + ' previously, but you are currently running as an unprivileged user.' #13#13 'The installer will automatically change the default install directory, or you might want to run the installer as an administrator instead.', mbConfirmation, MB_OK);
    end
  end
  else if (CurPageID = wpReady) then
  begin
    Wizardform.ReadyMemo.Lines.Add('');
    Wizardform.ReadyMemo.Lines.Add('Default build:');
    if (build64) then
      begin
        msg:='64';
      end
    else
      msg:='32';
    msg:=msg+'-bit ';
    if (Page.Values[0]) then
      msg:=msg+'Windows Release SDL1';
    if (Page.Values[1]) then
      msg:=msg+'Windows Release SDL2';
    if (Page.Values[2]) then
      msg:=msg+'Windows ARM SDL1';
    if (Page.Values[3]) then
      msg:=msg+'Windows ARM SDL2';
    if (Page.Values[4]) then
      msg:=msg+'MinGW build SDL1';
    if (Page.Values[5]) then
      msg:=msg+'MinGW for lowend systems';
    if (Page.Values[6]) then
      msg:=msg+'MinGW build SDL2';
    if (Page.Values[7]) then
      msg:=msg+'MinGW build with custom drawn menu';
    Wizardform.ReadyMemo.Lines.Add('      '+msg);
  end;
end;
procedure CurStepChanged(CurrentStep: TSetupStep);
begin
  if (CurrentStep = ssPostInstall) then
  begin
    msg:='The configuration file dosbox-x.conf already exist in the destination. Do you want to keep your current settings?' #13#13 'If you choose "Yes", the new default files will be named dosbox-x.reference.conf in the installation directory.' #13#13 'If you choose "No", your old files will be renamed dosbox-x.conf.old in the installation directory instead.';
    if not FileExists(ExpandConstant('{app}\dosbox-x.conf')) then
    begin
      FileCopy(ExpandConstant('{app}\dosbox-x.reference.conf'), ExpandConstant('{app}\dosbox-x.conf'), false);
    end
    else if FileExists(ExpandConstant('{app}\dosbox-x.conf')) and (MsgBox(msg, mbConfirmation, MB_YESNO) = IDNO) then
    begin
      FileCopy(ExpandConstant('{app}\dosbox-x.conf'), ExpandConstant('{app}\dosbox-x.conf.old'), false);
      FileCopy(ExpandConstant('{app}\dosbox-x.reference.conf'), ExpandConstant('{app}\dosbox-x.conf'), false);
    end
  end;
end;
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  case CurUninstallStep of
    usUninstall:
    begin
      if FileExists(ExpandConstant('{app}\dosbox-x.conf.old')) then
        DeleteFile(ExpandConstant('{app}\dosbox-x.conf.old'));
      msg:='Do you want to keep the configuration file dosbox-x.conf?';
      if FileExists(ExpandConstant('{app}\dosbox-x.conf')) and (MsgBox(msg, mbConfirmation, MB_YESNO) = IDNO) then
        DeleteFile(ExpandConstant('{app}\dosbox-x.conf'));
    end
  end;
end;
function CheckDirName(name: String): Boolean;
var
  dir: string;
begin
    if (build64) then
      begin
        dir:='Win64_builds\';
        if (Page.Values[0]) then
          dir:=dir+'x64_Release';
        if (Page.Values[1]) then
          dir:=dir+'x64_Release_SDL2';
        if (Page.Values[2]) then
          dir:=dir+'ARM64\Release';
        if (Page.Values[3]) then
          dir:=dir+'ARM64\Release_SDL2';
      end
    else
      begin
        dir:='Win32_builds\';
        if (Page.Values[0]) then
          dir:=dir+'x86_Release';
        if (Page.Values[1]) then
          dir:=dir+'x86_Release_SDL2';
        if (Page.Values[2]) then
          dir:=dir+'ARM_Release';
        if (Page.Values[3]) then
          dir:=dir+'ARM_Release_SDL2';
      end
    if (Page.Values[4]) then
      dir:=dir+'mingw';
    if (Page.Values[5]) then
      dir:=dir+'mingw-lowend';
    if (Page.Values[6]) then
      dir:=dir+'mingw-sdl2';
    if (Page.Values[7]) then
      dir:=dir+'mingw-sdldraw';
    Result := False;
    if (dir=name) then
      Result := True;
end;