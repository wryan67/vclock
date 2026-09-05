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

; Without these the installer and the Add/Remove Programs entry wear NSIS's
; default icon rather than the program's.  The paths are supplied by
; package.sh, since makensis resolves them relative to its working directory.
!define MUI_ICON "${ICON}"
!define MUI_UNICON "${ICON}"

; Ticked by default, which is the usual thing for an installer and is worth a
; little more here than usual: installing over a running copy closes it first,
; so on an upgrade the user's clocks were on screen a moment ago and leaving
; them with nothing running is the surprising outcome.
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Run vclock now"
!define MUI_FINISHPAGE_RUN_FUNCTION LaunchVclock

!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

; Writing to Program Files needs an elevated installer, but vclock itself must
; not inherit that.  Everything it remembers is per user -- the configs under
; %APPDATA% and the "Start at login" value under HKCU -- so a first run under
; the wrong token writes them into the wrong profile, and the settings then
; appear to vanish the next time the program is started normally.
;
; Exec would hand the program the installer's elevated token, so the launch
; goes through Explorer instead.  Explorer runs as the logged-in user, and what
; it starts inherits its token rather than ours.  This needs no plugin, which
; matters because the NSIS in the build container ships neither UAC nor
; ShellExecAsUser.
Function LaunchVclock
    Exec '"$WINDIR\explorer.exe" "$INSTDIR\vclock.exe"'
FunctionEnd

Function .onInit
    ; The payload is 64-bit; on a 32-bit Windows it would install and then
    ; refuse to start, which is a worse outcome than declining up front.
    ${IfNot} ${RunningX64}
        MessageBox MB_ICONSTOP "vclock is 64-bit and this is a 32-bit Windows."
        Abort
    ${EndIf}
    SetRegView 64
FunctionEnd

; Windows locks a running program's file, so installing over a copy that is
; still on screen fails partway through with "Error opening file for writing"
; -- having already replaced some of the DLLs.  The running copy is therefore
; asked to close before a single file is touched.
;
; taskkill without /F posts WM_CLOSE, which vclock handles: it writes out its
; settings and, because the close did not come from the Hide menu, treats it as
; the program stopping rather than as putting one clock away.  Every clock that
; was on screen stays marked as showing and they all come back afterwards.
; /F skips all of that, so it is only used as a last resort for a copy that has
; stopped answering.
;
; taskkill is part of Windows, so this needs no NSIS plugin beyond nsExec,
; which ships with NSIS itself.  Expanded twice, once for the installer and
; once for the uninstaller, which cannot share a function.
!macro CloseRunningVclock un
Function ${un}CloseRunningVclock
    Push $0
    Push $1
    Push $2

    StrCpy $1 0
    ; Exit code 0 means taskkill found the process and asked it to close, so
    ; there is something to wait for; anything else means none was running.
    ask:
        nsExec::ExecToStack '"$SYSDIR\taskkill.exe" /IM vclock.exe'
        Pop $0
        Pop $2
        StrCmp $0 0 0 gone
        Sleep 1000
        IntOp $1 $1 + 1
        IntCmp $1 10 force ask force

    force:
        nsExec::ExecToStack '"$SYSDIR\taskkill.exe" /F /IM vclock.exe'
        Pop $0
        Pop $2
        ; Killing the process does not release its file handles instantly.
        Sleep 1000

    gone:
    Pop $2
    Pop $1
    Pop $0
FunctionEnd
!macroend

!insertmacro CloseRunningVclock ""
!insertmacro CloseRunningVclock "un."

Section "vclock" SecMain
    SectionIn RO

    Call CloseRunningVclock

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

    ; Same problem as installing: the files cannot be deleted while they are in
    ; use, and an uninstall that leaves the program behind is worse than one
    ; that closes it.
    Call un.CloseRunningVclock

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
