; Inno Setup script for The Last Watch.
; Built by Scripts/build_installer.ps1, which packages the game first and passes
; the packaged folder and version on the command line (/DGameDir=... /DAppVersion=...).

#ifndef GameDir
  #define GameDir "..\..\Saved\Package\Windows"
#endif
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#define AppName "The Last Watch"
#define AppExe "TheLastWatch.exe"

[Setup]
AppId={{6F3C2A8E-9B41-4D7A-A5E2-1C7B0D4F9E31}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=omotechweb
AppPublisherURL=https://github.com/omotechweb/TheLastWatch
AppSupportURL=https://github.com/omotechweb/TheLastWatch/issues
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir=..\..\Saved\Installer
OutputBaseFilename=TheLastWatch-Setup-{#AppVersion}
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
WizardStyle=modern
WizardImageFile=wizard.bmp,wizard_250.bmp
WizardSmallImageFile=wizard_small.bmp,wizard_small_250.bmp
WizardImageStretch=yes
; the paks are already compressed, so a big solid dictionary only costs memory
Compression=lzma2/max
LZMAUseSeparateProcess=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequiredOverridesAllowed=dialog
ShowLanguageDialog=auto

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl"

[CustomMessages]
english.Prereqs=Installing Unreal Engine prerequisites (Visual C++)...
turkish.Prereqs=Unreal Engine gereksinimleri kuruluyor (Visual C++)...

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#GameDir}\*"; DestDir: "{app}"; Excludes: "*.pdb,Manifest_*.txt,vc_redist.arm64.exe"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\Engine\Extras\Redist\en-us\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "{cm:Prereqs}"; Check: NeedsVCRedist; Flags: waituntilterminated skipifdoesntexist
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent


[Code]
// The Visual C++ 2015-2022 runtime is usually already there; only run its installer when it isn't
function NeedsVCRedist: Boolean;
var
  Installed: Cardinal;
begin
  Result := not (RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Installed', Installed) and (Installed = 1));
end;
