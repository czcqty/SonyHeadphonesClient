Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
Name "SonyHeadphonesClient"
OutFile "${OUTDIR}\SonyHeadphonesClient-${VERSION}-windows-x64-setup.exe"
InstallDir "$LOCALAPPDATA\Programs\SonyHeadphonesClient"
InstallDirRegKey HKCU "Software\SoundConnectWinUI" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
AllowSkipFiles off
VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "SonyHeadphonesClient"
VIAddVersionKey "FileDescription" "SonyHeadphonesClient Setup"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "SonyHeadphonesClient contributors"
!define MUI_ICON "${APPDIR}\app-icon.ico"
!define MUI_UNICON "${APPDIR}\app-icon.ico"
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "SimpChinese"

LangString SetupLaunchError ${LANG_ENGLISH} "SonyHeadphonesClient Setup could not start. Check the available disk space and run setup again."
LangString SetupLaunchError ${LANG_SIMPCHINESE} "无法启动 SonyHeadphonesClient 安装程序。请检查可用磁盘空间后重试。"

Function .onInit
  IfSilent worker
  SetSilent silent
  InitPluginsDir
  SetOutPath "$PLUGINSDIR\wizard"
  ; Launch the same self-contained WinUI runtime from temporary storage before installation.
  ; NSIS reuses identical File data when it builds the install section below.
  ClearErrors
  File /r /x backend "${APPDIR}\*"
  IfErrors launch_error
  ExecWait '"$OUTDIR\SonyHeadphonesClient.exe" --setup "$EXEPATH" "${VERSION}" "$INSTDIR"' $0
  IfErrors launch_error
  ${If} $0 != 0
  ${AndIf} $0 != 1
    Goto launch_error
  ${EndIf}
  ; Cancellation is returned to the caller without entering an installation section.
  SetErrorLevel $0
  Quit
launch_error:
  MessageBox MB_OK|MB_ICONSTOP "$(SetupLaunchError)"
  SetErrorLevel 2
  Quit

worker:
  ; Hold the mutex in the worker only. The interactive launcher waits for this process.
  System::Call 'kernel32::CreateMutexW(p 0, i 0, w "Local\SoundConnectWinUI.Install") p.r0 ?e'
  Pop $1
  ${If} $0 == 0
  ${OrIf} $1 == 183
    SetErrorLevel 4
    Quit
  ${EndIf}
FunctionEnd

Section "SonyHeadphonesClient" SEC_MAIN
  SectionIn RO
  SetShellVarContext current
  ReadRegStr $1 HKCU "Software\SoundConnectWinUI" "InstallDir"
  ${If} ${FileExists} "$INSTDIR\Uninstall.exe"
  ${AndIf} $1 != $INSTDIR
    SetErrorLevel 5
    Quit
  ${EndIf}
  ${If} $1 != ""
  ${AndIf} ${FileExists} "$1\Uninstall.exe"
    ; Check for running binaries before the old uninstaller removes owned files.
    !macro CheckUnlocked relative
      ${If} ${FileExists} "$1\${relative}"
        ClearErrors
        FileOpen $2 "$1\${relative}" a
        IfErrors upgrade_busy
        FileClose $2
      ${EndIf}
    !macroend
    !insertmacro CheckUnlocked "SoundConnect.WinUI.exe" ; previous releases
    !insertmacro CheckUnlocked "SonyHeadphonesClient.exe"
    !insertmacro CheckUnlocked "backend\SoundConnectBackend.exe"
    ClearErrors
    ExecWait '"$1\Uninstall.exe" /S _?=$1' $0
    IfErrors upgrade_busy
    ${If} $0 != 0
      Goto upgrade_busy
    ${EndIf}
    ; _?= runs in place. After a move the old uninstaller must also be removed.
    Delete "$1\Uninstall.exe"
    RMDir "$1"
  ${EndIf}
  ClearErrors
  SetOutPath "$INSTDIR"
  File /r "${APPDIR}\*"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateDirectory "$SMPROGRAMS\SonyHeadphonesClient"
  CreateShortcut "$SMPROGRAMS\SonyHeadphonesClient\SonyHeadphonesClient.lnk" "$INSTDIR\SonyHeadphonesClient.exe" "--live"
  CreateShortcut "$SMPROGRAMS\SonyHeadphonesClient\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\SoundConnectWinUI" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundConnectWinUI" "DisplayName" "SonyHeadphonesClient"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundConnectWinUI" "DisplayIcon" "$INSTDIR\SonyHeadphonesClient.exe,0"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundConnectWinUI" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundConnectWinUI" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundConnectWinUI" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundConnectWinUI" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundConnectWinUI" "NoRepair" 1
  IfErrors install_failed
  ${GetParameters} $0
  ClearErrors
  ${GetOptions} $0 "/DESKTOP" $1
  IfErrors install_done
  CreateShortcut "$DESKTOP\SonyHeadphonesClient.lnk" "$INSTDIR\SonyHeadphonesClient.exe" "--live"
  IfErrors install_failed
install_done:
  SetErrorLevel 0
  Goto done
upgrade_busy:
  SetErrorLevel 3
  Quit
install_failed:
  SetErrorLevel 2
  Quit
done:
SectionEnd
Section "Uninstall"
  SetShellVarContext current
  Delete "$DESKTOP\SonyHeadphonesClient.lnk"
  Delete "$SMPROGRAMS\SonyHeadphonesClient\SonyHeadphonesClient.lnk"
  Delete "$SMPROGRAMS\SonyHeadphonesClient\Uninstall.lnk"
  RMDir "$SMPROGRAMS\SonyHeadphonesClient"
  !include "${OUTDIR}\uninstall-files.nsh"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundConnectWinUI"
  DeleteRegKey HKCU "Software\SoundConnectWinUI"
SectionEnd
