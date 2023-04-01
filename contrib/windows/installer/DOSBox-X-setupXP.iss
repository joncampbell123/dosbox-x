#define MyAppName "DOSBox-X"
#define MyAppVersion "2023.03.31"
#define MyAppBit "(32/64bit for XP)"
#define MyAppPublisher "joncampbell123 [DOSBox-X Team]"
#define MyAppURL "https://dosbox-x.com/"
#define MyAppExeName "dosbox-x.exe"
#define MyAppBuildDate GetDateTimeString('yyyymmdd_hhnnss', '', '')

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{63E5D76D-0092-415C-B97C-E0D2F4F6D2EC}
AppName={#MyAppName}
AppVersion={#MyAppVersion} {#MyAppBit}
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
OutputBaseFilename=dosbox-x-winXP-{#MyAppVersion}-setup
SetupIconFile=..\..\icons\dosbox-x.ico
Compression=lzma
SolidCompression=yes
UsePreviousAppDir=yes
ChangesAssociations=yes
DisableStartupPrompt=yes
DisableWelcomePage=no
DisableDirPage=no
DisableReadyPage=no
ShowLanguageDialog=no
AlwaysShowDirOnReadyPage=yes
AlwaysShowGroupOnReadyPage=yes
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=lowest
;PrivilegesRequiredOverridesAllowed=dialog
UninstallDisplayName={#MyAppName} {#MyAppVersion} {#MyAppBit}
UninstallDisplayIcon={app}\{#MyAppExeName}
WizardSmallImageFile=..\..\icons\dosbox-x.bmp
MinVersion=5.1

[Messages]
InfoBeforeLabel=Please read the general information about DOSBox-X below.
InfoAfterClickLabel=You have now installed DOSBox-X. Please note that you can customize DOSBox-X settings in dosbox-x.conf. Also, when in the DOSBox-X command line, you can type HELP for DOSBox-X help, or EXIT to close the DOSBox-X window.

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "fr"; MessagesFile: "compiler:Default.isl"
Name: "ge"; MessagesFile: "compiler:Default.isl"
Name: "ja"; MessagesFile: "compiler:Default.isl"
Name: "ko"; MessagesFile: "compiler:Default.isl"
Name: "pt"; MessagesFile: "compiler:Default.isl"
Name: "sc"; MessagesFile: "compiler:Default.isl"
Name: "sp"; MessagesFile: "compiler:Default.isl"
Name: "tc"; MessagesFile: "compiler:Default.isl"
Name: "tr"; MessagesFile: "compiler:Default.isl"

[LangOptions]
en.LanguageID=$0409
fr.LanguageID=$040C
ge.LanguageID=$0407
ja.LanguageID=$0411
ko.LanguageID=$0412
pt.LanguageID=$0416
sc.LanguageID=$0804
sp.LanguageID=$0C0A
tc.LanguageID=$0404
tr.LanguageID=$041F

[Tasks]
Name: "contextmenu"; Description: "Add ""Run/Open with DOSBox-X"" context menu for Windows Explorer"
Name: "commonoption"; Description: "Write common config options (instead of all) to the configuration file"
Name: "centerwindow"; Description: "Automatically center the window on the screen when DOSBox-X starts"
Name: "drivedelay"; Description: "Emulate the slowness of floppy drive and hard drive data transfers"; Flags: unchecked
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Types]
Name: "typical"; Description: "Typical installation"
Name: "full"; Description: "Full installation"
Name: "compact"; Description: "Compact installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "compact"; Description: "Install core files"; Types: full typical compact custom; Flags: fixed
Name: "typical"; Description: "Include typical components (such as shaders)"; Types: full typical custom
Name: "full"; Description: "Copy all DOSBox-X builds to sub-directories"; Types: full

[Files]
Source: ".\readme.txt"; DestDir: "{app}"; DestName: "README.txt"; Flags: ignoreversion; Components: full typical compact
Source: ".\dosbox-x.reference.conf"; DestDir: "{app}"; Flags: ignoreversion; Components: full typical compact
Source: ".\dosbox-x.reference.full.conf"; DestDir: "{app}"; Flags: ignoreversion; Components: full typical compact
Source: ".\dosbox-x.reference.setup.conf"; DestDir: "{app}"; Flags: ignoreversion; Components: full typical compact
Source: "..\..\..\CHANGELOG"; DestDir: "{app}"; DestName: "changelog.txt"; Flags: ignoreversion; Components: full typical compact
Source: "..\..\..\COPYING"; DestDir: "{app}"; DestName: "COPYING.txt"; Flags: ignoreversion; Components: full typical compact
Source: "..\..\fonts\FREECG98.BMP"; DestDir: "{app}"; Flags: ignoreversion; Components: full typical
Source: "..\..\fonts\wqy_1?pt.bdf"; DestDir: "{app}"; Flags: ignoreversion; Components: full typical
Source: "..\..\fonts\Nouveau_IBM.ttf"; DestDir: "{app}"; Flags: ignoreversion; Components: full typical compact
Source: "..\..\fonts\SarasaGothicFixed.ttf"; DestDir: "{app}"; Flags: ignoreversion; Components: full typical compact
Source: "..\..\translations\de\de_DE.lng"; DestDir: "{app}\languages"; Flags: ignoreversion; Components: full typical compact
Source: "..\..\translations\en\en_US.lng"; DestDir: "{app}\languages"; Flags: ignoreversion; Components: full typical compact
Source: "..\..\translations\es\es_ES.lng"; DestDir: "{app}\languages"; Flags: ignoreversion; Components: full typical compact
Source: "..\..\translations\fr\fr_FR.lng"; DestDir: "{app}\languages"; Flags: ignoreversion; Components: full typical compact
Source: "..\..\translations\ja\ja_JP.lng"; DestDir: "{app}\languages"; Flags: ignoreversion; Components: full typical compact
Source: "..\..\translations\ko\ko_KR.lng"; DestDir: "{app}\languages"; Flags: ignoreversion; Components: full typical compact
Source: "..\..\translations\tr\tr_TR.lng"; DestDir: "{app}\languages"; Flags: ignoreversion; Components: full typical compact
Source: "..\..\translations\zh\zh_CN.lng"; DestDir: "{app}\languages"; Flags: ignoreversion; Components: full typical compact
Source: "..\..\translations\zh\zh_TW.lng"; DestDir: "{app}\languages"; Flags: ignoreversion; Components: full typical compact
Source: "..\..\glshaders\*"; DestDir: "{app}\glshaders"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: full typical
Source: "..\shaders\*"; DestDir: "{app}\shaders"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: full typical
Source: ".\drivez_readme.txt"; DestDir: "{app}\drivez"; DestName: "README.TXT"; Flags: ignoreversion; Components: full typical
Source: ".\windows_explorer_context_menu_installer.bat"; DestDir: "{app}\scripts"; DestName: "windows_explorer_context_menu_installer.bat"; Flags: ignoreversion; Components: full typical
Source: ".\windows_explorer_context_menu_uninstaller.bat"; DestDir: "{app}\scripts"; DestName: "windows_explorer_context_menu_uninstaller.bat"; Flags: ignoreversion; Components: full typical
Source: ".\inpout32.dll"; DestDir: "{app}"; DestName: "inpout32.dll"; Flags: ignoreversion; Components: full typical
Source: ".\inpoutx64.dll"; DestDir: "{app}"; DestName: "inpoutx64.dll"; Flags: ignoreversion; Components: full typical
Source: ".\WinXP\dosbox-x_XPx64_SDL1.exe"; DestDir: "{app}"; DestName: "dosbox-x.exe"; Flags: ignoreversion; Check: InstallVersion(0); Components: full typical compact
Source: ".\WinXP\dosbox-x_XPx64_SDL2.exe"; DestDir: "{app}"; DestName: "dosbox-x.exe"; Flags: ignoreversion; Check: InstallVersion(1); Components: full typical compact
Source: ".\WinXP\dosbox-x_XPx86_SDL1.exe"; DestDir: "{app}"; DestName: "dosbox-x.exe"; Flags: ignoreversion; Check: InstallVersion(2); Components: full typical compact
Source: ".\WinXP\dosbox-x_XPx86_SDL2.exe"; DestDir: "{app}"; DestName: "dosbox-x.exe"; Flags: ignoreversion; Check: InstallVersion(3); Components: full typical compact
Source: ".\WinXP\dosbox-x_mingw_lowend_SDL1.exe"; DestDir: "{app}"; DestName: "dosbox-x.exe"; Flags: ignoreversion; Check: InstallVersion(4); Components: full typical compact
Source: ".\WinXP\dosbox-x_mingw_lowend_SDL2.exe"; DestDir: "{app}"; DestName: "dosbox-x.exe"; Flags: ignoreversion; Check: InstallVersion(5); Components: full typical compact
Source: ".\WinXP\dosbox-x_XPx64_SDL1.exe"; DestDir: "{app}"; Check: IsWin64; Flags: ignoreversion; Components: full
Source: ".\WinXP\dosbox-x_XPx64_SDL2.exe"; DestDir: "{app}"; Check: IsWin64; Flags: ignoreversion; Components: full
Source: ".\WinXP\dosbox-x_XPx86_SDL1.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: full
Source: ".\WinXP\dosbox-x_XPx86_SDL2.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: full
Source: ".\WinXP\dosbox-x_mingw_lowend_SDL1.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: full
Source: ".\WinXP\dosbox-x_mingw_lowend_SDL2.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: full

; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\View DOSBox-X README file"; Filename: "{app}\README.TXT"
Name: "{group}\View DOSBox-X Wiki guide"; Filename: "https://dosbox-x.com/wiki"
Name: "{group}\View or edit dosbox-x.conf"; Filename: "notepad.exe"; Parameters: "{app}\dosbox-x.conf"
Name: "{group}\Run DOSBox-X Configuration Tool"; Filename: "{app}\{#MyAppExeName}"; Parameters: "-startui"
Name: "{group}\Run DOSBox-X Mapper Editor"; Filename: "{app}\{#MyAppExeName}"; Parameters: "-startmapper"
Name: "{group}\All DOSBox-X builds\x64 Release SDL1"; Filename: "{app}\dosbox-x_XPx64_SDL1.exe"; WorkingDir: "{app}"; Check: IsWin64; Components: full
Name: "{group}\All DOSBox-X builds\x64 Release SDL2"; Filename: "{app}\dosbox-x_XPx64_SDL2.exe"; WorkingDir: "{app}"; Check: IsWin64; Components: full
Name: "{group}\All DOSBox-X builds\x86 Release SDL1"; Filename: "{app}\dosbox-x_XPx86_SDL1.exe"; WorkingDir: "{app}"; Components: full
Name: "{group}\All DOSBox-X builds\x86 Release SDL2"; Filename: "{app}\dosbox-x_XPx86_SDL2.exe"; WorkingDir: "{app}"; Components: full
Name: "{group}\All DOSBox-X builds\32-bit MinGW lowend"; Filename: "{app}\dosbox-x_mingw_lowend_SDL1.exe"; WorkingDir: "{app}"; Components: full
Name: "{group}\All DOSBox-X builds\32-bit MinGW lowend SDL2"; Filename: "{app}\dosbox-x_mingw_lowend_SDL2.exe"; WorkingDir: "{app}"; Components: full
Name: "{code:GetDesktopFolder}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: quicklaunchicon

[InstallDelete]
Type: files; Name: "{group}\Run DOSBox-X Configuration UI.lnk"
Type: files; Name: "{group}\All DOSBox-X builds\x64 Release SDL1.lnk"; Components: typical compact
Type: files; Name: "{group}\All DOSBox-X builds\x64 Release SDL2.lnk"; Components: typical compact
Type: files; Name: "{group}\All DOSBox-X builds\x86 Release SDL1.lnk"; Components: typical compact
Type: files; Name: "{group}\All DOSBox-X builds\x86 Release SDL2.lnk"; Components: typical compact
Type: files; Name: "{group}\All DOSBox-X builds\32-bit MinGW SDL1 lowend.lnk"; Components: typical compact
Type: files; Name: "{group}\All DOSBox-X builds\32-bit MinGW SDL2 lowend.lnk"; Components: typical compact
Type: files; Name: "{group}\All DOSBox-X builds\32-bit MinGW SDL1 drawn.lnk"; Components: typical compact

[Registry]
Root: HKCU; Subkey: "Software\DOSBox-X"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\DOSBox-X"; ValueType: string; ValueName: "Path"; ValueData: "{app}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\DOSBox-X"; ValueType: string; ValueName: "Version"; ValueData: "{#MyAppVersion}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Directory\shell\DOSBox-X"; ValueType: string; ValueName: ""; ValueData: "Open with DOSBox-X"; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\Directory\shell\DOSBox-X"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\dosbox-x.exe"",0"; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Directory\shell\DOSBox-X\command"; ValueType: string; ValueName: ""; ValueData: """{app}\dosbox-x.exe"" -prerun -defaultdir ""{app} "" ""%v """; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Directory\Background\shell\DOSBox-X"; ValueType: string; ValueName: ""; ValueData: "Open with DOSBox-X"; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\Directory\Background\shell\DOSBox-X"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\dosbox-x.exe"",0"; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Directory\Background\shell\DOSBox-X\command"; ValueType: string; ValueName: ""; ValueData: """{app}\dosbox-x.exe"" -prerun -defaultdir ""{app} "" ""%v """; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.exe\shell\Run with DOSBox-X"; ValueType: none; ValueName: ""; ValueData: ""; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.exe\shell\Run with DOSBox-X"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\dosbox-x.exe"",0"; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.exe\shell\Run with DOSBox-X\command"; ValueType: string; ValueName: ""; ValueData: """{app}\dosbox-x.exe"" -fastlaunch -prerun -defaultdir ""{app} "" ""%1"""; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.com\shell\Run with DOSBox-X"; ValueType: none; ValueName: ""; ValueData: ""; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.com\shell\Run with DOSBox-X"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\dosbox-x.exe"",0"; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.com\shell\Run with DOSBox-X\command"; ValueType: string; ValueName: ""; ValueData: """{app}\dosbox-x.exe"" -fastlaunch -prerun -defaultdir ""{app} "" ""%1"""; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bat\shell\Run with DOSBox-X"; ValueType: none; ValueName: ""; ValueData: ""; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bat\shell\Run with DOSBox-X"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\dosbox-x.exe"",0"; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bat\shell\Run with DOSBox-X\command"; ValueType: string; ValueName: ""; ValueData: """{app}\dosbox-x.exe"" -fastlaunch -prerun -defaultdir ""{app} "" ""%1"""; Check: IsTaskSelected('contextmenu'); Flags: uninsdeletekey
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
  msg, lang: string;
  HelpButton: TNewButton;
  PageCustom: TInputFileWizardPage;
  PageBuild, PageLang, PageOutput, PageFont, PageVer: TInputOptionWizardPage;
function IsVerySilent(): Boolean;
var
  k: Integer;
begin
  Result := False;
  for k := 1 to ParamCount do
    if CompareText(ParamStr(k), '/verysilent') = 0 then
    begin
      Result := True;
      Break;
    end;
end;
function IsWindowsVersionOrNewer(Major, Minor: Integer): Boolean;
var
  Version: TWindowsVersion;
begin
  GetWindowsVersionEx(Version);
  Result := (Version.Major > Major) or ((Version.Major = Major) and (Version.Minor >= Minor));
end;
function Is32BitInstaller(): Boolean;
begin
    if True then
    begin
      Result := True;
    end
    else
      Result := False;
end;
procedure HelpButtonOnClick(Sender: TObject);
begin
  MsgBox('The Setup pre-selects a Windows build for you according to your platform automatically, but you can change the default build to run if you encounter specific problem(s) with the pre-selected one.' #13#13 'For example, while the SDL1 version is the default version to run, the SDL2 version may be preferred over the SDL1 version for certain features such as improved keyboard and touchscreen support. Also, MinGW builds may work better with certain features (such as the Slirp backend for the NE2000 networking in standard MinGW builds) than Visual Studio builds even though they do not come with the debugger.' #13#13 'If you are not sure about which build to use, then you can just leave it unmodified and use the pre-selected one as the default build.', mbConfirmation, MB_OK);
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
procedure SetLanguage();
var
i: Integer;
line, linetmp, section: String;
FileLines: TStringList;
begin
      if FileExists(ExpandConstant('{app}\dosbox-x.conf')) and (PageLang.Values[1] or PageLang.Values[2] or PageLang.Values[3] or PageLang.Values[4] or PageLang.Values[5] or PageLang.Values[6] or PageLang.Values[7] or PageLang.Values[8] or PageLang.Values[9]) then
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
            if (CompareText(linetmp, 'language') = 0) and (CompareText(section, 'dosbox') = 0) then
            begin
              linetmp := Trim(Copy(line, 1, Pos('=', line)));
              if (PageLang.Values[1]) and FileExists(ExpandConstant('{app}\languages\fr_FR.lng')) then
                FileLines[i] := linetmp+' fr_FR';
              if (PageLang.Values[2]) and FileExists(ExpandConstant('{app}\languages\de_DE.lng')) then
                FileLines[i] := linetmp+' de_DE';
              if (PageLang.Values[3]) and FileExists(ExpandConstant('{app}\languages\ja_JP.lng')) then
                FileLines[i] := linetmp+' ja_JP';
              if (PageLang.Values[4]) and FileExists(ExpandConstant('{app}\languages\ko_KR.lng')) then
                FileLines[i] := linetmp+' ko_KR';
              if (PageLang.Values[5]) and FileExists(ExpandConstant('{app}\languages\pt_BR.lng')) then
                FileLines[i] := linetmp+' pt_BR';
              if (PageLang.Values[6]) and FileExists(ExpandConstant('{app}\languages\zh_CN.lng')) then
                FileLines[i] := linetmp+' zh_CN';
              if (PageLang.Values[7]) and FileExists(ExpandConstant('{app}\languages\es_ES.lng')) then
                FileLines[i] := linetmp+' es_ES';
              if (PageLang.Values[8]) and FileExists(ExpandConstant('{app}\languages\zh_TW.lng')) then
                FileLines[i] := linetmp+' zh_TW';
              if (PageLang.Values[9]) and FileExists(ExpandConstant('{app}\languages\tr_TR.lng')) then
                FileLines[i] := linetmp+' tr_TR';
            end;
            if (CompareText(linetmp, 'keyboardlayout') = 0) and (CompareText(section, 'dos') = 0) then
            begin
              linetmp := Trim(Copy(line, 1, Pos('=', line)));
              if (PageLang.Values[1]) then
                FileLines[i] := linetmp+' fr';
              if (PageLang.Values[2]) then
                FileLines[i] := linetmp+' de';
              if (PageLang.Values[3]) then
                FileLines[i] := linetmp+' jp';
              if (PageLang.Values[4]) then
                FileLines[i] := linetmp+' ko';
              if (PageLang.Values[5]) then
                FileLines[i] := linetmp+' br';
              if (PageLang.Values[6]) then
                FileLines[i] := linetmp+' cn';
              if (PageLang.Values[7]) then
                FileLines[i] := linetmp+' sp';
              if (PageLang.Values[8]) then
                FileLines[i] := linetmp+' tw';
              if (PageLang.Values[9]) then
                FileLines[i] := linetmp+' tr';
            end;
            if (CompareText(linetmp, 'country') = 0) and (CompareText(section, 'config') = 0) then
            begin
              linetmp := Trim(Copy(line, 1, Pos('=', line)));
              if (PageLang.Values[1]) then
                FileLines[i] := linetmp+' 33,859';
              if (PageLang.Values[2]) then
                FileLines[i] := linetmp+' 49,850';
              if (PageLang.Values[3]) then
                FileLines[i] := linetmp+' 81,932';
              if (PageLang.Values[4]) then
                FileLines[i] := linetmp+' 82,949';
              if (PageLang.Values[5]) then
                FileLines[i] := linetmp+' 55,860';
              if (PageLang.Values[6]) then
                FileLines[i] := linetmp+' 86,936';
              if (PageLang.Values[7]) then
                FileLines[i] := linetmp+' 34,858';
              if (PageLang.Values[8]) then
                FileLines[i] := linetmp+' 886,950';
              if (PageLang.Values[9]) then
                FileLines[i] := linetmp+' 90,857';
              break;
            end
          end
        end;
        FileLines.SaveToFile(ExpandConstant('{app}\dosbox-x.conf'));
      end;
end;
procedure InitializeWizard();
begin
    msg:='The selected build will be the default build when you run DOSBox-X from the Windows Start Menu or the desktop. DOSBox-X provides both SDL1 and SDL2 builds, and while SDL1 builds are preselected, you may prefer SDL2 builds if for example you encounter some issues with a non-U.S. keyboard layout in SDL1 builds. Please click the "Help" button below for more information about the DOSBox-X build selection.';
    PageBuild:=CreateInputOptionPage(wpSelectDir, 'DOSBox-X XP build (32/64-bit)', 'Select the default DOSBox-X build to run', msg, True, False);
      PageBuild.AddEx('Visual Studio XP build 64-bit', 0, True);
      PageBuild.AddEx('Visual Studio XP build 32-bit', 0, True);
      PageBuild.AddEx('MinGW low-end build (32-bit only)', 0, True);
    if not IsWin64 then
      begin
        PageBuild.CheckListBox.ItemEnabled[0] := False;
        PageBuild.Values[1] := True;
      end
    else
        PageBuild.Values[0] := True;
    PageBuild.CheckListBox.AddGroup('Check the following box to select the SDL2 build as the default DOSBox-X build.', '', 0, nil);
    PageBuild.CheckListBox.AddCheckBox('Default to SDL2 build (instead of SDL1 build) ', '', 0, False, True, False, True, nil);
    CreateHelpButton(ScaleX(20), WizardForm.CancelButton.Top, WizardForm.CancelButton.Width, WizardForm.CancelButton.Height);
    msg:='DOSBox-X supports different video output systems for different purposes.' #13#13 'By default it uses the Direct3D output, but you may want to select the OpenGL pixel-perfect scaling output for improved image quality (not available if you had selected an ARM build). Also, if you use text-mode DOS applications and/or the DOS shell frequently you probably want to select the TrueType font (TTF) output to make the text screen look much better by using scalable TrueType fonts.' #13#13 'This setting can be later modified in the DOSBox-X''s configuration file (dosbox-x.conf), or from DOSBox-X''s Video menu.';
    PageOutput:=CreateInputOptionPage(PageBuild.ID, 'Video output for DOSBox-X', 'Specify the DOSBox-X video output system', msg, True, False);
    PageOutput.Add('Default output (Direct3D)');
    PageOutput.Add('OpenGL with pixel-perfect scaling');
    PageOutput.Add('TrueType font (TTF) / Direct3D output');
    PageOutput.Values[2] := True;
    msg:='DOSBox-X supports language files to display messages in different languages. The language for the user interface and internal DOS is English by default, but you can select a different language for its user interface and internal DOS. The language and code page settings can be later modified in the configuration file (dosbox-x.conf).';
    PageLang:=CreateInputOptionPage(PageOutput.ID, 'User interface and DOS language', 'Select the language for DOSBox-X''s user interface and internal DOS', msg, True, False);
    PageLang.Add('Default (English)');
    PageLang.Add('French (Français)');
    PageLang.Add('German (Deutsch)');
    PageLang.Add('Japanese (日本語)');
    PageLang.Add('Korean (한국어)');
    PageLang.Add('Portuguese (português do Brasil)');
    PageLang.Add('Simplified Chinese (简体中文)');
    PageLang.Add('Spanish (Español)');
    PageLang.Add('Traditional Chinese (繁體/正體中文)');
    PageLang.Add('Turkish (Türkçe)');
    PageLang.Values[0] := True;
    lang := ExpandConstant('{language}');
    if lang = 'fr' then
        PageLang.Values[1] := True;
    if lang = 'ge' then
        PageLang.Values[2] := True;
    if lang = 'ja' then
        PageLang.Values[3] := True;
    if lang = 'ko' then
        PageLang.Values[4] := True;
    if lang = 'pt' then
        PageLang.Values[5] := True;
    if lang = 'sc' then
        PageLang.Values[6] := True;
    if lang = 'sp' then
        PageLang.Values[7] := True;
    if lang = 'tc' then
        PageLang.Values[8] := True;
    if lang = 'tr' then
        PageLang.Values[9] := True;
    msg:='DOSBox-X allows you to select a TrueType font (or OpenType font) for the TrueType font (TTF) output. It has a builtin TTF font as the default font for the output, but you may want to select a different TTF (or TTC/OTF) font than the default one.' #13#13 'This setting can be later modified in the DOSBox-X''s configuration file (dosbox-x.conf).';
    PageFont:=CreateInputOptionPage(PageLang.ID, 'TrueType font', 'Select the font for the TrueType font output', msg, True, False);
    PageFont.Add('Default TrueType font');
    PageFont.Add('Sarasa Gothic font');
    PageFont.Add('Consolas font');
    PageFont.Add('Courier New font');
    PageFont.Add('Nouveau IBM font');
    PageFont.Add('Custom TrueType font');
    if not FileExists(ExpandConstant('{fonts}\consola.ttf')) then
      PageFont.CheckListBox.ItemEnabled[2] := False;
    if not FileExists(ExpandConstant('{fonts}\cour.ttf')) then
      PageFont.CheckListBox.ItemEnabled[3] := False;
    PageFont.Values[0] := True;
    PageCustom := CreateInputFilePage(PageFont.ID, 'TrueType font', 'Select a custom TrueType font', 'Please select where your custom TrueType font (or OpenType font) is located, then click "Next" to continue.');
    PageCustom.Add('&Location of your custom TrueType font (TTF/TTC/OTF):', 'TrueType font files|*.ttf;*.ttc;*.otf|All files|*.*','.ttf;.ttc;.otf');
    msg:='You can specify a default DOS version for DOSBox-X to report to itself and DOS programs. This can sometimes change the feature sets of DOSBox-X. For example, selecting 7.10 as the reported DOS version will enable support for Windows-style long filenames (LFN) and FAT32 disk images (>2GB disk images) by default.' #13#13 'If you are not sure about which DOS version to report, you can also leave this unselected, then a preset DOS version will be reported (usually 5.00).' #13#13 'This setting can be later modified in the DOSBox-X''s configuration file (dosbox-x.conf).';
    PageVer:=CreateInputOptionPage(PageCustom.ID, 'Reported DOS version', 'Specify the default DOS version to report', msg, True, False);
    PageVer.Add('DOS version 3.30');
    PageVer.Add('DOS version 5.00 (reported by default)');
    PageVer.Add('DOS version 6.22');
    PageVer.Add('DOS version 7.10 (for LFN and FAT32 support)');
end;
function ShouldSkipPage(PageID: Integer): Boolean;
var
  i: Integer;
  defcp: Boolean;
  line, linetmp, section: String;
  FileLines: TStringList;
begin
  if (PageID = PageLang.ID) then
  begin
    if FileExists(ExpandConstant('{app}\dosbox-x.conf')) then
    begin
      if (ExpandConstant('{language}') = 'en') or (WizardSetupType(False) = 'compact') then
      begin
        Result := True;
        exit;
      end;
      FileLines := TStringList.Create;
      FileLines.LoadFromFile(ExpandConstant('{app}\dosbox-x.conf'));
      section := '';
      defcp := False;
      for i := 0 to FileLines.Count - 1 do
      begin
        line := Trim(FileLines[i]);
        if (Length(line)>2) and (Copy(line, 1, 1) = '[') and (Copy(line, Length(line), 1) = ']') then
          section := Copy(line, 2, Length(line)-2);
        if (Length(line)>0) and (Copy(line, 1, 1) <> '#') and (Copy(line, 1, 1) <> '[') and (Pos('=', line) > 1) then
        begin
          linetmp := Trim(Copy(line, 1, Pos('=', line) - 1));
          if (CompareText(linetmp, 'country') = 0) and (CompareText(section, 'config') = 0) then
          begin
            linetmp := Trim(Copy(line, Pos('=', line) + 1, Length(line)));
            if (CompareText(linetmp, '') = 0) then
              defcp := True;
            break;
          end;
        end;
      end;
      Result := not defcp;
    end
    else
      Result := False;
  end
  else
    Result := ((PageID = PageOutput.ID) or (PageID = PageFont.ID) or (PageID = PageCustom.ID) or (PageID = PageVer.ID)) and FileExists(ExpandConstant('{app}\dosbox-x.conf')) or ((PageID = PageFont.ID) and (not PageOutput.Values[2] or PageLang.Values[3] or PageLang.Values[4] or PageLang.Values[6] or PageLang.Values[8])) or ((PageID = PageCustom.ID) and (not PageOutput.Values[2] or not PageFont.Values[5]));
end;
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = PageCustom.ID) and (Length(PageCustom.Edits[0].Text) = 0) then
  begin
    MsgBox('Please select a custom TrueType font (or OpenType font) file.', mbError, MB_OK);
    WizardForm.ActiveControl := PageCustom.Edits[0];
    Result := False;
  end;
end;
procedure CurPageChanged(CurPageID: Integer);
begin
  HelpButton.Visible := CurPageID = PageBuild.ID;
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
    Wizardform.ReadyMemo.Lines.Add('Default DOSBox-X build:');
    if (PageBuild.Values[0]) and (not PageBuild.Values[4]) then
      msg:='Visual Studio XP 64bit SDL1';
    if (PageBuild.Values[0]) and (PageBuild.Values[4]) then
      msg:='Visual Studio XP 64bit SDL2';
    if (PageBuild.Values[1]) and (not PageBuild.Values[4]) then
      msg:='Visual Studio XP 32bit SDL1';
    if (PageBuild.Values[1]) and (PageBuild.Values[4]) then
      msg:='Visual Studio XP 32bit SDL2';
    if (PageBuild.Values[2]) and (not PageBuild.Values[4]) then
      msg:='MinGW low-end build SDL1';
    if (PageBuild.Values[2]) and (PageBuild.Values[4]) then
      msg:='MinGW low-end build SDL2';
    Wizardform.ReadyMemo.Lines.Add('      '+msg);
    lang:='';
    if PageLang.Values[0] or PageLang.Values[1] or PageLang.Values[2] or PageLang.Values[3] or PageLang.Values[4] or PageLang.Values[5] or PageLang.Values[6] or PageLang.Values[7] or PageLang.Values[8] or PageLang.Values[9] then
    begin
      lang:='Default (English)';
      if (PageLang.Values[1]) then
        lang:='French (Français)';
      if (PageLang.Values[2]) then
        lang:='German (Deutsch)';
      if (PageLang.Values[3]) then
        lang:='Japanese (日本語)';
      if (PageLang.Values[4]) then
        lang:='Korean (한국어)';
      if (PageLang.Values[5]) then
        lang:='Portuguese (português do Brasil)';
      if (PageLang.Values[6]) then
        lang:='Simplified Chinese (简体中文)';
      if (PageLang.Values[7]) then
        lang:='Spanish (Español)';
      if (PageLang.Values[8]) then
        lang:='Traditional Chinese (繁體/正體中文)';
      if (PageLang.Values[9]) then
        lang:='Turkish (Türkçe)';
    end;
    if not FileExists(ExpandConstant('{app}\dosbox-x.conf')) then
    begin
      if PageOutput.Values[0] or PageOutput.Values[1] or PageOutput.Values[2] then
      begin
        Wizardform.ReadyMemo.Lines.Add('');
        Wizardform.ReadyMemo.Lines.Add('Video output for DOSBox-X:');
        msg:='Default output (Direct3D)';
        if (PageOutput.Values[1]) then
          msg:='OpenGL with pixel-perfect scaling';
        if (PageOutput.Values[2]) then
          msg:='TrueType font (TTF) / Direct3D output';
        Wizardform.ReadyMemo.Lines.Add('      '+msg);
      end;
      if Length(lang) > 0 then
      begin
        Wizardform.ReadyMemo.Lines.Add('');
        Wizardform.ReadyMemo.Lines.Add('User interface and DOS language:');
        Wizardform.ReadyMemo.Lines.Add('      '+lang);
      end;
      if PageFont.Values[0] or PageFont.Values[1] or PageFont.Values[2] or PageFont.Values[3] or PageFont.Values[4] or PageFont.Values[5] then
      begin
        Wizardform.ReadyMemo.Lines.Add('');
        Wizardform.ReadyMemo.Lines.Add('TrueType font:');
        msg:='Default TrueType font';
        if (PageFont.Values[1]) then
          msg:='SarasaGothicFixed font';
        if (PageFont.Values[2]) then
          msg:='Consolas font';
        if (PageFont.Values[3]) then
          msg:='Courier New font';
        if (PageFont.Values[4]) then
          msg:='Nouveau IBM font';
        if (PageFont.Values[5]) then
          msg:='Custom TrueType font: ' + PageCustom.Values[0];
        Wizardform.ReadyMemo.Lines.Add('      '+msg);
      end;
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
    end
    else if Length(lang) > 0 then
    begin
      Wizardform.ReadyMemo.Lines.Add('');
      Wizardform.ReadyMemo.Lines.Add('User interface and DOS language:');
      Wizardform.ReadyMemo.Lines.Add('      '+lang);
    end
  end
end;
procedure CurStepChanged(CurrentStep: TSetupStep);
var
  i, j, k, adv, res: Integer;
  tsection, vsection, addcp, dosvcn, dosvtw, dosvset, found1, found2, found3, found4, found5: Boolean;
  refname, section, line, linetmp, lineold, linenew, SetupType: String;
  FileLines, FileLinesold, FileLinesnew, FileLinesave: TStringList;
begin
  if (CurrentStep = ssPostInstall) then
  begin
    refname:='{app}\dosbox-x.reference.full.conf';
    if FileExists(ExpandConstant('{app}\dosbox-x.conf')) then
    begin
      refname:='{app}\dosbox-x.reference.setup.conf';
    end
    else if IsTaskSelected('commonoption') then
    begin
       refname:='{app}\dosbox-x.reference.conf';
    end;
    if not FileExists(ExpandConstant(refname)) then
    begin
      MsgBox('Cannot find the ' + Copy(refname, 7, 33) + ' file.', mbError, MB_OK);
      DeleteFile(ExpandConstant('{app}\dosbox-x.reference.setup.conf'));
      Exit;
    end;
    refname:='{app}\dosbox-x.reference.full.conf';
    if IsTaskSelected('commonoption') then
      refname:='{app}\dosbox-x.reference.conf';
    if FileExists(ExpandConstant(refname)) then
    begin
      FileLines := TStringList.Create;
      FileLines.LoadFromFile(ExpandConstant(refname));
      section := '';
      found1 := False;
      found2 := False;
      for i := 0 to FileLines.Count - 1 do
      begin
        line := Trim(FileLines[i]);
        if (Length(line)>2) and (Copy(line, 1, 1) = '[') and (Copy(line, Length(line), 1) = ']') then
          section := Copy(line, 2, Length(line)-2);
        if (Length(line)>0) and (Copy(line, 1, 1) <> '#') and (Copy(line, 1, 1) <> '[') and (Pos('=', line) > 1) then
        begin
          linetmp := Trim(Copy(line, 1, Pos('=', line) - 1));
          if (CompareText(linetmp, 'printoutput') = 0) and (CompareText(section, 'printer') = 0) then
          begin
            linetmp := Trim(Copy(line, 1, Pos('=', line)));
            FileLines[i] := linetmp+' printer';
            found1 := True;
          end;
          if (CompareText(linetmp, 'file access tries') = 0) and (CompareText(section, 'dos') = 0) then
          begin
            linetmp := Trim(Copy(line, 1, Pos('=', line)));
            FileLines[i] := linetmp+' 3';
            found2 := True;
          end;
          if IsTaskSelected('centerwindow') and (CompareText(linetmp, 'windowposition') = 0) and (CompareText(section, 'sdl') = 0) then
          begin
            linetmp := Trim(Copy(line, 1, Pos('=', line)));
            FileLines[i] := linetmp+' ';
            found3 := True;
          end;
          if not IsTaskSelected('drivedelay') and (CompareText(linetmp, 'hard drive data rate limit') = 0) and (CompareText(section, 'dos') = 0) then
          begin
            linetmp := Trim(Copy(line, 1, Pos('=', line)));
            FileLines[i] := linetmp+' 0';
            found4 := True;
          end;
          if not IsTaskSelected('drivedelay') and (CompareText(linetmp, 'floppy drive data rate limit') = 0) and (CompareText(section, 'dos') = 0) then
          begin
            linetmp := Trim(Copy(line, 1, Pos('=', line)));
            FileLines[i] := linetmp+' 0';
            found5 := True;
          end;
          if found1 and found2 and (found3 or not IsTaskSelected('centerwindow')) and ((found4 and found5) or IsTaskSelected('drivedelay')) then
            break;
        end
      end;
      FileLines.SaveToFile(ExpandConstant(refname));
    end;
    if not FileExists(ExpandConstant('{app}\dosbox-x.conf')) then
    begin
      FileCopy(ExpandConstant(refname), ExpandConstant('{app}\dosbox-x.conf'), false);
      if FileExists(ExpandConstant('{app}\dosbox-x.conf')) and (PageOutput.Values[1] or PageOutput.Values[2]) then
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
            if (CompareText(linetmp, 'output') = 0) and (CompareText(section, 'sdl') = 0) then
            begin
              linetmp := Trim(Copy(line, 1, Pos('=', line)));
              if (PageOutput.Values[1]) then
                FileLines[i] := linetmp+' openglpp';
              if (PageOutput.Values[2]) then
                FileLines[i] := linetmp+' ttf';
              break;
            end
          end
        end;
        FileLines.SaveToFile(ExpandConstant('{app}\dosbox-x.conf'));
      end;
      SetupType := WizardSetupType(False);
      if SetupType = 'compact' then
      begin
        if (not PageLang.Values[1]) and FileExists(ExpandConstant('{app}\languages\fr_FR.lng')) then
          DeleteFile(ExpandConstant('{app}\languages\fr_FR.lng'));
        if (not PageLang.Values[2]) and FileExists(ExpandConstant('{app}\languages\de_DE.lng')) then
          DeleteFile(ExpandConstant('{app}\languages\de_DE.lng'));
        if (not PageLang.Values[3]) and FileExists(ExpandConstant('{app}\languages\ja_JP.lng')) then
          DeleteFile(ExpandConstant('{app}\languages\ja_JP.lng'));
        if (not PageLang.Values[4]) and FileExists(ExpandConstant('{app}\languages\ko_KR.lng')) then
          DeleteFile(ExpandConstant('{app}\languages\ko_KR.lng'));
        if (not PageLang.Values[5]) and FileExists(ExpandConstant('{app}\languages\pt_BR.lng')) then
          DeleteFile(ExpandConstant('{app}\languages\pt_BR.lng'));
        if (not PageLang.Values[6]) and FileExists(ExpandConstant('{app}\languages\zh_CN.lng')) then
          DeleteFile(ExpandConstant('{app}\languages\zh_CN.lng'));
        if (not PageLang.Values[7]) and FileExists(ExpandConstant('{app}\languages\es_ES.lng')) then
          DeleteFile(ExpandConstant('{app}\languages\es_ES.lng'));
        if (not PageLang.Values[8]) and FileExists(ExpandConstant('{app}\languages\zh_TW.lng')) then
          DeleteFile(ExpandConstant('{app}\languages\zh_TW.lng'));
        if (not PageLang.Values[9]) and FileExists(ExpandConstant('{app}\languages\tr_TR.lng')) then
          DeleteFile(ExpandConstant('{app}\languages\tr_TR.lng'));
        if (not PageLang.Values[3]) and (not PageLang.Values[4]) and (not PageLang.Values[6]) and (not PageLang.Values[8]) and (not PageFont.Values[1]) then
        begin
          if FileExists(ExpandConstant('{app}\SarasaGothicFixed.ttf')) then
            DeleteFile(ExpandConstant('{app}\SarasaGothicFixed.ttf'));
        end;
        if (not PageFont.Values[4]) then
        begin
          if FileExists(ExpandConstant('{app}\Nouveau_IBM.ttf')) then
            DeleteFile(ExpandConstant('{app}\Nouveau_IBM.ttf'));
        end;
      end;
      SetLanguage();
      if FileExists(ExpandConstant('{app}\dosbox-x.conf')) and (PageFont.Values[1] or PageFont.Values[2] or PageFont.Values[3] or PageFont.Values[4] or PageFont.Values[5]) then
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
            if (CompareText(linetmp, 'font') = 0) and (CompareText(section, 'ttf') = 0) then
            begin
              linetmp := Trim(Copy(line, 1, Pos('=', line)));
              if (PageFont.Values[1]) then
                FileLines[i] := linetmp+' SarasaGothicFixed';
              if (PageFont.Values[2]) then
                FileLines[i] := linetmp+' Consola';
              if (PageFont.Values[3]) then
                FileLines[i] := linetmp+' Cour';
              if (PageFont.Values[4]) then
                FileLines[i] := linetmp+' Nouveau_IBM';
              if (PageFont.Values[5]) then
                FileLines[i] := linetmp+' '+PageCustom.Values[0];
              break;
            end
          end
        end;
        FileLines.SaveToFile(ExpandConstant('{app}\dosbox-x.conf'));
      end;
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
              linetmp := Trim(Copy(line, 1, Pos('=', line)));
              if (PageVer.Values[0]) then
                FileLines[i] := linetmp+' 3.3';
              if (PageVer.Values[1]) then
                FileLines[i] := linetmp+' 5.0';
              if (PageVer.Values[2]) then
                FileLines[i] := linetmp+' 6.22';
              if (PageVer.Values[3]) then
                FileLines[i] := linetmp+' 7.1';
              break;
            end
          end
        end;
        FileLines.SaveToFile(ExpandConstant('{app}\dosbox-x.conf'));
      end
    end
    else if (CompareStr(GetSHA1OfFile(ExpandConstant('{app}\dosbox-x.conf')), GetSHA1OfFile(ExpandConstant(refname))) <> 0) or (CompareStr(GetMD5OfFile(ExpandConstant('{app}\dosbox-x.conf')), GetMD5OfFile(ExpandConstant(refname))) <> 0) then
    begin
      msg:='The configuration file dosbox-x.conf already exists in the destination. Do you want to keep your current settings?' #13#13 'If you choose "Yes", your current settings will be kept and the file dosbox-x.conf will be automatically upgraded to the latest version format (recommended).' #13#13 'If you choose "No", the dosbox-x.conf file will be reset to the new default configuration, and your old dosbox-x.conf file will be named dosbox-x.conf.old in the installation directory.' #13#13 'If you choose "Cancel", your current dosbox-x.conf file will be kept as is without any modifications.' #13 #13 'In any case, the new default configuration file will be named dosbox-x.reference.conf in the installation directory.';
      if IsVerySilent() then
      begin
        res := IDYES;
      end
      else
      begin
        res := MsgBox(msg, mbConfirmation, MB_YESNOCANCEL);
      end;
      if (res = IDNO) then
      begin
        FileCopy(ExpandConstant('{app}\dosbox-x.conf'), ExpandConstant('{app}\dosbox-x.conf.old'), false);
        FileCopy(ExpandConstant(refname), ExpandConstant('{app}\dosbox-x.conf'), false);
      end
      else if (res = IDYES) then
      begin
        FileCopy(ExpandConstant('{app}\dosbox-x.conf'), ExpandConstant('{app}\dosbox-x.conf.old'), false);
        FileLines := TStringList.Create;
        FileLinesold := TStringList.Create;
        FileLinesold.LoadFromFile(ExpandConstant('{app}\dosbox-x.conf.old'));
        FileLinesnew := TStringList.Create;
        FileLinesnew.LoadFromFile(ExpandConstant('{app}\dosbox-x.reference.setup.conf'));
        FileLinesave := TStringList.Create;
        section := '';
        tsection := False;
        vsection := False;
        dosvcn := False;
        dosvtw := False;
        dosvset := False;
        addcp := True;
        if not FileExists(ExpandConstant('{app}\SarasaGothicFixed.ttf')) then
          dosvset:= True;
        for j := 0 to FileLinesold.Count - 1 do
        begin
          lineold := Trim(FileLinesold[j]);
          if (Length(lineold)>2) and (Copy(lineold, 1, 1) = '[') and (Copy(lineold, Length(lineold), 1) = ']') then
          begin
            section := Copy(lineold, 2, Length(lineold)-2);
            if (CompareText(section, 'ttf') = 0) then
            begin
              tsection := True;
              if (vsection and dosvset) then
                break;
            end
            else if(CompareText(section, 'video') = 0) then
            begin
              vsection := True;
              if (tsection and dosvset) then
                break;
            end
          end
          else if not dosvset and (CompareText(section, 'dosv') = 0) and (Length(lineold)>0) and (Copy(lineold, 1, 1) <> '#') then
          begin
            linetmp := Copy(lineold, 1, Pos('=', lineold) - 1);
            if (CompareText(Trim(linetmp), 'dosv') = 0) then
            begin
              dosvset := True;
              linetmp := Copy(lineold, Pos('=', lineold) + 1, Length(lineold));
              msg:='Your existing DOSBox-X is configured for the Chinese DOS/V mode. Do you want to change it to the Chinese TrueType font (TTF) mode for a general DOS emulation environment?';
              if ((CompareText(Trim(linetmp), 'cn') = 0) or (CompareText(Trim(linetmp), 'chs') = 0)) and (MsgBox(msg, mbConfirmation, MB_YESNO) = IDYES) then
              begin
                dosvcn := True;
                addcp := False;
              end
              else if ((CompareText(Trim(linetmp), 'tw') = 0) or (CompareText(Trim(linetmp), 'cht') = 0)) and (MsgBox(msg, mbConfirmation, MB_YESNO) = IDYES) then
              begin
                dosvtw := True;
                addcp := False;
              end;
              if (tsection and vsection) then
                break;
            end
          end
        end;
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
              if (Length(lineold)>2) and (Copy(lineold, 1, 1) = '[') and (Copy(lineold, Length(lineold), 1) = ']') and ((((CompareText(section, 'ttf') <> 0) or (tsection)) and (section = Copy(lineold, 2, Length(lineold)-2))) or ((not tsection) and (CompareText(section, 'ttf') = 0) and (CompareText(Copy(lineold, 2, Length(lineold)-2), 'render') = 0)) or (((CompareText(section, 'video') <> 0) or (vsection)) and (section = Copy(lineold, 2, Length(lineold)-2))) or ((not vsection) and (CompareText(section, 'video') = 0) and (CompareText(Copy(lineold, 2, Length(lineold)-2), 'dosbox') = 0))) then
              begin
                FileLines := TStringList.Create;
                for k := j+1 to FileLinesold.Count - 1 do
                begin
                  lineold := Trim(FileLinesold[k]);
                  if (Copy(lineold, 1, 4) = 'ttf.') and (CompareText(section, 'ttf') = 0) then
                    lineold := Copy(lineold, 5, Length(lineold)-4);
                  if (Length(lineold)>2) and (Copy(lineold, 1, 1) = '[') and (Copy(lineold, Length(lineold), 1) = ']') then
                  begin
                    break;
                  end;
                  if (CompareText(section, '4dos') = 0) or (CompareText(section, 'config') = 0) or (CompareText(section, 'autoexec') = 0) then
                  begin
                    if (Length(lineold)>0) or (FileLines.Count>0) then
                    begin
                      linetmp := Copy(FileLinesold[k], 1, Pos('=', FileLinesold[k]) - 1);
                      if (CompareText(section, 'config') = 0) and (CompareText(Trim(linetmp), 'country') = 0) and dosvcn then
                      begin
                        addcp := True;
                        FileLinesold[k] := linetmp + '= 86,936';
                      end
                      else if (CompareText(section, 'config') = 0) and (CompareText(Trim(linetmp), 'country') = 0) and dosvtw then
                      begin
                        addcp := True;
                        FileLinesold[k] := linetmp + '= 886,950';
                      end;
                      FileLinesave.add(FileLinesold[k]);
                    end;
                    if (Length(lineold)>0) then
                    begin
                      linetmp := Copy(lineold, 1, Pos('=', lineold) - 1);
                      if (CompareText(section, 'config') = 0) and (CompareText(Trim(linetmp), 'country') = 0) and dosvcn then
                      begin
                        lineold := linetmp + '= 86,936';
                      end
                      else if (CompareText(section, 'config') = 0) and (CompareText(Trim(linetmp), 'country') = 0) and dosvtw then
                      begin
                        lineold := linetmp + '= 886,950';
                      end;
                      FileLines.add(lineold);
                    end
                  end
                  else if (Length(lineold)>0) and (Copy(lineold, 1, 1) <> '#') then
                    FileLines.add(lineold);
                end;
                break;
              end
            end
          end
          else if (CompareText(section, '4dos') = 0) or (CompareText(section, 'config') = 0) or (CompareText(section, 'autoexec') = 0) then
          begin
            linetmp := Copy(FileLinesnew[i], 1, Pos('=', FileLinesnew[i]) - 1);
            if (FileLines.Count=0) or (not addcp and (CompareText(section, 'config') = 0) and (CompareText(Trim(linetmp), 'country') = 0)) then
            begin
              if (CompareText(section, 'config') = 0) and (CompareText(Trim(linetmp), 'country') = 0) and dosvcn then
              begin
                addcp := True;
                FileLinesnew[i] := linetmp + '= 86,936';
              end
              else if (CompareText(section, 'config') = 0) and (CompareText(Trim(linetmp), 'country') = 0) and dosvtw then
              begin
                addcp := True;
                FileLinesnew[i] := linetmp + '= 886,950';
              end;
              FileLinesave.add(FileLinesnew[i]);
            end;
            continue;
          end
          else if (Length(linenew)=0) or ((Copy(linenew, 1, 1) = '#') and (Copy(linenew, 1, 14) <> '#DOSBOX-X-ADV:') and (Copy(linenew, 1, 18) <> '#DOSBOX-X-ADV-SEE:')) then
          begin
            FileLinesave.add(FileLinesnew[i]);
            continue;
          end
          else if not IsTaskSelected('commonoption') and (Copy(linenew, 1, 15) = '#DOSBOX-X-ADV:#') then
          begin
            Delete(linenew, 1, 14);
            FileLinesave.add(linenew);
            continue;
          end
          else if IsTaskSelected('commonoption') and (Copy(linenew, 1, 19) = '#DOSBOX-X-ADV-SEE:#') then
          begin
            Delete(linenew, 1, 18);
            FileLinesave.add(linenew);
            continue;
          end
          else if (Copy(linenew, 1, 15) = '#DOSBOX-X-ADV:#') or (Copy(linenew, 1, 19) = '#DOSBOX-X-ADV-SEE:#') then
          begin
            continue;
          end
          else if (Length(section)>0) and (Length(linenew)>0) and (Pos('=', linenew) > 1) then
          begin
            adv := 0;
            if (Copy(linenew, 1, 14) = '#DOSBOX-X-ADV:') then
            begin
              Delete(linenew, 1, 14);
              adv := 1;
            end;
            res := 0;
            linetmp := Copy(linenew, 1, Pos('=', linenew) - 1);
            for j := 0 to FileLines.Count - 1 do
            begin
              lineold := Trim(FileLines[j]);
              if (Length(lineold)>0) and (Pos('=', lineold) > 1) and (CompareText(Trim(linetmp), Trim(Copy(lineold, 1, Pos('=', lineold) - 1))) = 0) then
              begin
                res := 1;
                if (CompareText(section, 'dos') = 0) and (CompareText(Trim(linetmp), 'file access tries') = 0) and (Trim(Copy(lineold, Pos('=', lineold) + 1, Length(lineold))) = '0') then
                begin
                  FileLinesave.add(linetmp + '= 3');
                  continue;
                end;
                if (CompareText(section, 'dos') = 0) and ((CompareText(Trim(linetmp), 'hard drive data rate limit') = 0) or (CompareText(Trim(linetmp), 'floppy drive data rate limit') = 0)) then
                begin
                  if IsTaskSelected('drivedelay') and (Trim(Copy(lineold, Pos('=', lineold) + 1, Length(lineold))) = '0') then
                  begin
                    FileLinesave.add(linetmp + '= -1');
                    continue;
                  end;
                  if not IsTaskSelected('drivedelay') and (Trim(Copy(lineold, Pos('=', lineold) + 1, Length(lineold))) = '-1') then
                  begin
                    FileLinesave.add(linetmp + '= 0');
                    continue;
                  end;
                end;
                if (CompareText(section, 'sdl') = 0) and (CompareText(Trim(linetmp), 'windowposition') = 0) then
                begin
                  if IsTaskSelected('centerwindow') and (Trim(Copy(lineold, Pos('=', lineold) + 1, Length(lineold))) = '-') then
                  begin
                    FileLinesave.add(linetmp + '= ');
                    continue;
                  end;
                  if not IsTaskSelected('centerwindow') and ((Trim(Copy(lineold, Pos('=', lineold) + 1, Length(lineold))) = '') or (Trim(Copy(lineold, Pos('=', lineold) + 1, Length(lineold))) = ',')) then
                  begin
                    FileLinesave.add(linetmp + '= -');
                    continue;
                  end;
                end;
                if (CompareText(section, 'sdl') = 0) and (CompareText(Trim(linetmp), 'output') = 0) and (dosvcn or dosvtw) then
                begin
                  FileLinesave.add(linetmp + '= ttf');
                  continue;
                end;
                if (CompareText(section, 'dosbox') = 0) and (CompareText(Trim(linetmp), 'language') = 0) and (SetupType <> 'compact') and dosvcn then
                begin
                  FileLinesave.add(linetmp + '= zh_CN');
                  continue;
                end;
                if (CompareText(section, 'dosbox') = 0) and (CompareText(Trim(linetmp), 'language') = 0) and (SetupType <> 'compact') and dosvtw then
                begin
                  FileLinesave.add(linetmp + '= zh_TW');
                  continue;
                end;
                if (CompareText(section, 'dosv') = 0) and (CompareText(Trim(linetmp), 'dosv') = 0) and ((CompareText(Trim(Copy(lineold, Pos('=', lineold) + 1, Length(lineold))), 'cn') = 0) and dosvcn) or ((CompareText(Trim(Copy(lineold, Pos('=', lineold) + 1, Length(lineold))), 'tw') = 0) and dosvtw) then
                begin
                  FileLinesave.add(linetmp + '= off');
                  continue;
                end;
                if not ((adv = 1) and IsTaskSelected('commonoption') and ((Trim(Copy(lineold, Pos('=', lineold) + 1, Length(lineold))) = Trim(Copy(linenew, Pos('=', linenew) + 1, Length(linenew)))) or ((CompareText(Trim(linetmp), 'drive z hide files') = 0) and (Trim(Copy(lineold, Pos('=', lineold) + 1, Length(lineold))) = '/A20GATE.COM /DSXMENU.EXE /HEXMEM16.EXE /HEXMEM32.EXE /LOADROM.COM /NMITEST.COM /VESAMOED.COM /VFRCRATE.COM')))) then
                  FileLinesave.add(linetmp + '= ' + Trim(Copy(lineold, Pos('=', lineold) + 1, Length(lineold))));
                FileLines.Delete(j);
                break;
              end
            end;
            if (res = 0) and ((adv = 0) or not IsTaskSelected('commonoption')) then
            begin
              linetmp := Copy(linenew, 1, Pos('=', linenew) - 1);
              if (CompareText(Trim(linetmp), 'output') = 0) and (dosvcn or dosvtw) then
                linenew := linetmp + '= ttf';
              if (CompareText(Trim(linetmp), 'dosv') = 0) and (dosvcn or dosvtw) then
                linenew := linetmp + '= off';
              if (CompareText(Trim(linetmp), 'language') = 0) and (SetupType <> 'compact') and dosvcn then
                linenew := linetmp + '= zh_CN';
              if (CompareText(Trim(linetmp), 'language') = 0) and (SetupType <> 'compact') and dosvtw then
                linenew := linetmp + '= zh_TW';
              if (CompareText(Trim(linetmp), 'country') = 0) and dosvcn then
                linenew := linetmp + '= 86,936';
              if (CompareText(Trim(linetmp), 'country') = 0) and dosvtw then
                linenew := linetmp + '= 886,950';
              if (CompareText(Trim(linetmp), 'file access tries') = 0) then
                linenew := Copy(Trim(linenew), 1, Length(Trim(linenew)) - 1) + '3';
              if IsTaskSelected('centerwindow') and (CompareText(Trim(linetmp), 'windowposition') = 0) then
                linenew := Copy(Trim(linenew), 1, Length(Trim(linenew)) - 2) + '';
              if not IsTaskSelected('drivedelay') and ((CompareText(Trim(linetmp), 'hard drive data rate limit') = 0) or (CompareText(Trim(linetmp), 'floppy drive data rate limit') = 0)) then
                linenew := Copy(Trim(linenew), 1, Length(Trim(linenew)) - 2) + '0';
              FileLinesave.add(linenew);
            end
          end
        end;
        FileLinesave.SaveToFile(ExpandConstant('{app}\dosbox-x.conf'));
        FileLinesold.free;
        FileLinesnew.free;
        DeleteFile(ExpandConstant('{app}\dosbox-x.conf.old'));
      end;
      SetLanguage();
    end;
    DeleteFile(ExpandConstant('{app}\dosbox-x.reference.setup.conf'));
  end
end;
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  case CurUninstallStep of
    usUninstall:
    begin
      if FileExists(ExpandConstant('{app}\dosbox-x.conf.old')) then
        DeleteFile(ExpandConstant('{app}\dosbox-x.conf.old'));
      msg:='Do you want to keep the configuration file dosbox-x.conf?';
      if FileExists(ExpandConstant('{app}\dosbox-x.conf')) and (IsVerySilent() or (MsgBox(msg, mbConfirmation, MB_YESNO) = IDNO)) then
        DeleteFile(ExpandConstant('{app}\dosbox-x.conf'));
    end
  end;
end;
function CheckDirName(name: String): Boolean;
var
  dir: string;
begin
    dir:='Win32_builds\';
    if (PageBuild.Values[0]) and (not PageBuild.Values[5]) then
      dir:=dir+'x86_Release';
    if (PageBuild.Values[0]) and (PageBuild.Values[5]) then
      dir:=dir+'x86_Release_SDL2';
    if (PageBuild.Values[1]) and (not PageBuild.Values[5]) then
      dir:=dir+'ARM_Release';
    if (PageBuild.Values[1]) and (PageBuild.Values[5]) then
      dir:=dir+'ARM_Release_SDL2';
    if (PageBuild.Values[2]) and (not PageBuild.Values[5]) then
      dir:=dir+'mingw';
    if (PageBuild.Values[2]) and (PageBuild.Values[5]) then
      dir:=dir+'mingw-sdl2';
    if Is32BitInstaller() and (PageBuild.Values[3]) and (not PageBuild.Values[5]) then
      dir:=dir+'mingw-lowend';
    if Is32BitInstaller() and (PageBuild.Values[3]) and (PageBuild.Values[5]) then
      dir:=dir+'mingw-lowend-sdl2';
    Result := False;
    if (dir=name) then
      Result := True;
end;
function GetDesktopFolder(Param: String): String;
begin
  if (IsAdminLoggedOn or IsPowerUserLoggedOn) then
    Result := ExpandConstant('{commondesktop}')
  else
    Result := ExpandConstant('{userdesktop}');
end;
function InstallVersion(val: integer): Boolean;
begin
    Result := False;
    if (PageBuild.Values[0]) and (not PageBuild.Values[4]) and (val = 0) then
      Result := True;
    if (PageBuild.Values[0]) and (PageBuild.Values[4]) and (val = 1) then
      Result := True;
    if (PageBuild.Values[1]) and (not PageBuild.Values[4]) and (val = 2) then
      Result := True;
    if (PageBuild.Values[1]) and (PageBuild.Values[4]) and (val = 3) then
      Result := True;
    if (PageBuild.Values[2]) and (not PageBuild.Values[4]) and (val = 4) then
      Result := True;
    if (PageBuild.Values[2]) and (PageBuild.Values[4]) and (val = 5) then
      Result := True;
end;
