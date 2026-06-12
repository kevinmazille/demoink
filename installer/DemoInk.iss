; DemoInk — per-user installer (Inno Setup 6)
;
; Installs the SAME DemoInk.exe that also runs standalone (portable). The exe is
; self-contained (static CRT) so no VC++ redistributable is bundled. Per-user
; install (no admin / no UAC), consistent with the in-app autostart toggle which
; writes the identical HKCU Run value.
;
; Build:
;   1) winget install --id JRSoftware.InnoSetup -e        (once)
;   2) build.bat                                           (produces the bundled exe)
;   3) "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\DemoInk.iss
; Output: installer\Output\DemoInk-1.0.0-Setup.exe

#define MyAppName "DemoInk"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Kevin Mazille (fork of DemoHelper by Stefan Kueng)"
#define MyAppExeName "DemoInk.exe"

[Setup]
; AppId uniquely identifies this app for upgrades/uninstall. Keep it stable
; across versions. Generated fresh (not the vcxproj ProjectGuid).
AppId={{10AC6F33-6D2E-4AF6-94C5-0815601E3293}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\Programs\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
SetupIconFile=..\src\DemoHelper.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
WizardStyle=modern
; Gracefully close a running DemoInk before overwriting the exe.
CloseApplications=yes
CloseApplicationsFilter={#MyAppExeName}
OutputDir=Output
OutputBaseFilename={#MyAppName}-{#MyAppVersion}-Setup

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; Flags: unchecked
Name: "autostart"; Description: "Launch {#MyAppName} automatically when Windows starts"; Flags: unchecked

[Files]
; Static CRT -> no VC++ redist needed.
Source: "..\bin\Release\x64\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; Same HKCU Run value name/format as the in-app toggle, so the two never diverge.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; \
    ValueType: string; ValueName: "DemoInk"; \
    ValueData: """{app}\{#MyAppExeName}"""; \
    Flags: uninsdeletevalue; Tasks: autostart

[UninstallDelete]
; Remove the runtime-created INI so uninstall leaves the dir clean.
Type: files; Name: "{app}\DemoInk.ini"
Type: dirifempty; Name: "{app}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName} now"; \
    Flags: nowait postinstall skipifsilent
