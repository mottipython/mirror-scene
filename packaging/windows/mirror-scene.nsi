; The Mirror Scene screensaver installer.
; Installs for all users (asks for administrator rights) so that it appears in the
; Windows list of screensavers like any other.
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "x64.nsh"

!ifndef VERSION
  !define VERSION "1.0.2"
!endif

Name "The Mirror Scene ${VERSION}"
OutFile "..\..\dist\MirrorScene-Setup-${VERSION}.exe"
InstallDir "$PROGRAMFILES64\The Mirror Scene"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
Unicode true
!define MUI_ICON "mirror-scene.ico"
!define MUI_UNICON "mirror-scene.ico"

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Open Screen Saver Settings"
!define MUI_FINISHPAGE_RUN_FUNCTION OpenSettings

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

!define UNKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\MirrorScene"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "The Mirror Scene needs 64-bit Windows."
    Abort
  ${EndIf}
FunctionEnd

Function OpenSettings
  Exec 'rundll32.exe shell32.dll,Control_RunDLL desk.cpl,,1'
FunctionEnd

Section "The Mirror Scene" SecMain
  SectionIn RO
  SetRegView 64
  SetOutPath "$INSTDIR"
  File "..\..\build\win\mirror-scene.exe"
  File "..\..\build\win\*.dll"
  File /oname=README.txt "..\..\README.md"
  File /oname=LICENSE.txt "..\..\LICENSE"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\MirrorScene" "InstallDir" "$INSTDIR"

  ; the screensaver file itself goes in the real (64-bit) system folder
  ${DisableX64FSRedirection}
  SetOutPath "$WINDIR\System32"
  File "..\..\build\win\MirrorScene.scr"
  ${EnableX64FSRedirection}
  SetOutPath "$INSTDIR"

  ; make it the current screensaver for whoever is installing
  WriteRegStr HKCU "Control Panel\Desktop" "SCRNSAVE.EXE" "$WINDIR\System32\MirrorScene.scr"
  WriteRegStr HKCU "Control Panel\Desktop" "ScreenSaveActive" "1"
  ReadRegStr $1 HKCU "Control Panel\Desktop" "ScreenSaveTimeOut"
  StrCmp $1 "" 0 +2
    WriteRegStr HKCU "Control Panel\Desktop" "ScreenSaveTimeOut" "300"

  CreateDirectory "$SMPROGRAMS\The Mirror Scene"
  CreateShortcut "$SMPROGRAMS\The Mirror Scene\The Mirror Scene (window).lnk" "$INSTDIR\mirror-scene.exe" "--window"
  CreateShortcut "$SMPROGRAMS\The Mirror Scene\The Mirror Scene (full screen).lnk" "$INSTDIR\mirror-scene.exe" "/s"
  CreateShortcut "$SMPROGRAMS\The Mirror Scene\Uninstall.lnk" "$INSTDIR\Uninstall.exe"

  WriteRegStr HKLM "${UNKEY}" "DisplayName" "The Mirror Scene screensaver"
  WriteRegStr HKLM "${UNKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNKEY}" "DisplayIcon" "$INSTDIR\mirror-scene.exe"
  WriteRegStr HKLM "${UNKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "${UNKEY}" "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKLM "${UNKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNKEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $2 $3 $4
  IntFmt $2 "0x%08X" $2
  WriteRegDWORD HKLM "${UNKEY}" "EstimatedSize" "$2"
SectionEnd

Section "Uninstall"
  SetRegView 64
  ; stop being the screensaver, if we still are
  ReadRegStr $1 HKCU "Control Panel\Desktop" "SCRNSAVE.EXE"
  StrCmp $1 "$WINDIR\System32\MirrorScene.scr" 0 +2
    DeleteRegValue HKCU "Control Panel\Desktop" "SCRNSAVE.EXE"
  ${DisableX64FSRedirection}
  Delete "$WINDIR\System32\MirrorScene.scr"
  ${EnableX64FSRedirection}

  Delete "$INSTDIR\mirror-scene.exe"
  Delete "$INSTDIR\SDL2.dll"
  Delete "$INSTDIR\SDL3.dll"
  Delete "$INSTDIR\README.txt"
  Delete "$INSTDIR\LICENSE.txt"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\The Mirror Scene\The Mirror Scene (window).lnk"
  Delete "$SMPROGRAMS\The Mirror Scene\The Mirror Scene (full screen).lnk"
  Delete "$SMPROGRAMS\The Mirror Scene\Uninstall.lnk"
  RMDir "$SMPROGRAMS\The Mirror Scene"
  DeleteRegKey HKLM "${UNKEY}"
  DeleteRegKey HKLM "Software\MirrorScene"
SectionEnd
