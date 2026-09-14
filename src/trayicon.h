// The icon in the notification area, and the menu behind it.
//
// A clock has no frame, no taskbar button and no entry in the window switcher,
// so a program made only of clocks can be on screen and still be hard to get
// hold of: one sent behind a maximised window leaves nothing to right-click.
// The launcher icon answers that from outside the program; this answers it
// from inside, and is the one place that is there whatever the clocks are
// doing.
#pragma once

namespace tray {

// Put the icon up, if the desktop has somewhere to put it.  Safe to call when
// it is already up, and safe to call on a desktop with no tray at all, where
// it quietly does nothing.
void install();

}  // namespace tray
