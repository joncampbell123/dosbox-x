#define MyAppName "DOSBox-X"
#define MyAppVersion "0.83.6"
#define MyAppPublisher "joncampbell123"
#define MyAppURL "https://dosbox-x.com/"
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
SetupIconFile=..\..\icons\dosbox-x.ico
Compression=lzma
SolidCompression=yes
UsePreviousAppDir=yes
ChangesAssociations=yes
DisableStartupPrompt=yes
DisableWelcomePage=no
DisableDirPage=no
DisableReadyPage=no
AlwaysShowDirOnReadyPage=yes
AlwaysShowGroupOnReadyPage=yes
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=lowest
UninstallDisplayIcon={app}\{#MyAppExeName}
WizardSmallImageFile=..\..\icons\dosbox-x.bmp

[Messages]
InfoBeforeLabel=Please read the general information about DOSBox-X below.
InfoAfterClickLabel=You have now installed DOSBox-X. Please note that you can customize DOSBox-X settings in dosbox-x.conf. Also, when in the DOSBox-X command line, you can type HELP for DOSBox-X help, or EXIT to close the DOSBox-X window.

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "contextmenu"; Description: "Add ""Run/Open with DOSBox-X"" context menu for Windows Explorer"
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Types]
Name: "full"; Description: "Full installation"
Name: "compact"; Description: "Compact installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "compact"; Description: "Install default build";   Types: full compact custom; Flags: fixed
Name: "full"; Description: "Copy all builds to subdirectories";   Types: full

[Files]
Source: ".\readme.txt"; DestDir: "{app}"; DestName: "README.txt"; Flags: ignoreversion; Components: full compact
Source: ".\dosbox-x.reference.conf"; DestDir: "{app}"; Flags: ignoreversion; Components: full compact
Source: "..\..\..\CHANGELOG"; DestDir: "{app}"; DestName: "changelog.txt"; Flags: ignoreversion; Components: full compact
Source: "..\..\..\COPYING"; DestDir: "{app}"; DestName: "COPYING.txt"; Flags: ignoreversion; Components: full compact
Source: "..\..\fonts\FREECG98.BMP"; DestDir: "{app}"; Flags: ignoreversion; Components: full compact
Source: "..\..\glshaders\*"; DestDir: "{app}\glshaders"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: full compact
Source: "..\shaders\*"; DestDir: "{app}\shaders"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: full compact
Source: ".\drivez_readme.txt"; DestDir: "{app}\drivez"; DestName: "README.TXT"; Flags: ignoreversion; Components: full compact
Source: "Win32_builds\x86_Release\dosbox-x.exe"; DestDir: "{app}"; Flags: ignoreversion; Check: CheckDirName('Win32_builds\x86_Release'); Components: full compact
Source: "Win32_builds\x86_Release_SDL2\dosbox-x.exe"; DestDir: "{app}"; Flags: ignoreversion; Check: CheckDirName('Win32_builds\x86_Release_SDL2'); Components: full compact
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
Source: ".\windows_explorer_context_menu_installer.bat"; DestDir: "{app}\scripts"; DestName: "windows_explorer_context_menu_installer.bat"; Flags: ignoreversion; Components: full compact
Source: ".\windows_explorer_context_menu_uninstaller.bat"; DestDir: "{app}\scripts"; DestName: "windows_explorer_context_menu_uninstaller.bat"; Flags: ignoreversion; Components: full compact
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\View DOSBox-X README file"; Filename: "{app}\README.TXT"
Name: "{group}\View DOSBox-X Wiki guide"; Filename: "https://github.com/joncampbell123/dosbox-x/wiki"
Name: "{group}\View or edit dosbox-x.conf"; Filename: "notepad.exe"; Parameters: "{app}\dosbox-x.conf"
Name: "{group}\Run DOSBox-X Configuration Tool"; Filename: "{app}\{#MyAppExeName}"; Parameters: "-startui"
Name: "{group}\Run DOSBox-X Mapper Editor"; Filename: "{app}\{#MyAppExeName}"; Parameters: "-startmapper"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: quicklaunchicon

