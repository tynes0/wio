#ifndef AppVersion
  #define AppVersion "0.3.0"
#endif

#ifndef AppVersionInfo
  #define AppVersionInfo "0.3.0.0"
#endif

#ifndef PackageRoot
  #define PackageRoot "..\artifacts\packages-release\wio-0.5.0-windows-x64-release"
#endif

#ifndef OutputDir
  #define OutputDir "..\artifacts\packages-release"
#endif

#define AppName "Wio"
#define AppPublisher "Tynes"
#define AppURL "https://github.com/tynes0/wio"
#define AppExeName "wio.exe"

[Setup]
AppId={{B23B53C3-F4E9-4E63-8E5B-A1A5D069E5A6}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
DefaultDirName={localappdata}\Programs\Wio
DefaultGroupName=Wio
OutputDir={#OutputDir}
OutputBaseFilename=WioSetup-{#AppVersion}-windows-x64
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes
AllowNoIcons=yes
ChangesEnvironment=yes
UninstallDisplayIcon={app}\bin\{#AppExeName}
VersionInfoVersion={#AppVersionInfo}
VersionInfoCompany={#AppPublisher}
VersionInfoDescription=Wio language toolchain installer
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}
VersionInfoCopyright=Copyright (c) {#AppPublisher}
CloseApplications=no
RestartIfNeededByRun=no

[Tasks]
Name: "addtopath"; Description: "Add Wio to PATH for the current user"; GroupDescription: "Environment:"; Flags: checkedonce
Name: "desktopquickstart"; Description: "Create a desktop shortcut to QUICKSTART.md"; GroupDescription: "Shortcuts:"; Flags: unchecked

[InstallDelete]
Type: filesandordirs; Name: "{app}\bin"
Type: filesandordirs; Name: "{app}\cmake"
Type: filesandordirs; Name: "{app}\docs"
Type: filesandordirs; Name: "{app}\runtime"
Type: filesandordirs; Name: "{app}\scripts"
Type: filesandordirs; Name: "{app}\sdk"
Type: filesandordirs; Name: "{app}\std"
Type: files; Name: "{app}\Install-Wio.ps1"
Type: files; Name: "{app}\install-wio.sh"
Type: files; Name: "{app}\Uninstall-Wio.ps1"
Type: files; Name: "{app}\uninstall-wio.sh"
Type: files; Name: "{app}\LICENSE"
Type: files; Name: "{app}\QUICKSTART.md"
Type: files; Name: "{app}\README.md"
Type: files; Name: "{app}\WIO_PACKAGE_INFO.json"

[Files]
Source: "{#PackageRoot}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Wio Quickstart"; Filename: "{app}\QUICKSTART.md"
Name: "{group}\Wio README"; Filename: "{app}\README.md"
Name: "{group}\Uninstall Wio"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Wio Quickstart"; Filename: "{app}\QUICKSTART.md"; Tasks: desktopquickstart

[Run]
Filename: "{app}\QUICKSTART.md"; Description: "Open QUICKSTART.md"; Flags: postinstall shellexec skipifsilent unchecked

[Code]
function QuoteArg(Value: String): String;
begin
  Result := '"' + Value + '"';
end;

function GetWioExePath(): String;
begin
  Result := ExpandConstant('{app}\bin\{#AppExeName}');
end;

function GetEnvSetupParams(): String;
begin
  Result := 'env setup --wio-root ' + QuoteArg(ExpandConstant('{app}')) + ' --set-user --no-prompt';

  if WizardIsTaskSelected('addtopath') then
    Result := Result + ' --add-path';
end;

function GetDoctorParams(): String;
begin
  Result := 'env doctor --wio-root ' + QuoteArg(ExpandConstant('{app}')) + ' --backend-smoke';
end;

procedure ConfigureWioEnvironment();
var
  ResultCode: Integer;
  WioExe: String;
  Params: String;
  DoctorParams: String;
begin
  WioExe := GetWioExePath();
  Params := GetEnvSetupParams();
  DoctorParams := GetDoctorParams();

  if not FileExists(WioExe) then begin
    MsgBox('Wio was copied, but bin\wio.exe could not be found. Environment setup was skipped.', mbError, MB_OK);
    exit;
  end;

  Log('Configuring Wio environment: ' + WioExe + ' ' + Params);

  if not Exec(WioExe, Params, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then begin
    MsgBox('Wio was installed, but environment setup could not be started.'#13#10#13#10 +
           'You can run this manually:'#13#10 +
           QuoteArg(WioExe) + ' ' + Params,
           mbError, MB_OK);
    exit;
  end;

  if ResultCode <> 0 then begin
    MsgBox('Wio files were installed, but environment setup failed with exit code ' + IntToStr(ResultCode) + '.'#13#10#13#10 +
           'You can retry manually from PowerShell or CMD:'#13#10 +
           QuoteArg(WioExe) + ' ' + Params,
           mbError, MB_OK);
  end else begin
    Log('Wio environment setup completed successfully.');

    if not Exec(WioExe, DoctorParams, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then begin
      MsgBox('Wio installed, but backend smoke verification could not be started.'#13#10#13#10 +
             'You can run this manually:'#13#10 +
             QuoteArg(WioExe) + ' ' + DoctorParams,
             mbError, MB_OK);
      exit;
    end;

    if ResultCode <> 0 then begin
      MsgBox('Wio installed, but backend smoke verification failed with exit code ' + IntToStr(ResultCode) + '.'#13#10#13#10 +
             'Please run this manually from a terminal for details:'#13#10 +
             QuoteArg(WioExe) + ' ' + DoctorParams,
             mbError, MB_OK);
    end else begin
      Log('Wio backend smoke verification completed successfully.');
    end;
  end;
end;

procedure RemoveWioEnvironment();
var
  ResultCode: Integer;
  WioExe: String;
  Params: String;
begin
  WioExe := GetWioExePath();

  if not FileExists(WioExe) then begin
    Log('Skipping Wio environment cleanup because bin\wio.exe was not found.');
    exit;
  end;

  Params := 'env remove --wio-root ' + QuoteArg(ExpandConstant('{app}')) + ' --set-user --remove-path --no-prompt';
  Log('Removing Wio environment: ' + WioExe + ' ' + Params);

  if not Exec(WioExe, Params, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then begin
    MsgBox('Wio uninstaller could not start environment cleanup.'#13#10#13#10 +
           'You may need to remove Wio from PATH manually.',
           mbError, MB_OK);
    exit;
  end;

  if ResultCode <> 0 then begin
    MsgBox('Wio environment cleanup returned exit code ' + IntToStr(ResultCode) + '.'#13#10#13#10 +
           'Files will still be removed. If PATH keeps a Wio entry, remove it manually.',
           mbError, MB_OK);
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    ConfigureWioEnvironment();
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
    RemoveWioEnvironment();
end;
