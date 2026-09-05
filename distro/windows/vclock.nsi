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
!include "LogicLib.nsh"
!include "nsDialogs.nsh"

; What the maintenance page asked for.  Only ever consulted when vclock was
; already installed; a first install has nothing to choose between.
!define MODE_UPGRADE     0   ; install over the top, keeping settings
!define MODE_RESET       1   ; put settings aside, then install
!define MODE_RESET_ONLY  2   ; put settings aside and stop

Var InstallMode
Var PreviousInstall     ; where an existing copy lives, empty if there is none
Var RadioUpgrade
Var RadioReset
Var RadioResetOnly
Var RemoveSettings      ; the uninstaller's "settings too" box
Var CheckRemoveSettings ; ... and the control itself
Var FinishTitle         ; the last page says different things in different modes
Var FinishText

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

; A reset-only run installs nothing, so the stock "has been installed on your
; computer" would be a plain lie.  MUI takes these as compile-time defines, but
; what it does with them is build a language string, and NSIS expands variables
; in those when they are used rather than when they are declared -- so a
; variable here is filled in at runtime, once the mode is known.
!define MUI_FINISHPAGE_TITLE "$FinishTitle"
!define MUI_FINISHPAGE_TEXT "$FinishText"

; Offered only when there is an existing copy to reinstall over or reset; on a
; first install there is nothing to decide, so the page is skipped rather than
; shown with its one meaningful option.
Page custom ModePageCreate ModePageLeave

; An existing install has a location already and reset-only installs nothing,
; so neither has any use for the directory page.
!define MUI_PAGE_CUSTOMFUNCTION_PRE DirectoryPagePre
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

; The stock confirm page cannot carry a checkbox, and asking twice -- once to
; confirm, once for the settings -- is worse than one page that does both.
UninstPage custom un.OptionsPageCreate un.OptionsPageLeave
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

    StrCpy $InstallMode ${MODE_UPGRADE}

    ; An existing install fixes where this one goes, so the directory page has
    ; nothing to ask and the maintenance page has something to offer.
    ReadRegStr $PreviousInstall HKLM "Software\vclock" "InstallDir"
    ${If} $PreviousInstall != ""
        StrCpy $INSTDIR $PreviousInstall
    ${EndIf}

    ; A silent install cannot click a radio button, so the same choices are
    ; available as switches for anyone scripting this.
    Push $0
    Push $1
    ${GetParameters} $0
    ClearErrors
    ${GetOptions} $0 "/RESETONLY" $1
    ${IfNot} ${Errors}
        StrCpy $InstallMode ${MODE_RESET_ONLY}
    ${Else}
        ClearErrors
        ${GetOptions} $0 "/RESET" $1
        ${IfNot} ${Errors}
            StrCpy $InstallMode ${MODE_RESET}
        ${EndIf}
    ${EndIf}
    ClearErrors
    Pop $1
    Pop $0

    Call UpdateFinishText
FunctionEnd

; The wording on the last page depends on what was actually done, and the mode
; is settled in two places -- a switch in .onInit, or the radio buttons later --
; so both call this rather than each carrying its own copy of the strings.
Function UpdateFinishText
    ${If} $InstallMode == ${MODE_RESET_ONLY}
        StrCpy $FinishTitle "vclock settings reset"
        StrCpy $FinishText "vclock's settings have been put aside in \
            $APPDATA\vclock.bak, so it will start with its defaults.$\r$\n$\r$\n\
            The program itself was left exactly as it was."
    ${Else}
        StrCpy $FinishTitle "Completing vclock Setup"
        StrCpy $FinishText "vclock ${VERSION} has been installed on your \
            computer.$\r$\n$\r$\nClick Finish to close Setup."
    ${EndIf}
FunctionEnd

; Settings are per user, and this installer is elevated.  When the person at
; the keyboard is an administrator and merely clicked through the UAC prompt,
; the elevated process keeps their profile and $APPDATA is theirs.  When a
; standard user instead typed somebody else's administrator credentials, the
; elevated process belongs to that other account and $APPDATA points at its
; profile, where there is unlikely to be anything to reset.
;
; Resolving the invoking user's profile from an elevated process needs the
; shell's token, which is a good deal of Win32 for something that would go
; wrong silently and destructively.  So the path is printed on the page
; instead: the one case where this does the wrong thing is the one where the
; wrong path is on screen to see.
;
; Nothing is deleted either -- the folder is renamed -- so a wrong guess or a
; misclick costs a rename rather than every clock the user had.
!macro ResetSettings un
Function ${un}ResetSettings
    ${If} ${FileExists} "$APPDATA\vclock\*.*"
        ; Only one backup is kept; a second reset replaces the first, which is
        ; more predictable than accumulating folders nobody ever removes.
        RMDir /r "$APPDATA\vclock.bak"
        Rename "$APPDATA\vclock" "$APPDATA\vclock.bak"
    ${EndIf}
    ; Start at login is a setting like any other, so a reset clears it too.
    DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "vclock"
FunctionEnd
!macroend

!insertmacro ResetSettings ""
!insertmacro ResetSettings "un."

Function DirectoryPagePre
    ${If} $PreviousInstall != ""
    ${OrIf} $InstallMode == ${MODE_RESET_ONLY}
        Abort
    ${EndIf}
FunctionEnd

