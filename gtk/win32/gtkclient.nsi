;NSIS Script For Crossfire GTK Client

;This script can run just like that, doesn't need anything special...

!include "MUI.nsh"

;Title Of Your Application
Name "Crossfire GTK Client 1.7.0"

;Do A CRC Check
CRCCheck On

;Output File Name
OutFile "crossfire-client-windows.exe"

;The Default Installation Directory
InstallDir "$PROGRAMFILES\Crossfire GTK Client"
InstallDirRegKey HKCU "Software\Crossfire GTK Client" ""

!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\COPYING"
!insertmacro MUI_PAGE_DIRECTORY
;;!insertmacro MUI_PAGE_STARTMENU page_id variable
!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_RUN $INSTDIR\GTKClient.exe
!define MUI_FINISHPAGE_SHOWREADME $INSTDIR\Running.txt

!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

Var GTKPath

Function .onInit

         Banner::Show /NOUNLOAD "Checking for GTK 2"

         ;Check for GTK, and get registry key
         ReadRegStr $GTKPath HKLM Software\GTK\2.0 DllPath
         StrCmp $GTKPath "" 0 GTKOk
                ; GTK not found, abort (unless user is really sure)
                MessageBox MB_YESNOCANCEL|MB_ICONEXCLAMATION "The installer cannot find GTK!$\rCrossfire client requires GTK 2, which is available from http://www.dropline.net/gtk/.$\r$\rDo you want to go to the site now?$\r$\rPress 'Yes' to open Dropline's web site.$\rPress 'No' to exit the installer.$\rPress 'Cancel' to continue the installation (use at your own risk!)" IDNO abort IDCANCEL ignoregtk
                
                ExecShell open http://www.dropline.net/gtk

                abort:
                Quit
                
                ignoregtk:
                ;User chose to really install, just warn one more time :)
                MessageBox MB_OK|MB_ICONINFORMATION "You have chosen to install even if GTK was not detected.$\rNo registry key will be set for Crossfire, so you may need to set it manually.$\rIts location should be HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\App Paths\GTKClient.exe$\rand the default value should point to GTK's lib subdirectory."
                
         GTKOk:
         Banner::Destroy
         
FunctionEnd

Section "Install"
  ;Install Files
  SetOutPath $INSTDIR
  SetCompress Auto
  SetOverwrite IfNewer
  File "bmaps.client"
  File "crossfire.base"
  File "crossfire.clsc"
  File "Release\GTKClient.exe"
  File "Win32Changes.txt"
  File /oname=ChangeLog.rtf "..\..\Changes"
  File /oname=Copying.rtf "..\..\Copying"
  File ..\..\Documentation\Scripting.html
  File "Running.txt"
  File "Building.txt"
  SetOutPath $INSTDIR\.crossfire
  File ".crossfire\gwinpos"
  File ".crossfire\gdefaults"
  File ".crossfire\keys"
  
  ; Write AppPath key
  StrCmp $GTKPath "" +2
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\App Paths\GTKClient.exe" "Path" $GTKPath

  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Crossfire GTK Client" "DisplayName" "Crossfire GTK Client (remove only)"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Crossfire GTK Client" "UninstallString" "$INSTDIR\Uninst.exe"
WriteUninstaller "Uninst.exe"
SectionEnd

Section "Shortcuts"
  ;Add Shortcuts
  ;SetOutPath called so that shortcuts point to correct directory
  SetOutPath $INSTDIR
  CreateDirectory "$SMPROGRAMS\Crossfire GTK Client"
  CreateShortCut "$SMPROGRAMS\Crossfire GTK Client\Crossfire GTK Client.lnk" "$INSTDIR\\GTKClient.exe" "" "$INSTDIR\\GTKClient.exe" 0
  CreateShortCut "$SMPROGRAMS\Crossfire GTK Client\License.lnk" "$INSTDIR\\Copying.rtf"
  CreateShortcut "$SMPROGRAMS\Crossfire GTK Client\Changes.lnk" "$INSTDIR\\Win32Changes.txt"
  CreateShortCut "$SMPROGRAMS\Crossfire GTK Client\Full change log.lnk" "$INSTDIR\\ChangeLog.rtf"
  CreateShortCut "$SMPROGRAMS\Crossfire GTK Client\Running the client.lnk" "$INSTDIR\\Running.txt"
  CreateShortCut "$SMPROGRAMS\Crossfire GTK Client\Scripting Guide.lnk" "$INSTDIR\\Scripting.html"
  CreateShortCut "$SMPROGRAMS\Crossfire GTK Client\Uninstall Crossfire GTK Client.lnk" "$INSTDIR\\Uninst.exe" 0
SectionEnd

UninstallText "This will uninstall Crossfire GTK Client from your system"

Section Uninstall
  ;Delete Files
  Delete "$INSTDIR\bmaps.client"
  Delete "$INSTDIR\crossfire.base"
  Delete "$INSTDIR\crossfire.clsc"
  Delete "$INSTDIR\GTKClient.exe"
  Delete "$INSTDIR\.CROSSFIRE\gwinpos"
  Delete "$INSTDIR\.CROSSFIRE\gdefaults"
  Delete "$INSTDIR\.crossfire\keys"
  Delete "$INSTDIR\Scripting.html"
  Delete "$INSTDIR\Win32Changes.txt"
  Delete "$INSTDIR\ChangeLog.rtf"
  Delete "$INSTDIR\Copying.rtf"
  Delete "$INSTDIR\Running.txt"
  Delete "$INSTDIR\Building.txt"
  ;Delete directories, but only if empty
  RmDir "$INSTDIR\.crossfire"
  RmDir "$INSTDIR"

  ;Delete Start Menu Shortcuts
  Delete "$SMPROGRAMS\Crossfire GTK Client\*.*"
  RmDir "$SMPROGRAMS\Crossfire GTK Client"

  ;Delete Uninstaller And Unistall Registry Entries
  Delete "$INSTDIR\Uninst.exe"
  DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Crossfire GTK Client"
  DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Crossfire GTK Client"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\App Paths\GTKClient.exe"
  RMDir "$INSTDIR"
SectionEnd

