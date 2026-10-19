Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"
Name "Sound Connect (WinUI)"
OutFile "${OUTDIR}\SoundConnect-WinUI-${VERSION}-windows-x64-setup.exe"
InstallDir "$LOCALAPPDATA\Programs\SoundConnectWinUI"
InstallDirRegKey HKCU "Software\SoundConnectWinUI" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "SimpChinese"
Section "Sound Connect" SEC_MAIN
  SectionIn RO
  SetShellVarContext current
  IfFileExists "$INSTDIR\Uninstall.exe" 0 upgrade_done
  ExecWait '"$INSTDIR\Uninstall.exe" /S _?=$INSTDIR' $0
  ${If} $0 != 0
    Abort "Close Sound Connect before upgrading."
  ${EndIf}
upgrade_done:
  SetOutPath "$INSTDIR"
  File /r "${APPDIR}\*"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateDirectory "$SMPROGRAMS\Sound Connect (WinUI)"
  CreateShortcut "$SMPROGRAMS\Sound Connect (WinUI)\Sound Connect.lnk" "$INSTDIR\SoundConnect.WinUI.exe" "--live"
  CreateShortcut "$SMPROGRAMS\Sound Connect (WinUI)\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\SoundConnectWinUI" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundConnectWinUI" "DisplayName" "Sound Connect (WinUI)"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundConnectWinUI" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundConnectWinUI" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundConnectWinUI" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundConnectWinUI" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundConnectWinUI" "NoRepair" 1
SectionEnd
Section /o "Desktop shortcut" SEC_DESKTOP
  CreateShortcut "$DESKTOP\Sound Connect (WinUI).lnk" "$INSTDIR\SoundConnect.WinUI.exe" "--live"
SectionEnd
Section "Uninstall"
  SetShellVarContext current
  Delete "$DESKTOP\Sound Connect (WinUI).lnk"
  Delete "$SMPROGRAMS\Sound Connect (WinUI)\Sound Connect.lnk"
  Delete "$SMPROGRAMS\Sound Connect (WinUI)\Uninstall.lnk"
  RMDir "$SMPROGRAMS\Sound Connect (WinUI)"
  !include "${OUTDIR}\uninstall-files.nsh"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundConnectWinUI"
  DeleteRegKey HKCU "Software\SoundConnectWinUI"
SectionEnd