[Registry]
Root: HKCU; Subkey: "Software\Classes\Directory\shell\DOSBox-X"; ValueType: string; ValueName: ""; ValueData: "Open with DOSBox-X"; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\Directory\shell\DOSBox-X"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\dosbox-x.exe"",0"; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Directory\shell\DOSBox-X\command"; ValueType: string; ValueName: ""; ValueData: """{app}\dosbox-x.exe"" -defaultdir ""{app} "" ""%v """; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Directory\Background\shell\DOSBox-X"; ValueType: string; ValueName: ""; ValueData: "Open with DOSBox-X"; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\Directory\Background\shell\DOSBox-X"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\dosbox-x.exe"",0"; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Directory\Background\shell\DOSBox-X\command"; ValueType: string; ValueName: ""; ValueData: """{app}\dosbox-x.exe"" -defaultdir ""{app} "" ""%v """; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.exe\shell\Run with DOSBox-X"; ValueType: none; ValueName: ""; ValueData: ""; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.exe\shell\Run with DOSBox-X"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\dosbox-x.exe"",0"; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.exe\shell\Run with DOSBox-X\command"; ValueType: string; ValueName: ""; ValueData: """{app}\dosbox-x.exe"" -fastlaunch -defaultdir ""{app} "" ""%1"""; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.com\shell\Run with DOSBox-X"; ValueType: none; ValueName: ""; ValueData: ""; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.com\shell\Run with DOSBox-X"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\dosbox-x.exe"",0"; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.com\shell\Run with DOSBox-X\command"; ValueType: string; ValueName: ""; ValueData: """{app}\dosbox-x.exe"" -fastlaunch -defaultdir ""{app} "" ""%1"""; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bat\shell\Run with DOSBox-X"; ValueType: none; ValueName: ""; ValueData: ""; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bat\shell\Run with DOSBox-X"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\dosbox-x.exe"",0"; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bat\shell\Run with DOSBox-X\command"; ValueType: string; ValueName: ""; ValueData: """{app}\dosbox-x.exe"" -fastlaunch -defaultdir ""{app} "" ""%1"""; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.conf\shell\Open with DOSBox-X"; ValueType: none; ValueName: ""; ValueData: ""; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.conf\shell\Open with DOSBox-X"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\dosbox-x.exe"",0"; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.conf\shell\Open with DOSBox-X\command"; ValueType: string; ValueName: ""; ValueData: """{app}\dosbox-x.exe"" -conf ""%1"""; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Directory\shell\DOSBox-X"; ValueType: none; Check: not IsTaskSelected('contextmenu'); Flags: deletekey
Root: HKCU; Subkey: "Software\Classes\Directory\shell\DOSBox-X\command"; ValueType: none; Check: not IsTaskSelected('contextmenu'); Flags: deletekey
Root: HKCU; Subkey: "Software\Classes\Directory\Background\shell\DOSBox-X"; ValueType: none; Check: not IsTaskSelected('contextmenu'); Flags: deletekey
Root: HKCU; Subkey: "Software\Classes\Directory\Background\shell\DOSBox-X\command"; ValueType: none; Check: not IsTaskSelected('contextmenu'); Flags: deletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.exe\shell\Run with DOSBox-X"; ValueType: none; Check: not IsTaskSelected('contextmenu'); Flags: deletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.exe\shell\Run with DOSBox-X\command"; ValueType: none; Check: not IsTaskSelected('contextmenu'); Flags: deletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.com\shell\Run with DOSBox-X"; ValueType: none; Check: not IsTaskSelected('contextmenu'); Flags: deletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.com\shell\Run with DOSBox-X\command"; ValueType: none; Check: not IsTaskSelected('contextmenu'); Flags: deletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bat\shell\Run with DOSBox-X"; ValueType: none; Check: not IsTaskSelected('contextmenu'); Flags: deletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bat\shell\Run with DOSBox-X\command"; ValueType: none; Check: not IsTaskSelected('contextmenu'); Flags: deletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.conf\shell\Open with DOSBox-X"; ValueType: none; Check: not IsTaskSelected('contextmenu'); Flags: deletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.conf\shell\Open with DOSBox-X\command"; ValueType: none; Check: not IsTaskSelected('contextmenu'); Flags: deletekey

[Run]
Filename: "{app}\readme.txt"; Description: "View README.txt"; Flags: waituntilterminated runascurrentuser postinstall shellexec skipifsilent
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: files; Name: "{app}\stderr.txt"

[Code]
var
  msg: string;
  build64: Boolean;    
  HelpButton: TNewButton;
  PageBuild, PageVer: TInputOptionWizardPage;
function IsWindowsVersionOrNewer(Major, Minor: Integer): Boolean;
var
  Version: TWindowsVersion;
begin
  GetWindowsVersionEx(Version);
  Result :=
    (Version.Major > Major) or
    ((Version.Major = Major) and (Version.Minor >= Minor));
end;
procedure HelpButtonOnClick(Sender: TObject);
begin
  MsgBox('The Setup pre-selects a Windows build for you according to your platform automatically, but you can change the default build to run if you encounter specific problem(s) with the pre-selected one.' #13#13 'For example, while the SDL1 version is the default version to run, the SDL2 version may be preferred over the SDL1 version for certain features such as touchscreen input support. Also, MinGW builds may be used for lower-end systems.' #13#13 'If you are not sure about which build to use, then you can just leave it unmodified and use the pre-selected one as the default build.', mbConfirmation, MB_OK);
end;
procedure CreateHelpButton(X: integer; Y: integer; W: integer; H: integer);
begin
  HelpButton         := TNewButton.Create(WizardForm);
  HelpButton.Left    := X;
  HelpButton.Top     := Y;
  HelpButton.Width   := W;
  HelpButton.Height  := H;
  HelpButton.Caption := '&Help';
  HelpButton.OnClick := @HelpButtonOnClick;
  HelpButton.Parent  := WizardForm;
end;
procedure InitializeWizard();
begin
    msg:='The selected build will be the default build when you run DOSBox-X from the Windows Start Menu or the desktop. Click the "Help" button for more information about this.';
    PageBuild:=CreateInputOptionPage(wpSelectDir, 'Default DOSBox-X build', 'Select the default DOSBox-X build to run', msg, True, False);
    PageBuild.Add('Windows Release SDL1');
    PageBuild.Add('Windows Release SDL2');
    PageBuild.Add('Windows ARM SDL1 (ARM platform only)');
    PageBuild.Add('Windows ARM SDL2 (ARM platform only)');
    PageBuild.Add('MinGW build SDL1');
    PageBuild.Add('MinGW build for lower-end systems');
    PageBuild.Add('MinGW build SDL2');
    PageBuild.Add('MinGW build with custom drawn menu');
    if IsX86 or IsX64 then
    begin
      PageBuild.CheckListBox.ItemEnabled[2] := False;
      PageBuild.CheckListBox.ItemEnabled[3] := False;
    end
    if IsARM64 then
      begin
        PageBuild.Values[2] := True;
      end
    else if IsWindowsVersionOrNewer(6, 0) then
      begin
        PageBuild.Values[0] := True;
      end
    else
      PageBuild.Values[4] := True;
    CreateHelpButton(ScaleX(20), WizardForm.CancelButton.Top, WizardForm.CancelButton.Width, WizardForm.CancelButton.Height);
    msg:='You can specify a default DOS version for DOSBox-X to report to itself and DOS programs. This can sometimes change the feature sets of DOSBox-X. For example, selecting 7.10 as the reported DOS version will enable support for Windows-style long filenames (LFN) and FAT32 disk images (>2GB disk images) by default.' #13#13 'If you are not sure about which DOS version to report, you can also leave this unselected, then a preset DOS version will be reported (usually 5.00).' #13#13 'This setting can be later modified in the DOSBox-X''s configuration file (dosbox-x.conf).';
    PageVer:=CreateInputOptionPage(100, 'Reported DOS version', 'Specify the default DOS version to report', msg, True, False);
    PageVer.Add('DOS version 3.30');
    PageVer.Add('DOS version 5.00');
    PageVer.Add('DOS version 6.22');
    PageVer.Add('DOS version 7.10');
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
function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := (PageID = 101) and FileExists(ExpandConstant('{app}\dosbox-x.conf'));
end;
procedure CurPageChanged(CurPageID: Integer);
begin
  HelpButton.Visible:=CurPageID=100;
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
    if (PageBuild.Values[0]) then
      msg:=msg+'Windows Release SDL1';
    if (PageBuild.Values[1]) then
      msg:=msg+'Windows Release SDL2';
    if (PageBuild.Values[2]) then
      msg:=msg+'Windows ARM SDL1';
    if (PageBuild.Values[3]) then
      msg:=msg+'Windows ARM SDL2';
    if (PageBuild.Values[4]) then
      msg:=msg+'MinGW build SDL1';
    if (PageBuild.Values[5]) then
      msg:=msg+'MinGW for lower-end systems';
    if (PageBuild.Values[6]) then
      msg:=msg+'MinGW build SDL2';
    if (PageBuild.Values[7]) then
      msg:=msg+'MinGW build with custom drawn menu';
    Wizardform.ReadyMemo.Lines.Add('      '+msg);
    if PageVer.Values[0] or PageVer.Values[1] or PageVer.Values[2] or PageVer.Values[3] then
    begin
      Wizardform.ReadyMemo.Lines.Add('');
      Wizardform.ReadyMemo.Lines.Add('Reported DOS version:');
      msg:='Default';
      if (PageVer.Values[0]) then
        msg:='3.30';
      if (PageVer.Values[1]) then
        msg:='5.00';
      if (PageVer.Values[2]) then
        msg:='6.22';
      if (PageVer.Values[3]) then
        msg:='7.10';
      Wizardform.ReadyMemo.Lines.Add('      '+msg);
    end
  end;
end;
procedure CurStepChanged(CurrentStep: TSetupStep);
var
  vsection: Boolean;
  i, j, k, res: Integer;
  section, line, linetmp, lineold, linenew: String;
  FileLines, FileLinesold, FileLinesnew, FileLinesave: TStringList;
begin
  if (CurrentStep = ssPostInstall) then
  begin
    if not FileExists(ExpandConstant('{app}\dosbox-x.reference.conf')) then
    begin
      MsgBox('Cannot find the dosbox-x.reference.conf file.', mbError, MB_OK);
      Exit;
    end
    if not FileExists(ExpandConstant('{app}\dosbox-x.conf')) then
    begin
      FileCopy(ExpandConstant('{app}\dosbox-x.reference.conf'), ExpandConstant('{app}\dosbox-x.conf'), false);
      if FileExists(ExpandConstant('{app}\dosbox-x.conf')) and (PageVer.Values[0] or PageVer.Values[1] or PageVer.Values[2] or PageVer.Values[3]) then
      begin
        FileLines := TStringList.Create;
        FileLines.LoadFromFile(ExpandConstant('{app}\dosbox-x.conf'));
        section := '';
        for i := 0 to FileLines.Count - 1 do
        begin
          line := Trim(FileLines[i]);
          if (Length(line)>2) and (Copy(line, 1, 1) = '[') and (Copy(line, Length(line), 1) = ']') then
            section := Copy(line, 2, Length(line)-2);
          if (Length(line)>0) and (Copy(line, 1, 1) <> '#') and (Copy(line, 1, 1) <> '[') and (Pos('=', line) > 1) then
          begin
            linetmp := Trim(Copy(line, 1, Pos('=', line) - 1));
            if (CompareText(linetmp, 'ver') = 0) and (CompareText(section, 'dos') = 0) then
            begin
              if (PageVer.Values[0]) then
                FileLines[i] := line+' 3.3';
              if (PageVer.Values[1]) then
                FileLines[i] := line+' 5.0';
              if (PageVer.Values[2]) then
                FileLines[i] := line+' 6.22';
              if (PageVer.Values[3]) then
                FileLines[i] := line+' 7.1';
              break;
            end
          end
        end
        FileLines.SaveToFile(ExpandConstant('{app}\dosbox-x.conf'));
      end
    end
    else if (CompareStr(GetSHA1OfFile(ExpandConstant('{app}\dosbox-x.conf')), GetSHA1OfFile(ExpandConstant('{app}\dosbox-x.reference.conf'))) <> 0) or (CompareStr(GetMD5OfFile(ExpandConstant('{app}\dosbox-x.conf')), GetMD5OfFile(ExpandConstant('{app}\dosbox-x.reference.conf'))) <> 0) then
    begin
      msg:='The configuration file dosbox-x.conf already exist in the destination. Do you want to keep your current settings?' #13#13 'If you choose "Yes", your current settings will be kept and the file dosbox-x.conf will be automatically upgraded to the latest version format (recommended).' #13#13 'If you choose "No", the dosbox-x.conf file will be reset to the new default configuration, and your old dosbox-x.conf file will be named dosbox-x.conf.old in the installation directory.' #13#13 'If you choose "Cancel", your current dosbox-x.conf file will be kept as is without any modifications.' #13 #13 'In any case, the new default configuration file will be named dosbox-x.reference.conf in the installation directory.';
      res := MsgBox(msg, mbConfirmation, MB_YESNOCANCEL);
      if (res = IDNO) then
      begin
        FileCopy(ExpandConstant('{app}\dosbox-x.conf'), ExpandConstant('{app}\dosbox-x.conf.old'), false);
        FileCopy(ExpandConstant('{app}\dosbox-x.reference.conf'), ExpandConstant('{app}\dosbox-x.conf'), false);
      end
      else if (res = IDYES) then
      begin
        FileCopy(ExpandConstant('{app}\dosbox-x.conf'), ExpandConstant('{app}\dosbox-x.conf.old'), false);
        FileLines := TStringList.Create;
        FileLinesold := TStringList.Create;
        FileLinesold.LoadFromFile(ExpandConstant('{app}\dosbox-x.conf.old'));
        FileLinesnew := TStringList.Create;
        FileLinesnew.LoadFromFile(ExpandConstant('{app}\dosbox-x.reference.conf'));
        FileLinesave := TStringList.Create;
        vsection := False;
        for j := 0 to FileLinesold.Count - 1 do
        begin
          lineold := Trim(FileLinesold[j]);
          if (Length(lineold)>2) and (Copy(lineold, 1, 1) = '[') and (Copy(lineold, Length(lineold), 1) = ']') and (Copy(lineold, 2, Length(lineold)-2) = 'video') then
          begin
            vsection := True;
            break;
          end
        end
        section := '';
        for i := 0 to FileLinesnew.Count - 1 do
        begin
          linenew := Trim(FileLinesnew[i]);
          if (Length(linenew)>2) and (Copy(linenew, 1, 1) = '[') and (Copy(linenew, Length(linenew), 1) = ']') then
          begin
            FileLinesave.add(linenew);
            section := Copy(linenew, 2, Length(linenew)-2);
            for j := 0 to FileLinesold.Count - 1 do
            begin
              lineold := Trim(FileLinesold[j]);
              if (Length(lineold)>2) and (Copy(lineold, 1, 1) = '[') and (Copy(lineold, Length(lineold), 1) = ']') and ((((CompareText(section, 'video') <> 0) or (vsection)) and (section = Copy(lineold, 2, Length(lineold)-2))) or ((not vsection) and (CompareText(section, 'video') = 0) and (CompareText(Copy(lineold, 2, Length(lineold)-2), 'dosbox') = 0))) then
              begin
                FileLines := TStringList.Create;
                for k := j+1 to FileLinesold.Count - 1 do
                begin
                  lineold := Trim(FileLinesold[k]);
                  if (Length(lineold)>2) and (Copy(lineold, 1, 1) = '[') and (Copy(lineold, Length(lineold), 1) = ']') then
                  begin
                    break;
                  end
                  if (CompareText(section, '4dos') = 0) or (CompareText(section, 'config') = 0) or (CompareText(section, 'autoexec') = 0) then
                  begin
                    if (Length(lineold)>0) or (FileLines.Count>0) then
                      FileLinesave.add(FileLinesold[k]);
                    if (Length(lineold)>0) then
                      FileLines.add(lineold);
                  end
                  else if (Length(lineold)>0) and (Copy(lineold, 1, 1) <> '#') then
                    FileLines.add(lineold);
                end
                break;
              end
            end
          end
          else if (CompareText(section, '4dos') = 0) or (CompareText(section, 'config') = 0) or (CompareText(section, 'autoexec') = 0) then
          begin
            if (FileLines.Count=0) then
              FileLinesave.add(FileLinesnew[i]);
            continue;
          end
          else if (Length(linenew)=0) or (Copy(linenew, 1, 1) = '#') then
          begin
            FileLinesave.add(FileLinesnew[i]);
            continue;
          end
          else if (Length(section)>0) and (Length(linenew)>0) and (Pos('=', linenew) > 1) then
          begin
            res := 0;
            linetmp := Copy(linenew, 1, Pos('=', linenew) - 1);
            for j := 0 to FileLines.Count - 1 do
            begin
              lineold := Trim(FileLines[j]);
              if (Length(lineold)>0) and (Pos('=', lineold) > 1) and (CompareText(Trim(linetmp), Trim(Copy(lineold, 1, Pos('=', lineold) - 1))) = 0) then
              begin
                res := 1;
                FileLinesave.add(linetmp + '= ' + Trim(Copy(lineold, Pos('=', lineold) + 1, Length(lineold))));
                FileLines.Delete(j);
                break;
              end
            end
            if (res = 0) then
              FileLinesave.add(linenew);
          end
        end
        FileLinesave.SaveToFile(ExpandConstant('{app}\dosbox-x.conf'));
        FileLinesold.free;
        FileLinesnew.free;
        DeleteFile(ExpandConstant('{app}\dosbox-x.conf.old'));
      end
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
        if (PageBuild.Values[0]) then
          dir:=dir+'x64_Release';
        if (PageBuild.Values[1]) then
          dir:=dir+'x64_Release_SDL2';
        if (PageBuild.Values[2]) then
          dir:=dir+'ARM64\Release';
        if (PageBuild.Values[3]) then
          dir:=dir+'ARM64\Release_SDL2';
      end
    else
      begin
        dir:='Win32_builds\';
        if (PageBuild.Values[0]) then
          dir:=dir+'x86_Release';
        if (PageBuild.Values[1]) then
          dir:=dir+'x86_Release_SDL2';
        if (PageBuild.Values[2]) then
          dir:=dir+'ARM_Release';
        if (PageBuild.Values[3]) then
          dir:=dir+'ARM_Release_SDL2';
      end
    if (PageBuild.Values[4]) then
      dir:=dir+'mingw';
    if (PageBuild.Values[5]) then
      dir:=dir+'mingw-lowend';
    if (PageBuild.Values[6]) then
      dir:=dir+'mingw-sdl2';
    if (PageBuild.Values[7]) then
      dir:=dir+'mingw-sdldraw';
    Result := False;
    if (dir=name) then
      Result := True;
end;
