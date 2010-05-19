;NSIS Script For Crossfire GTK Client

;This script can run just like that, doesn't need anything special...

!include "MUI.nsh"

;Title Of Your Application
Name "Crossfire GTK2 Client"

VIAddVersionKey "ProductName" "Crossfire GTK2 client installer"
VIAddVersionKey "Comments" "Website: http://crossfire.real-time.com"
VIAddVersionKey "FileDescription" "Crossfire GTK client installer"
VIAddVersionKey "FileVersion" "1.50"
VIAddVersionKey "LegalCopyright" "Crossfire is released under the GPL."
VIProductVersion "1.50.0.0"

;Do A CRC Check
CRCCheck On

;Output File Name
OutFile "crossfire-client-windows.exe"


;The Default Installation Directory
InstallDir "$PROGRAMFILES\Crossfire GTK Client"
InstallDirRegKey HKCU "Software\Crossfire GTK Client" ""

!define MUI_ABORTWARNING

!define MUI_ICON "crossfire-client-gtk2.ico"
!define MUI_UNICON "crossfire-client-gtk2.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\COPYING"
!insertmacro MUI_PAGE_DIRECTORY
;;!insertmacro MUI_PAGE_STARTMENU page_id variable
!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_RUN $INSTDIR\crossfire-client-gtk2.exe
!define MUI_FINISHPAGE_SHOWREADME $INSTDIR\Running.txt

!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

; GetWindowsVersion
;
; Based on Yazno's function, http://yazno.tripod.com/powerpimpit/
; Updated by Joost Verburg
;
; Returns on top of stack
;
; Windows Version (95, 98, ME, NT x.x, 2000, XP, 2003)
; or
; '' (Unknown Windows Version)
;
; Usage:
;   Call GetWindowsVersion
;   Pop $R0
;   ; at this point $R0 is "NT 4.0" or whatnot

Function GetWindowsVersion

  Push $R0
  Push $R1

  ClearErrors

  ReadRegStr $R0 HKLM \
  "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion

  IfErrors 0 lbl_winnt
  
  ; we are not NT
  ReadRegStr $R0 HKLM \
  "SOFTWARE\Microsoft\Windows\CurrentVersion" VersionNumber

  StrCpy $R1 $R0 1
  StrCmp $R1 '4' 0 lbl_error

  StrCpy $R1 $R0 3

  StrCmp $R1 '4.0' lbl_win32_95
  StrCmp $R1 '4.9' lbl_win32_ME lbl_win32_98

  lbl_win32_95:
    StrCpy $R0 '95'
  Goto lbl_done

  lbl_win32_98:
    StrCpy $R0 '98'
  Goto lbl_done

  lbl_win32_ME:
    StrCpy $R0 'ME'
  Goto lbl_done

  lbl_winnt:

  StrCpy $R1 $R0 1

  StrCmp $R1 '3' lbl_winnt_x
  StrCmp $R1 '4' lbl_winnt_x

  StrCpy $R1 $R0 3

  StrCmp $R1 '5.0' lbl_winnt_2000
  StrCmp $R1 '5.1' lbl_winnt_XP
  StrCmp $R1 '5.2' lbl_winnt_2003 lbl_error

  lbl_winnt_x:
    StrCpy $R0 "NT $R0" 6
  Goto lbl_done

  lbl_winnt_2000:
    Strcpy $R0 '2000'
  Goto lbl_done

  lbl_winnt_XP:
    Strcpy $R0 'XP'
  Goto lbl_done

  lbl_winnt_2003:
    Strcpy $R0 '2003'
  Goto lbl_done

  lbl_error:
    Strcpy $R0 ''
  lbl_done:

  Pop $R1
  Exch $R0

FunctionEnd

