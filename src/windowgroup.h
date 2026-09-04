// Every window a process opens on X11 belongs, by default, to one window
// group. Window managers stack a group as a unit: whatever the topmost member
// is, the rest are lifted to meet it. That is sensible for a document window
// and its dialogs, and quite wrong for a set of clocks that are only related
// by having been started together, because turning "always on top" on for one
// of them turns it on for all of them.
#pragma once

class QWidget;

namespace windowgroup {

// Take the window out of the shared group, so that its stacking is decided by
// its own state and nothing else. Safe to call more than once, and does
// nothing on platforms where the idea does not apply.
void detach(QWidget *widget);

// Ask the window manager to keep this window above the others, or to stop.
// Returns false where there is no way to say so directly -- on those
// platforms the caller has to fall back on Qt's window flag, which costs a
// rebuild of the native window. The widget must already have one.
bool setAlwaysOnTop(QWidget *widget, bool on);

}  // namespace windowgroup
