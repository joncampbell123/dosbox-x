# Microsoft Developer Studio Project File - Name="Glide2x" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=Glide2x - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Glide2x.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Glide2x.mak" CFG="Glide2x - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Glide2x - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "Glide2x - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/Glide2x", BAAAAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Glide2x - Win32 Release"

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
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /G6 /Gr /MD /W3 /vd0 /O2 /Ob2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "CPPDLL" /YX /FD /c
# SUBTRACT CPP /Fr
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x416 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib shell32.lib opengl32.lib glu32.lib /nologo /subsystem:windows /dll /pdb:none /machine:I386
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "Glide2x - Win32 Debug"

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
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /G6 /Gr /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "DEBUG" /D "_WINDOWS" /D "CPPDLL" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x416 /d "_DEBUG"
# ADD RSC /l 0x416 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib opengl32.lib glu32.lib ddraw.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "Glide2x - Win32 Release"
# Name "Glide2x - Win32 Debug"
# Begin Group "Header"

# PROP Default_Filter ".h"
# Begin Source File

SOURCE=.\amd3dx.h
# End Source File
# Begin Source File

SOURCE=.\glext.h
# End Source File
# Begin Source File

SOURCE=.\Glextensions.h
# End Source File
# Begin Source File

SOURCE=.\GlOgl.h
# End Source File
# Begin Source File

SOURCE=.\GLRender.h
# End Source File
# Begin Source File

SOURCE=.\PGTexture.h
# End Source File
# Begin Source File

SOURCE=.\PGUTexture.h
# End Source File
# Begin Source File

SOURCE=.\sdk2_3dfx.h
# End Source File
# Begin Source File

SOURCE=.\sdk2_glide.h
# End Source File
# Begin Source File

SOURCE=.\sdk2_glidesys.h
# End Source File
# Begin Source File

SOURCE=.\sdk2_glideutl.h
# End Source File
# Begin Source File

SOURCE=.\sdk2_sst1vid.h
# End Source File
# Begin Source File

SOURCE=.\TexDB.h
# End Source File
# End Group
# Begin Group "Source"

# PROP Default_Filter ".c .cpp"
# Begin Source File

SOURCE=.\GLExtensions.cpp
# End Source File
# Begin Source File

SOURCE=.\Glide.cpp
# End Source File
# Begin Source File

SOURCE=.\GLRender.cpp
# End Source File
# Begin Source File

SOURCE=.\GLutil.cpp
# End Source File
# Begin Source File

SOURCE=.\grgu3df.cpp
# End Source File
# Begin Source File

SOURCE=.\grguBuffer.cpp
# End Source File
# Begin Source File

SOURCE=.\grguColorAlpha.cpp
# End Source File
# Begin Source File

SOURCE=.\grguDepth.cpp
# End Source File
# Begin Source File

SOURCE=.\grguDraw.cpp
# End Source File
# Begin Source File

SOURCE=.\grguFog.cpp
# End Source File
# Begin Source File

SOURCE=.\grguLfb.cpp
# End Source File
# Begin Source File

SOURCE=.\grguMisc.cpp
# End Source File
# Begin Source File

SOURCE=.\grguTex.cpp
# End Source File
# Begin Source File

SOURCE=.\PGTexture.cpp
# End Source File
# Begin Source File

SOURCE=.\PGUTexture.cpp
# End Source File
# Begin Source File

SOURCE=.\TexDB.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=.\Glide2x.rc
# End Source File
# End Target
# End Project