Function CheckWindows
        ;Warn the user if under Windows 95 or Windows 98.
        Call GetWindowsVersion
        Pop $R0
        
        StrCmp $R0 "95" +2 +1
        StrCmp $R0 "98" +1 windows_ok
        MessageBox MB_YESNOCANCEL|MB_ICONEXCLAMATION "Warning!\rThe client cannot correctly work under Windows 95 or 98.\rContinue at your own risk!\rInstall anyway?" IDYES windows_ok
        
        ;User choosed to quit
        Quit
        
        windows_ok:
FunctionEnd

Function .onInit

        ;Check Windows version
        Call CheckWindows

FunctionEnd

Section "Install"
  ;Install Files
  SetOutPath $INSTDIR
  SetCompress Auto
  SetOverwrite IfNewer
  File "..\src\crossfire-client-gtk2.exe"
  File "..\src\libcurl.dll"
  File "..\src\pthreadGC2.dll"
  File "..\src\zlib1.dll"
  File "..\src\freetype6.dll"
  File "..\src\libexpat-1.dll"
  File "..\src\libglade-2.0-0.dll"
  File "..\src\libgtk-win32-2.0-0.dll"
  File "..\src\libpng12-0.dll"
  File "..\src\libpng14-14.dll"
  File "..\src\iconv.dll"
  File "..\src\libfontconfig-1.dll"
  File "..\src\libglib-2.0-0.dll"
  File "..\src\libpango-1.0-0.dll"
  File "..\src\SDL.dll"
  File "..\src\libatk-1.0-0.dll"
  File "..\src\libgdk_pixbuf-2.0-0.dll"
  File "..\src\libgmodule-2.0-0.dll"
  File "..\src\libpangocairo-1.0-0.dll"
  File "..\src\libcairo-2.dll"
  File "..\src\libgdk-win32-2.0-0.dll"
  File "..\src\libgobject-2.0-0.dll"
  File "..\src\libpangoft2-1.0-0.dll"
  File "..\src\libgio-2.0-0.dll"
  File "..\src\libgthread-2.0-0.dll"
  File "..\src\libpangowin32-1.0-0.dll"
  File "..\src\libxml2.dll"
  File "Win32Changes.txt"
  File /oname=ChangeLog.rtf "..\..\ChangeLog"
  File /oname=Copying.rtf "..\..\Copying"
  File "Running.txt"
  File "Building.txt"

  SetOutPath $INSTDIR\.crossfire
  File "gtk-v2.pos"
  File "gdefaults2"
  File "keys"
  File "msgs"

  SetOutPath $INSTDIR\crossfire-client\glade-gtk2
  File "..\glade\dialogs.glade"
  File "..\glade\dialogs.gladep"
  File "..\glade\chthonic.glade"
  File "..\glade\chthonic.gladep"
  File "..\glade\eureka.glade"
  File "..\glade\eureka.gladep"
  File "..\glade\gtk-v2.glade"
  File "..\glade\gtk-v2.gladep"
  File "..\glade\sixforty.glade"
  File "..\glade\sixforty.gladep"
  File "..\glade\v1-redux.glade"
  File "..\glade\v1-redux.gladep"
  File "..\glade\lobotomy.glade"
  File "..\glade\lobotomy.gladep"

  SetOutPath $INSTDIR\crossfire-client
  File "..\src\bmaps.client"
  File "..\src\crossfire.base"
  File "..\src\crossfire.clsc"

  ; Copy files to user's appdata directory
  CreateDirectory "$APPDATA\.crossfire"
  CopyFiles "$INSTDIR\.crossfire\*" "$APPDATA\.crossfire"
  
  ; Write AppPath key
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\App Paths\crossfire-client-gtk2.exe" "Path" $INSTDIR

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
  CreateShortCut "$SMPROGRAMS\Crossfire GTK Client\Crossfire GTK Client.lnk" "$INSTDIR\\crossfire-client-gtk2.exe" "" "$INSTDIR\\crossfire-client-gtk2.exe" 0
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
  Delete "$INSTDIR\crossfire-client\bmaps.client"
  Delete "$INSTDIR\crossfire-client\crossfire.base"
  Delete "$INSTDIR\crossfire-client\crossfire.clsc"
  Delete "$INSTDIR\crossfire-client-gtk2.exe"
  Delete "$INSTDIR\.CROSSFIRE\gtk-v2.pos"
  Delete "$INSTDIR\.CROSSFIRE\gdefaults2"
  Delete "$INSTDIR\.crossfire\keys"
  Delete "$INSTDIR\.crossfire\msgs"
  Delete "$INSTDIR\crossfire-client\glade-gtk2\dialogs.glade"
  Delete "$INSTDIR\crossfire-client\glade-gtk2\dialogs.gladep"
  Delete "$INSTDIR\crossfire-client\glade-gtk2\chthonic.glade"
  Delete "$INSTDIR\crossfire-client\glade-gtk2\chthonic.gladep"
  Delete "$INSTDIR\crossfire-client\glade-gtk2\eureka.glade"
  Delete "$INSTDIR\crossfire-client\glade-gtk2\eureka.gladep"
  Delete "$INSTDIR\crossfire-client\glade-gtk2\gtk-v2.glade"
  Delete "$INSTDIR\crossfire-client\glade-gtk2\gtk-v2.gladep"
  Delete "$INSTDIR\crossfire-client\glade-gtk2\sixforty.glade"
  Delete "$INSTDIR\crossfire-client\glade-gtk2\sixforty.gladep"
  Delete "$INSTDIR\crossfire-client\glade-gtk2\v1-redux.glade"
  Delete "$INSTDIR\crossfire-client\glade-gtk2\v1-redux.gladep"
  Delete "$INSTDIR\crossfire-client\glade-gtk2\lobotomy.glade"
  Delete "$INSTDIR\crossfire-client\glade-gtk2\lobotomy.gladep"

  Delete "$INSTDIR\Win32Changes.txt"
  Delete "$INSTDIR\ChangeLog.rtf"
  Delete "$INSTDIR\Copying.rtf"
  Delete "$INSTDIR\Running.txt"
  Delete "$INSTDIR\Building.txt"

  ; Delete the dlls that are needed
  Delete "$INSTDIR\libcurl.dll"
  Delete "$INSTDIR\pthreadGC2.dll"
  Delete "$INSTDIR\zlib1.dll"
  Delete "$INSTDIR\freetype6.dll"
  Delete "$INSTDIR\libexpat-1.dll"
  Delete "$INSTDIR\libglade-2.0-0.dll"
  Delete "$INSTDIR\libgtk-win32-2.0-0.dll"
  Delete "$INSTDIR\libpng12-0.dll"
  Delete "$INSTDIR\libpng14-14.dll"
  Delete "$INSTDIR\iconv.dll"
  Delete "$INSTDIR\libfontconfig-1.dll"
  Delete "$INSTDIR\libglib-2.0-0.dll"
  Delete "$INSTDIR\libpango-1.0-0.dll"
  Delete "$INSTDIR\SDL.dll"
  Delete "$INSTDIR\libatk-1.0-0.dll"
  Delete "$INSTDIR\libgdk_pixbuf-2.0-0.dll"
  Delete "$INSTDIR\libgmodule-2.0-0.dll"
  Delete "$INSTDIR\libpangocairo-1.0-0.dll"
  Delete "$INSTDIR\libcairo-2.dll"
  Delete "$INSTDIR\libgdk-win32-2.0-0.dll"
  Delete "$INSTDIR\libgobject-2.0-0.dll"
  Delete "$INSTDIR\libpangoft2-1.0-0.dll"
  Delete "$INSTDIR\libgio-2.0-0.dll"
  Delete "$INSTDIR\libgthread-2.0-0.dll"
  Delete "$INSTDIR\libpangowin32-1.0-0.dll"
  Delete "$INSTDIR\libxml2.dll"

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
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\App Paths\crossfire-client-gtk2.exe"
  RMDir "$INSTDIR"
SectionEnd

