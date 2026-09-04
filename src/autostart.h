// Starting the program when the desktop session starts.
//
// Every desktop has this and no two agree on how.  Linux writes a .desktop file
// into ~/.config/autostart, macOS writes a launchd property list into
// ~/Library/LaunchAgents, and Windows writes a value under the Run key in the
// registry.  All three are per user and need no elevated privileges, which is
// what makes a checkbox an honest interface for them.
//
// The program writes and removes these itself, so the box in Manage clocks is
// the whole of the interface -- there is nothing for anyone to hand-edit.
#pragma once

class QString;

namespace autostart {

// Whether this platform has a startup mechanism the program knows how to
// write.  False means the box is not offered at all, rather than offered and
// then quietly doing nothing.
bool supported();

// Whether the program is set to start at login.  Read from the system each
// time rather than remembered, so removing the entry by hand and reopening the
// dialog shows the box unticked.
bool enabled();

// Turn it on or off.  Returns false if the entry could not be written or
// removed, in which case reason() says why.
bool setEnabled(bool on);

// Why the last setEnabled() failed, for showing to the user.
QString reason();

}  // namespace autostart
