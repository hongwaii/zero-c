; Inno Setup 脚本 for Modem Agent
; 编译: iscc.exe tools/installer/agent.iss /DMyAppVersion=1.1.0

[Setup]
AppName=Modem Agent
AppVersion={#MyAppVersion}
AppPublisher=huanghongwei
AppPublisherURL=https://example.com
DefaultDirName={autopf}\ModemAgent
DefaultGroupName=Modem Agent
OutputDir=..\dist
OutputBaseFilename=agent-setup-{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
UninstallDisplayIcon={app}\APP.exe
; 不签 v1.0
; SignTool=...
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; 复制 build 产物
Source: "..\out\APP\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; 复制默认配置（如存在）
; Source: "..\config\app.json.default"; DestDir: "{app}\config"; Flags: ignoreversion

[Icons]
Name: "{group}\Modem Agent"; Filename: "{app}\APP.exe"
Name: "{group}\{cm:UninstallProgram,Modem Agent}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\Modem Agent"; Filename: "{app}\APP.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts"

[Run]
Filename: "{app}\APP.exe"; Description: "Launch Modem Agent"; Flags: nowait postinstall skipifsilent
