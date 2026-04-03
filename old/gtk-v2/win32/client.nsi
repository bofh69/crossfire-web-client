!include "MUI.nsh"

;If the user passed "/DGITVERSION=something" when invoking the script, use that. Else, use a placeholder.
!ifdef GITVERSION
!define REVISION "git-${GITVERSION}"
!else
!define REVISION "git-unknown"
!endif

;Title Of Your Application
Name "Crossfire GTKClient"

VIAddVersionKey "ProductName" "Crossfire client installer"
VIAddVersionKey "Comments" "Website: http://crossfire.real-time.com"
VIAddVersionKey "FileDescription" "Crossfire client installer"
VIAddVersionKey "FileVersion" "${REVISION}"
VIAddVersionKey "LegalCopyright" "Crossfire is released under the GPL."

;If the user passed "/DVERSION=something" when invoking the script, use that. Else, use a placeholder.
;Note that it must be numerals, in the format x.x.x.x
!ifdef VERSION
VIProductVersion ${VERSION}
!else
VIProductVersion 1.99.99.99
!endif

;Do A CRC Check
CRCCheck On
SetCompressor /SOLID lzma

;Output File Name
;If the user passed "/DOUTPUTDIR=something" when invoking the script, use that. Else, use the current working directory.
!ifdef OUTPUTDIR
OutFile "${OUTPUTDIR}\CrossfireClient-${REVISION}.exe"
!else
OutFile "CrossfireClient-${REVISION}.exe"
!endif

;The Default Installation Directory
InstallDir "$PROGRAMFILES\Crossfire Client"
InstallDirRegKey HKCU "Software\Crossfire Client" ""

!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH


!insertmacro MUI_LANGUAGE "English"

Section "Crossfire Client (required)" cf
  SectionIn RO
  ;Install Files
  SetOutPath $INSTDIR
  SetCompress Auto
  SetOverwrite IfNewer
  
  ;If the user passed "/DINPUTDIR=something" when invoking the script, use that. Else, find the files in ".\files\"
  !ifdef INPUTDIR
  File /r "${INPUTDIR}\*.*"
  !else 
  File /r "files\*.*"
  !endif
  
  ;If the user passed "/DSOURCELOCATION=something" when invoking the script, use that. Else, find the icon in  "..\..\pixmaps\client.ico"
  !ifdef SOURCELOCATION
  File ${SOURCELOCATION}\pixmaps\client.ico
  !else
  File ..\..\pixmaps\client.ico
  !endif
  
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Crossfire Client" "DisplayName" "Crossfire Client (remove only)"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Crossfire Client" "UninstallString" "$INSTDIR\Uninst.exe"
  WriteUninstaller "Uninst.exe"

SectionEnd

Section "Menu and Desktop Shortcuts" menus
  ;Add Shortcuts
  SetOutPath $INSTDIR
  CreateDirectory "$SMPROGRAMS\Crossfire Client"
  CreateShortCut "$SMPROGRAMS\Crossfire Client\Crossfire Client.lnk" "$INSTDIR\crossfire-client-gtk2.exe" "" "$INSTDIR\client.ico" 0
  CreateShortCut "$SMPROGRAMS\Crossfire Client\Uninstall.lnk" "$INSTDIR\uninst.exe" "" "$INSTDIR\uninst.exe" 0

  SetShellVarContext all
  CreateShortcut "$desktop\Crossfire Client.lnk" "$INSTDIR\crossfire-client-gtk2.exe" "" "$INSTDIR\client.ico" 0
SectionEnd

UninstallText "This will uninstall Crossfire Client from your system"

Section "un.Crossfire Client" un_cf
  SectionIn RO

  ;Delete Files
  RmDir /r $INSTDIR

  ;Delete Start Menu Shortcuts
  RmDir /r "$SMPROGRAMS\Crossfire Client"
  ;Delete Desktop Shortcut
  SetShellVarContext all
  Delete "$desktop\Crossfire Client.lnk"

  ;Delete Uninstaller And Unistall Registry Entries
  Delete "$INSTDIR\Uninst.exe"
  DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Crossfire Client"
  DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Crossfire Client"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${cf} "Crossfire Client (required)."
  !insertmacro MUI_DESCRIPTION_TEXT ${menus} "Create icons in Start Menu and on Desktop."
!insertmacro MUI_FUNCTION_DESCRIPTION_END
