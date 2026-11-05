; Script Inno Setup para Nord Pianos (Standalone + VST3)
#define MyAppName "Nord Pianos"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Personal Audio Research"
#define MyAppExeName "Nord Pianos.exe"
#define BuildDir "C:\Projetos\Nord_Piano_Kit\Nord_Piano_Reader\build-win\NordPianoPrototype_artefacts\Release"

[Setup]
AppId={{9F824A1B-4C67-46BE-9D72-B1E5C8D405AA}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=C:\Projetos\Nord_Piano_Kit\Nord_Piano_Reader\dist
OutputBaseFilename=Nord_Pianos_Setup_v1.0
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
WizardStyle=modern
PrivilegesRequired=admin
SetupIconFile=C:\Projetos\Nord_Piano_Kit\Nord_Piano_Reader\installer\app_icon.ico

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "Instalação Completa (Aplicativo Standalone + Plugin VST3)"
Name: "compact"; Description: "Apenas Plugin VST3"
Name: "custom"; Description: "Personalizada"; Flags: iscustom

[Components]
Name: "standalone"; Description: "Aplicativo Standalone (.exe autônomo - roda sem DAW)"; Types: full; Flags: checkablealone
Name: "vst3"; Description: "Plugin VST3 (para Ableton Live, FL Studio, Reaper, Cubase, etc.)"; Types: full compact custom; Flags: checkablealone

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Components: standalone

[InstallDelete]
; Remove versoes antigas em cache para forcar Ableton a carregar a nova versao
Type: files; Name: "{commoncf64}\VST3\NPno Piano Reader.vst3\Contents\x86_64-win\NPno Piano Reader.vst3"
Type: files; Name: "{localappdata}\Programs\Common\VST3\NPno Piano Reader.vst3\Contents\x86_64-win\NPno Piano Reader.vst3"

[Files]
; Icone do aplicativo instalado
Source: "C:\Projetos\Nord_Piano_Kit\Nord_Piano_Reader\installer\app_icon.ico"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone

; Executavel Standalone
Source: "{#BuildDir}\Standalone\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone

; Plugin VST3 oficial no Windows (C:\Program Files\Common Files\VST3\Nord Pianos.vst3\...)
Source: "{#BuildDir}\VST3\Nord Pianos.vst3\*"; DestDir: "{commoncf64}\VST3\Nord Pianos.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3

; Atualiza tambem NPno Piano Reader no Program Files para sessoes antigas do Ableton
Source: "{#BuildDir}\VST3\Nord Pianos.vst3\*"; DestDir: "{commoncf64}\VST3\NPno Piano Reader.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3

; Plugin VST3 para AppData Local
Source: "{#BuildDir}\VST3\Nord Pianos.vst3\*"; DestDir: "{localappdata}\Programs\Common\VST3\Nord Pianos.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
Source: "{#BuildDir}\VST3\Nord Pianos.vst3\*"; DestDir: "{localappdata}\Programs\Common\VST3\NPno Piano Reader.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3

; Pasta Customizada do Ableton Live do usuario (E:\Disco Elisson\PLUGINS)
Source: "{#BuildDir}\VST3\Nord Pianos.vst3\*"; DestDir: "E:\Disco Elisson\PLUGINS\Nord Pianos.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
Source: "{#BuildDir}\VST3\Nord Pianos.vst3\*"; DestDir: "E:\Disco Elisson\PLUGINS\NPno Piano Reader.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3

[Icons]
Name: "{autoprograms}\{#MyAppName}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\app_icon.ico"; Components: standalone
Name: "{autoprograms}\{#MyAppName}\Desinstalar {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\app_icon.ico"; Tasks: desktopicon; Components: standalone

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent; Components: standalone
