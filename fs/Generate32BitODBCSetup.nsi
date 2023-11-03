#
# July 08, 2021
#
# The registry entry for this ODBC driver on a 64 bit machine will be at (because of registry redirection)
# Computer\HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\ODBC\ODBCINST.INI\DB/C FS6 ODBC Driver (x86)
# The above entry will point to c:\windows\system32\fs6odbc.dll even though it is actually in c:\windows\syswow64
# because file system redirection is also in effect.
#

Name "DB/C FS6 ODBC Driver (x86)"

# General Symbol Definitions
!define VERSION 101.0.0.0
!define COMPANY "Portable Software Co."
!define COPYRIGHT "(c) Portable Software Co."
!define URL http://www.portablesoftware.com
!define UNINST_FILE_NAME fs101odbcuninst.exe
!define DLL_FILE_NAME "fsodbc32.dll"
!define DBC_ICON "dbcbig.ico"

# MultiUser Symbol Definitions
!define MULTIUSER_EXECUTIONLEVEL Admin

# MUI Symbol Definitions
!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_UNFINISHPAGE_NOAUTOCLOSE
!define MUI_ICON ${DBC_ICON}

# Included files
!include MultiUser.nsh
!include Sections.nsh
!include MUI2.nsh
!include x64.nsh

# Installer pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

# Installer languages
!insertmacro MUI_LANGUAGE English

# Installer attributes
OutFile setupodb.exe
#
# On 32 bit systems, this will be C:\Windows\System32
# On 64 bit systems, file system redirection is in effect so this will actually be C:\Windows\SysWOW64
#
InstallDir $SYSDIR
CRCCheck on
XPStyle on
ShowInstDetails show
VIProductVersion 101.0.0.0
VIAddVersionKey ProductName "DB/C FS101 ODBC Driver (x86)"
VIAddVersionKey ProductVersion "${VERSION}"
VIAddVersionKey CompanyName "${COMPANY}"
VIAddVersionKey CompanyWebsite "${URL}"
VIAddVersionKey FileVersion "${VERSION}"
VIAddVersionKey FileDescription "DB/C FS101 ODBC Driver (x86) Installer"
VIAddVersionKey OriginalFilename "setupodb.exe"
VIAddVersionKey LegalCopyright "${COPYRIGHT}"
ShowUninstDetails show
UninstallIcon ${DBC_ICON}

# Installer sections
Section -Main SEC0000
    SetOutPath $INSTDIR
    SetOverwrite on
    File ${DLL_FILE_NAME}
    WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\ODBC Drivers" $(^NAME) "Installed"
    ${If} ${RunningX64}
    WriteRegExpandStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\$(^NAME)" Driver "%WINDIR%\system32\${DLL_FILE_NAME}"
    WriteRegExpandStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\$(^NAME)" Setup "%WINDIR%\system32\${DLL_FILE_NAME}"
    ${Else}
    WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\$(^NAME)" Driver "$INSTDIR\${DLL_FILE_NAME}"
    WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\$(^NAME)" Setup "$INSTDIR\${DLL_FILE_NAME}"
    ${EndIf}
    WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\$(^NAME)" APILevel "1"
    WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\$(^NAME)" ConnectFunctions "YYN"
    WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\$(^NAME)" DriverODBCVer "03.80"
    WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\$(^NAME)" FileUsage "0"
    WriteRegStr HKLM "SOFTWARE\ODBC\ODBCINST.INI\$(^NAME)" SQLLevel "0"
    WriteRegDWORD HKLM "SOFTWARE\ODBC\ODBCINST.INI\$(^NAME)" UsageCount 1
SectionEnd

Section -post SEC0001
    SetOutPath "$PROGRAMFILES32\PortableSoftwareCompany"
    WriteUninstaller "$OUTDIR\${UNINST_FILE_NAME}"
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" DisplayName "$(^Name)"
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" DisplayVersion "${VERSION}"
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" Publisher "${COMPANY}"
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" URLInfoAbout "${URL}"
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" DisplayIcon "$OUTDIR\${UNINST_FILE_NAME}"
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" UninstallString "$OUTDIR\${UNINST_FILE_NAME}"
    WriteRegDWORD HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" NoModify 1
    WriteRegDWORD HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" NoRepair 1
        
SectionEnd

# Uninstaller sections
Section -un.Main UNSEC0000
    Delete /REBOOTOK $SYSDIR\${DLL_FILE_NAME}
    DeleteRegValue HKLM "SOFTWARE\ODBC\ODBCINST.INI\ODBC Drivers" $(^NAME)
    DeleteRegKey HKLM "SOFTWARE\ODBC\ODBCINST.INI\$(^NAME)"
SectionEnd

Section -un.post UNSEC0001
    DeleteRegKey HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)"
    Delete /REBOOTOK "$PROGRAMFILES32\PortableSoftwareCompany\${UNINST_FILE_NAME}"
SectionEnd

# Installer functions
Function .onInit
    InitPluginsDir
    !insertmacro MULTIUSER_INIT
FunctionEnd

# Uninstaller functions
Function un.onInit
    !insertmacro MULTIUSER_UNINIT
FunctionEnd

