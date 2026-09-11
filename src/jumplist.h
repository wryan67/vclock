// The right-click menu on the Windows taskbar button.
//
// Pinning vclock and right-clicking it should offer more than "Unpin", because
// the program's own menu lives on the clock face, and a clock that is hidden
// has no face to right-click.  The taskbar is then the only thing left
// pointing at the program, so Manage clocks belongs on it.
//
// Windows calls this a Jump List.  Qt5 had QWinJumpList for it; Qt6 dropped
// QtWinExtras, so this is the COM underneath that class, which is a dozen
// calls.  Everywhere else it does nothing, so the caller never has to ask what
// platform it is on.
#pragma once

namespace jumplist {

// Put vclock's tasks on the taskbar button.  Called once at startup: the shell
// keeps the list, but it keeps the last one registered, so a version with new
// tasks has to say so.
void install();

}  // namespace jumplist
