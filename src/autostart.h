// Starting the program when the desktop session starts.
//
// This is the freedesktop autostart convention: a .desktop file in
// ~/.config/autostart, which every desktop that has a "startup applications"
// list reads.  The file is written and removed by the program itself, so the
// box in Manage clocks is the whole of the interface -- there is nothing for
// anyone to hand-edit.
#pragma once

class QString;

namespace autostart {

// Whether the program is set to start at login.
bool enabled();

// Turn it on or off.  Returns false if the file could not be written or
// removed, in which case reason() says why.
bool setEnabled(bool on);

// Why the last setEnabled() failed, for showing to the user.
QString reason();

}  // namespace autostart
