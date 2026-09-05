#!/bin/bash
#
# uninstall.sh -- remove vclock from a Mac.
#
#     ./uninstall.sh [--purge] [--yes]
#
#       --purge   settings too; the folder is renamed to .bak, not deleted
#       --yes     do not ask
#
# macOS has no uninstaller of its own.  An app installed by dragging it out of
# a disk image is removed by dragging it to the Trash, and for most programs
# that is genuinely all there is to it -- everything they own lives inside the
# bundle.
#
# vclock is not quite one of those programs.  Ticking "Start at login" writes a
# launchd agent to ~/Library/LaunchAgents, which is outside the bundle and so
# survives the Trash.  What is left behind is an agent naming an application
# that no longer exists, which launchd dutifully tries to start at every login
# for as long as the account lasts.  That is what this script is mainly for.
#
# It is deliberately readable and deliberately conservative: it prints what it
# is about to do, it never deletes settings (it renames them), and it does
# nothing at all without confirmation unless --yes says otherwise.
#
# It needs no administrator rights.  /Applications is writable by admin users,
# which is who installed the app in the first place, and everything else it
# touches is inside the user's own home directory.

set -euo pipefail

LABEL=org.vclock.vclock
APP=/Applications/vclock.app
AGENT=$HOME/Library/LaunchAgents/$LABEL.plist
SETTINGS=$HOME/Library/Application\ Support/vclock

purge=0
assume_yes=0

while [ $# -gt 0 ]; do
    case $1 in
        --purge) purge=1; shift ;;
        --yes|-y) assume_yes=1; shift ;;
        -h|--help) sed -n '2,27p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "unknown option $1" >&2; exit 1 ;;
    esac
done

[ "$(uname -s)" = "Darwin" ] || {
    echo "error: this uninstalls a macOS app and has to run on macOS" >&2
    exit 1
}

# The app may have been dragged somewhere other than /Applications, or to the
# Trash already.  Neither is a reason to leave the launch agent behind, so a
# missing app is reported and then ignored.
if [ ! -d "$APP" ]; then
    echo "note: $APP is not there; looking for the rest anyway"
fi

echo "This will remove:"
[ -d "$APP" ]      && echo "    $APP"
[ -f "$AGENT" ]    && echo "    $AGENT  (start at login)"
if [ "$purge" = 1 ] && [ -d "$SETTINGS" ]; then
    echo "    $SETTINGS  -> renamed to vclock.bak"
elif [ -d "$SETTINGS" ]; then
    echo "and will keep:"
    echo "    $SETTINGS  (pass --purge to remove it too)"
fi
echo

if [ "$assume_yes" != 1 ]; then
    printf "Go ahead? [y/N] "
    read -r reply
    case $reply in
        y|Y|yes|YES) ;;
        *) echo "nothing done"; exit 0 ;;
    esac
fi

# ------------------------------------------------------------- start at login
#
# Unloaded before the file is removed.  launchd holds the agent in memory once
# it has read it, so deleting the plist on its own leaves the job registered
# until the next login -- and if it is one of the "keep alive" kind, running.
#
# bootout is the modern spelling and `unload` the old one; which of the two is
# available depends on the macOS version, so both are tried and neither is
# allowed to fail the script.  An agent that was never loaded makes both
# complain, which is not an error here.
if [ -f "$AGENT" ]; then
    launchctl bootout "gui/$(id -u)/$LABEL" 2>/dev/null ||
        launchctl unload "$AGENT" 2>/dev/null || true
    rm -f "$AGENT"
    echo "removed the login agent"
fi

# ------------------------------------------------------------------- the app
#
# Quit first.  Deleting a running application's bundle out from under it leaves
# it running with no files to page in, and on the way out it would write its
# settings back -- putting back the very folder --purge is about to move aside.
if pgrep -x vclock >/dev/null 2>&1; then
    osascript -e 'tell application "vclock" to quit' 2>/dev/null || true
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        pgrep -x vclock >/dev/null 2>&1 || break
        sleep 1
    done
    # Only for a copy that has stopped answering; this skips the orderly
    # shutdown, and with it the chance to save.
    pgrep -x vclock >/dev/null 2>&1 && pkill -x vclock || true
    sleep 1
fi

if [ -d "$APP" ]; then
    rm -rf "$APP"
    echo "removed $APP"
fi

# --------------------------------------------------------------- the settings
#
# Renamed rather than deleted, matching the Windows uninstaller: a misread
# prompt then costs a rename instead of every clock the user had arranged.
# One backup is kept, so a second run replaces the first rather than leaving a
# collection of folders nobody will ever look at again.
if [ "$purge" = 1 ] && [ -d "$SETTINGS" ]; then
    rm -rf "$SETTINGS.bak"
    mv "$SETTINGS" "$SETTINGS.bak"
    echo "settings moved to $SETTINGS.bak"
fi

echo "done"
