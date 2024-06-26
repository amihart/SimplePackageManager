# Microsoft Developer Studio Project File - Name="sdcpp" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=sdcpp - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "sdcpp.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "sdcpp.mak" CFG="sdcpp - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "sdcpp - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE "sdcpp - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "sdcpp - Win32 Debug"

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
# ADD BASE CPP /nologo /MLd /W3 /Gm /GX /ZI /Od /I "." /I ".\libiberty" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "HAVE_CONFIG_H"/FR /FD /GZ /c
# ADD CPP /nologo /MLd /W3 /Gm /GX /ZI /Od /I "." /I ".\libiberty" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "HAVE_CONFIG_H" /FR /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:console /debug /machine:I386 /out:"..\..\bin_vc\sdcpp.exe" /pdbtype:sept
# ADD LINK32 /nologo /subsystem:console /debug /machine:I386 /out:"..\..\bin_vc\sdcpp.exe" /pdbtype:sept
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "sdcpp - Win32 Release"

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
# ADD BASE CPP /nologo /ML /W3 /GX /O2 /I "." /I ".\libiberty" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /FD /c
# ADD CPP /nologo /ML /W3 /GX /O2 /I "." /I ".\libiberty" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "HAVE_CONFIG_H" /D "_WIN32" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:console /machine:I386 /out:"..\..\bin_vc\sdcpp.exe" /pdbtype:sept
# ADD LINK32 /nologo /subsystem:console /machine:I386 /out:"..\..\bin_vc\sdcpp.exe" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "sdcpp - Win32 Release"
# Name "sdcpp - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\cppdefault.c
# End Source File
# Begin Source File

SOURCE=.\cpperror.c
# End Source File
# Begin Source File

SOURCE=.\cppexp.c
# End Source File
# Begin Source File

SOURCE=.\cppfiles.c
# End Source File
# Begin Source File

SOURCE=.\cpphash.c
# End Source File
# Begin Source File

SOURCE=.\cppinit.c
# End Source File
# Begin Source File

SOURCE=.\cpplex.c
# End Source File
# Begin Source File

SOURCE=.\cpplib.c
# End Source File
# Begin Source File

SOURCE=.\cppmacro.c
# End Source File
# Begin Source File

SOURCE=.\cppmain.c
# End Source File
# Begin Source File

SOURCE=.\hashtable.c
# End Source File
# Begin Source File

SOURCE=.\intl.c
# End Source File
# Begin Source File

SOURCE=.\libiberty\lbasename.c
# End Source File
# Begin Source File

SOURCE=.\mbchar.c
# End Source File
# Begin Source File

SOURCE=.\mkdeps.c
# End Source File
# Begin Source File

SOURCE=.\libiberty\obstack.c
# End Source File
# Begin Source File

SOURCE=.\prefix.c
# End Source File
# Begin Source File

SOURCE=".\libiberty\safe-ctype.c"
# End Source File
# Begin Source File

SOURCE=".\libiberty\splay-tree.c"
# End Source File
# Begin Source File

SOURCE=.\version.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\ansidecl.h
# End Source File
# Begin Source File

SOURCE=".\auto-host.h"
# End Source File
# Begin Source File

SOURCE=.\config.h
# End Source File
# Begin Source File

SOURCE=.\cppdefault.h
# End Source File
# Begin Source File

SOURCE=.\cpphash.h
# End Source File
# Begin Source File

SOURCE=.\cpplib.h
# End Source File
# Begin Source File

SOURCE=.\hashtable.h
# End Source File
# Begin Source File

SOURCE=.\hwint.h
# End Source File
# Begin Source File

SOURCE=.\intl.h
# End Source File
# Begin Source File

SOURCE=.\libiberty.h
# End Source File
# Begin Source File

SOURCE=.\mkdeps.h
# End Source File
# Begin Source File

SOURCE=.\libiberty\obstack.h
# End Source File
# Begin Source File

SOURCE=.\output.h
# End Source File
# Begin Source File

SOURCE=.\prefix.h
# End Source File
# Begin Source File

SOURCE=".\libiberty\safe-ctype.h"
# End Source File
# Begin Source File

SOURCE=.\sdcc.h
# End Source File
# Begin Source File

SOURCE=".\libiberty\splay-tree.h"
# End Source File
# Begin Source File

SOURCE=.\symcat.h
# End Source File
# Begin Source File

SOURCE=.\system.h
# End Source File
# Begin Source File

SOURCE=.\version.h
# End Source File
# End Group
# End Target
# End Project
