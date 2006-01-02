# Microsoft Developer Studio Project File - Name="GTKClient" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=GTKClient - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "GTKClient.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "GTKClient.mak" CFG="GTKClient - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "GTKClient - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "GTKClient - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "." /I "c:\program files\fichiers communs\gtk\2.6\include" /I "..\..\common" /I "C:\Program Files\Fichiers communs\GTK\2.6\include\gtk-2.0" /I "C:\Program Files\Fichiers communs\GTK\2.6\include\glib-2.0" /I "C:\Program Files\Fichiers communs\GTK\2.6\lib\glib-2.0\include" /I "C:\Program Files\Fichiers communs\GTK\2.6\include\pango-1.0" /I "C:\Program Files\Fichiers communs\GTK\2.6\lib\gtk-2.0\include" /I "C:\Program Files\Fichiers communs\GTK\2.6\include\atk-1.0" /I "..\.." /I "f:\projets\include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "GTK_ENABLE_BROKEN" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "NDEBUG"
# ADD RSC /l 0x40c /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib glib-2.0.lib gtk-win32-2.0.lib libpng.lib gdk-win32-2.0.lib pango-1.0.lib gobject-2.0.lib pangowin32-1.0.lib gmodule-2.0.lib winmm.lib /nologo /subsystem:console /machine:I386 /nodefaultlib:"libc.lib" /libpath:"C:\Program Files\Fichiers communs\GTK\2.6\lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "." /I "c:\program files\fichiers communs\gtk\2.6\include" /I "..\..\common" /I "C:\Program Files\Fichiers communs\GTK\2.6\include\gtk-2.0" /I "C:\Program Files\Fichiers communs\GTK\2.6\include\glib-2.0" /I "C:\Program Files\Fichiers communs\GTK\2.6\lib\glib-2.0\include" /I "C:\Program Files\Fichiers communs\GTK\2.6\include\pango-1.0" /I "C:\Program Files\Fichiers communs\GTK\2.6\lib\gtk-2.0\include" /I "C:\Program Files\Fichiers communs\GTK\2.6\include\atk-1.0" /I "..\.." /I "f:\projets\include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "GTK_ENABLE_BROKEN" /Fr /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "_DEBUG"
# ADD RSC /l 0x40c /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winmm.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib glib-2.0.lib gtk-win32-2.0.lib gdk-win32-2.0.lib pango-1.0.lib gobject-2.0.lib pangowin32-1.0.lib gmodule-2.0.lib libpng.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept /libpath:"C:\Program Files\Fichiers communs\GTK\2.6\lib"
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "GTKClient - Win32 Release"
# Name "GTKClient - Win32 Debug"
# Begin Group "GTK client"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\config.c

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\gtk"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\gtk"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gtkproto.h

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\gtk"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\gtk"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gx11.c

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\gtk"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\gtk"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gx11.h

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\gtk"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\gtk"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\help.c
# End Source File
# Begin Source File

SOURCE=..\image.c

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\gtk"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\gtk"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\inventory.c
# End Source File
# Begin Source File

SOURCE=..\keys.c

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\gtk"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\gtk"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\map.c

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\gtk"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\gtk"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\png.c

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\gtk"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\gtk"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\sdl.c

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\gtk"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\gtk"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\sound.c
# End Source File
# Begin Source File

SOURCE=..\text.c
# End Source File
# End Group
# Begin Group "Common files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\common\cconfig.h

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\common"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\common"

!ENDIF 

# End Source File
# Begin Source File

SOURCE="..\..\common\client-types.h"

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\common"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\common"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\common\client.c

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\common"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\common"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\common\client.h

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\common"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\common"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\common\commands.c

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\common"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\common"

!ENDIF 

# End Source File
# Begin Source File

SOURCE="..\..\common\def-keys.h"

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\common"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\common"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\common\external.h

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\common"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\common"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\common\image.c

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\common"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\common"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\common\init.c

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\common"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\common"

!ENDIF 

# End Source File
# Begin Source File

SOURCE="..\..\common\item-types.h"

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\common"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\common"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\common\item.c

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\common"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\common"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\common\item.h

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\common"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\common"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\common\mapdata.c
# End Source File
# Begin Source File

SOURCE=..\..\common\mapdata.h
# End Source File
# Begin Source File

SOURCE=..\..\common\metaserver.c

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\common"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\common"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\common\misc.c

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\common"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\common"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\common\newclient.h

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\common"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\common"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\common\newsocket.c

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\common"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\common"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\common\p_cmd.c
# End Source File
# Begin Source File

SOURCE=..\..\common\player.c

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\common"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\common"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\common\proto.h

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\common"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\common"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\common\script.c
# End Source File
# Begin Source File

SOURCE=..\..\common\script.h
# End Source File
# End Group
# Begin Group "Win32 specific files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\config.h

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\win32"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\win32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\GTKClient.ico
# End Source File
# Begin Source File

SOURCE=.\GTKClient.rc
# End Source File
# Begin Source File

SOURCE=.\porting.c

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\win32"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\win32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\soundsdef.h

!IF  "$(CFG)" == "GTKClient - Win32 Release"

# PROP Intermediate_Dir "Release\win32"

!ELSEIF  "$(CFG)" == "GTKClient - Win32 Debug"

# PROP Intermediate_Dir "Debug\win32"

!ENDIF 

# End Source File
# End Group
# Begin Source File

SOURCE=..\..\ChangeLog
# End Source File
# End Target
# End Project
