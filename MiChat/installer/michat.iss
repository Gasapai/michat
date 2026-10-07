; Instalador único de Mi Chat (Inno Setup 6)
; Los chats se guardan en %APPDATA%\MiChat\MiChat\chat.db, así que desinstalar/actualizar NO borra tus mensajes.

[Setup]
AppId={{6F1B7E64-3C0A-4B58-9D2E-5A7C1D9E4B11}
AppName=Mi Chat
AppVersion=1.0.0
AppPublisher=Mi Chat
DefaultDirName={autopf}\MiChat
DefaultGroupName=Mi Chat
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=MiChat-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
UninstallDisplayName=Mi Chat
UninstallDisplayIcon={app}\MiChat.exe

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "Crear un icono en el escritorio"; GroupDescription: "Accesos directos:"

[Files]
Source: "..\deploy\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{autoprograms}\Mi Chat"; Filename: "{app}\MiChat.exe"
Name: "{autodesktop}\Mi Chat"; Filename: "{app}\MiChat.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\MiChat.exe"; Description: "Abrir Mi Chat"; Flags: nowait postinstall skipifsilent
