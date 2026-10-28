; Compile only through Build-Installer.ps1 against a verified unified release.
[Setup]
AppId={{00D2BDB7-8213-44DF-BF8A-7A1E66DB9E4F}
AppName=Kura
AppVersion={#AppVersion}
AppPublisher=Kura Project Team
DefaultDirName={localappdata}\Programs\Kura
DefaultGroupName=Kura
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#InstallerOutput}
OutputBaseFilename={#InstallerName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\resources\icons\kura.ico
UninstallDisplayIcon={app}\kura.ico
CloseApplications=yes
RestartApplications=no
SetupLogging=yes
VersionInfoVersion={#AppVersion}.0

[Languages]
Name: "chinesesimp"; MessagesFile: "ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"

[Files]
Source: "{#Payload}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Kura"; Filename: "{app}\KuraCpp.exe"; WorkingDir: "{app}"; IconFilename: "{app}\kura.ico"
Name: "{group}\Kura Demo"; Filename: "{app}\KuraCpp.exe"; Parameters: "--demo"; WorkingDir: "{app}"; IconFilename: "{app}\kura.ico"
Name: "{autodesktop}\Kura"; Filename: "{app}\KuraCpp.exe"; WorkingDir: "{app}"; IconFilename: "{app}\kura.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\KuraCpp.exe"; Description: "{cm:LaunchProgram,Kura}"; Flags: nowait postinstall skipifsilent
