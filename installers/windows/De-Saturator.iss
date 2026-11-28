; The Windows installer, built with Inno Setup 6 from the repository root:
;   ISCC /DAppVersion=0.9.1 installers\windows\De-Saturator.iss
; Art comes from design/installers/windows. The VST3 goes to the standard VST3 folder; the
; folder in Program Files only holds the uninstaller, the icon and the licences.

#ifndef AppVersion
  #define AppVersion "0.9.1"
#endif
#define Root "..\.."
#define Design Root + "\design"
#define Wizard Design + "\installers\windows\wizard"
#define Artefacts Root + "\build\DeSaturator_artefacts\Release"

[Setup]
; Permanent: it's how Windows recognises an upgrade of the same product.
AppId={{F5CDFFE9-6272-4EC9-B441-2391B6A3AB74}
AppName=De-Saturator
AppVersion={#AppVersion}
AppVerName=De-Saturator {#AppVersion}
AppPublisher=Aria Ashrafi
AppPublisherURL=https://ariaashrafi.com
VersionInfoVersion={#AppVersion}
DefaultDirName={commonpf64}\Aria Ashrafi\De-Saturator
DisableDirPage=yes
DefaultGroupName=Aria Ashrafi
DisableProgramGroupPage=yes
DisableReadyPage=no
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir={#Root}\build\installer
OutputBaseFilename=De-Saturator-{#AppVersion}-Windows
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
LicenseFile={#Root}\LICENSE
SetupIconFile={#Design}\installers\windows\de-saturator.ico
UninstallDisplayIcon={app}\de-saturator.ico
UninstallDisplayName=De-Saturator
WizardImageFile={#Wizard}\WizardImage-164x314.bmp,{#Wizard}\WizardImage-192x386.bmp,{#Wizard}\WizardImage-246x459.bmp,{#Wizard}\WizardImage-273x556.bmp,{#Wizard}\WizardImage-328x604.bmp,{#Wizard}\WizardImage-355x700.bmp,{#Wizard}\WizardImage-410x797.bmp
WizardSmallImageFile={#Wizard}\WizardSmallImage-55x58.bmp,{#Wizard}\WizardSmallImage-64x68.bmp,{#Wizard}\WizardSmallImage-83x80.bmp,{#Wizard}\WizardSmallImage-92x97.bmp,{#Wizard}\WizardSmallImage-110x106.bmp,{#Wizard}\WizardSmallImage-119x123.bmp,{#Wizard}\WizardSmallImage-138x140.bmp

[Dirs]
; Marked as a system folder so that Explorer shows the bundle's icon.
Name: "{commoncf64}\VST3\De-Saturator.vst3"; Attribs: system

[Files]
Source: "{#Artefacts}\VST3\De-Saturator.vst3\*"; DestDir: "{commoncf64}\VST3\De-Saturator.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Design}\installers\windows\de-saturator.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Root}\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
; The embedded fonts are under the SIL Open Font License, which asks for its text to travel with them.
Source: "{#Design}\fonts\Anybody-OFL.txt"; DestDir: "{app}\licences"; Flags: ignoreversion
Source: "{#Design}\fonts\DepartureMono-OFL.txt"; DestDir: "{app}\licences"; Flags: ignoreversion
