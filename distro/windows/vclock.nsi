; The Windows installer.
;
; VERSION, STAGE and OUTFILE are supplied by package.sh, which assembles STAGE
; from the cross-built binary, the Qt DLLs it links against and the plugins Qt
; loads by directory at runtime.

Unicode true
SetCompressor /SOLID lzma

!include "MUI2.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"

Name "vclock ${VERSION}"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\vclock"
InstallDirRegKey HKLM "Software\vclock" "InstallDir"

; Writing to Program Files and to HKLM both need it.
RequestExecutionLevel admin

VIProductVersion "${VERSION}.0.0"
VIAddVersionKey "ProductName" "vclock"
VIAddVersionKey "FileDescription" "A transparent analog desktop clock"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" ""

!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
    ; The payload is 64-bit; on a 32-bit Windows it would install and then
    ; refuse to start, which is a worse outcome than declining up front.
    ${IfNot} ${RunningX64}
        MessageBox MB_ICONSTOP "vclock is 64-bit and this is a 32-bit Windows."
        Abort
    ${EndIf}
    SetRegView 64
FunctionEnd

Section "vclock" SecMain
    SectionIn RO
    SetOutPath "$INSTDIR"

    File "${STAGE}\vclock.exe"
    File "${STAGE}\*.dll"
    File "${STAGE}\vclock.svg"

    ; Qt looks for these in directories beside the executable.
    SetOutPath "$INSTDIR\platforms"
    File "${STAGE}\platforms\*.dll"

    SetOutPath "$INSTDIR\styles"
    File /nonfatal "${STAGE}\styles\*.dll"

    SetOutPath "$INSTDIR\imageformats"
    File /nonfatal "${STAGE}\imageformats\*.dll"

    SetOutPath "$INSTDIR\iconengines"
    File /nonfatal "${STAGE}\iconengines\*.dll"

    SetOutPath "$INSTDIR"

    CreateDirectory "$SMPROGRAMS\vclock"
    CreateShortCut "$SMPROGRAMS\vclock\vclock.lnk" "$INSTDIR\vclock.exe"
    CreateShortCut "$SMPROGRAMS\vclock\Uninstall vclock.lnk" "$INSTDIR\Uninstall.exe"

    WriteRegStr HKLM "Software\vclock" "InstallDir" "$INSTDIR"

    !define UNINST_KEY \
        "Software\Microsoft\Windows\CurrentVersion\Uninstall\vclock"
    WriteRegStr HKLM "${UNINST_KEY}" "DisplayName" "vclock"
    WriteRegStr HKLM "${UNINST_KEY}" "DisplayVersion" "${VERSION}"
    WriteRegStr HKLM "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\vclock.exe"
    WriteRegStr HKLM "${UNINST_KEY}" "Publisher" "vclock"
    WriteRegStr HKLM "${UNINST_KEY}" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
    WriteRegStr HKLM "${UNINST_KEY}" "QuietUninstallString" "$\"$INSTDIR\Uninstall.exe$\" /S"
    WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
    WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1

    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "${UNINST_KEY}" "EstimatedSize" "$0"

    WriteUninstaller "$INSTDIR\Uninstall.exe"
SectionEnd

Section "Uninstall"
    SetRegView 64

    ; The program writes this itself when "Start at login" is ticked, so it has
    ; to be cleared here or Windows goes looking for a deleted executable at
    ; every login.  It lives under HKCU because it is per user.
    DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "vclock"

    Delete "$INSTDIR\vclock.exe"
    Delete "$INSTDIR\vclock.svg"
    Delete "$INSTDIR\*.dll"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir /r "$INSTDIR\platforms"
    RMDir /r "$INSTDIR\styles"
    RMDir /r "$INSTDIR\imageformats"
    RMDir /r "$INSTDIR\iconengines"
    RMDir "$INSTDIR"

    Delete "$SMPROGRAMS\vclock\vclock.lnk"
    Delete "$SMPROGRAMS\vclock\Uninstall vclock.lnk"
    RMDir "$SMPROGRAMS\vclock"

    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vclock"
    DeleteRegKey HKLM "Software\vclock"
SectionEnd