Function ModePageCreate
    ; Nothing installed means nothing to upgrade, repair or reset.
    ${If} $PreviousInstall == ""
        Abort
    ${EndIf}

    !insertmacro MUI_HEADER_TEXT "Existing installation" \
        "vclock is already installed. Choose what this should do."

    nsDialogs::Create 1018
    Pop $0
    ${If} $0 == error
        Abort
    ${EndIf}

    ${NSD_CreateLabel} 0 0 100% 20u \
        "Found in $PreviousInstall$\r$\nSettings folder: $APPDATA\vclock"
    Pop $1

    ${NSD_CreateRadioButton} 0 26u 100% 11u "Upgrade or repair, keeping my settings"
    Pop $RadioUpgrade
    ${NSD_CreateRadioButton} 0 41u 100% 11u "Reset all settings, then reinstall"
    Pop $RadioReset
    ${NSD_CreateRadioButton} 0 56u 100% 11u "Reset all settings only, without reinstalling"
    Pop $RadioResetOnly

    ; Windows treats consecutive radio buttons as one group only when the first
    ; carries WS_GROUP; without it all three can be ticked at once.
    ${NSD_AddStyle} $RadioUpgrade ${WS_GROUP}

    ${NSD_CreateLabel} 0 74u 100% 24u \
        "Resetting renames the settings folder to vclock.bak rather than \
         deleting it, replacing any previous backup. Start at login is a \
         setting and is cleared too."
    Pop $1

    ${If} $InstallMode == ${MODE_RESET}
        ${NSD_Check} $RadioReset
    ${ElseIf} $InstallMode == ${MODE_RESET_ONLY}
        ${NSD_Check} $RadioResetOnly
    ${Else}
        ${NSD_Check} $RadioUpgrade
    ${EndIf}

    nsDialogs::Show
FunctionEnd

Function ModePageLeave
    ${NSD_GetState} $RadioResetOnly $0
    ${If} $0 == ${BST_CHECKED}
        StrCpy $InstallMode ${MODE_RESET_ONLY}
    ${Else}
        ${NSD_GetState} $RadioReset $0
        ${If} $0 == ${BST_CHECKED}
            StrCpy $InstallMode ${MODE_RESET}
        ${Else}
            StrCpy $InstallMode ${MODE_UPGRADE}
        ${EndIf}
    ${EndIf}
    Call UpdateFinishText
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

    ; Before the settings are touched as well as before the files: a running
    ; copy writes its settings out as it closes, which would put back the very
    ; folder a reset has just moved aside.
    Call CloseRunningVclock

    ${If} $InstallMode != ${MODE_UPGRADE}
        Call ResetSettings
    ${EndIf}

    ; Reset-only leaves the installed program exactly as it was, so there is
    ; nothing further to write -- including the uninstaller and its registry
    ; entries, which are already there and still correct.
    ${If} $InstallMode == ${MODE_RESET_ONLY}
        Return
    ${EndIf}

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

Function un.onInit
    SetRegView 64
    StrCpy $RemoveSettings 0
    ; So an unattended uninstall can still say which it wants.
    Push $0
    Push $1
    ${GetParameters} $0
    ClearErrors
    ${GetOptions} $0 "/PURGE" $1
    ${IfNot} ${Errors}
        StrCpy $RemoveSettings 1
    ${EndIf}
    ClearErrors
    Pop $1
    Pop $0
FunctionEnd

Function un.OptionsPageCreate
    !insertmacro MUI_HEADER_TEXT "Uninstall vclock" \
        "This removes vclock from $INSTDIR."

    nsDialogs::Create 1018
    Pop $0
    ${If} $0 == error
        Abort
    ${EndIf}

    ; Unticked: settings are cheap to keep and expensive to lose, and an
    ; uninstall is often just the first half of an upgrade.
    ${NSD_CreateCheckbox} 0 10u 100% 11u "Also remove my settings"
    Pop $CheckRemoveSettings
    ${If} $RemoveSettings == 1
        ${NSD_Check} $CheckRemoveSettings
    ${EndIf}

    ${NSD_CreateLabel} 0 26u 100% 56u \
        "Settings live in $APPDATA\vclock. Leaving them means a later \
         reinstall finds the clocks exactly as they were. Removing them \
         renames the folder to vclock.bak rather than deleting it.$\r$\n$\r$\n\
         Start at login is cleared either way, since an entry naming a \
         program that is gone is one Windows looks for at every login."
    Pop $1

    nsDialogs::Show
FunctionEnd

Function un.OptionsPageLeave
    ${NSD_GetState} $CheckRemoveSettings $0
    ${If} $0 == ${BST_CHECKED}
        StrCpy $RemoveSettings 1
    ${Else}
        StrCpy $RemoveSettings 0
    ${EndIf}
FunctionEnd

Section "Uninstall"
    SetRegView 64

    ; Same problem as installing: the files cannot be deleted while they are in
    ; use, and an uninstall that leaves the program behind is worse than one
    ; that closes it.
    Call un.CloseRunningVclock

    ; The program writes this itself when "Start at login" is ticked, so it has
    ; to be cleared here or Windows goes looking for a deleted executable at
    ; every login.  It lives under HKCU because it is per user.  Cleared even
    ; when the settings are being kept: retaining settings is about the clocks
    ; coming back, not about launching a program that is no longer installed.
    DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "vclock"

    ${If} $RemoveSettings == 1
        Call un.ResetSettings
    ${EndIf}

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
