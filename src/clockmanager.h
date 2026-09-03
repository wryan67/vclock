// Owns the running clocks and the registry that lists them.
//
// The clocks used to be a vector in main() and the program ended when a static
// counter in ClockWindow reached zero.  Now that a clock can be opened and
// closed from the manage dialog, one place has to know which are running, and
// it has to be able to hold the program open while that dialog is up -- closing
// the last clock from the dialog must not take the dialog down with it.
#pragma once

#include "registry.h"

#include <QHash>
#include <QObject>
#include <QString>

class ClockWindow;

class ClockManager : public QObject
{
    Q_OBJECT

public:
    static ClockManager &instance();

    const Registry &registry() const { return m_registry; }

    // Replace the registry and write it out.  Emits changed().
    void setRegistry(const Registry &registry);

    // Open every clock the registry has marked as showing, or the default one
    // when none is.  Used at startup when no --config was given.
    void openVisible();

    // Open the clocks at these paths, whether or not the registry lists them,
    // adding any it does not know about.  Used for --config.
    void openPaths(const QVector<QString> &paths);

    bool isOpen(const QString &path) const;
    ClockWindow *clockAt(const QString &path) const;

    // Show the clock for this config, raising it if it is already up.
    void openClock(const QString &path);
    void closeClock(const QString &path);
    void closeAll();

    // End the program: close every clock and stop, whatever holds are out.
    // Hiding the last clock ends the program too, but only when nothing is
    // holding it open; Quit means it either way.
    void quitNow();

    int openCount() const { return m_clocks.size(); }

    // The display name for a config path, falling back to its base name when
    // the registry has no entry for it.
    QString nameFor(const QString &path) const;

    // While held, closing the last clock does not end the program.  The
    // manage dialog holds one for as long as it is open so that emptying the
    // list leaves the user somewhere to add a clock back from.
    void acquireHold();
    void releaseHold();

signals:
    // The set of running clocks, or the registry, has changed.
    void changed();

private:
    ClockManager() = default;

    void adopt(ClockWindow *clock, const QString &path);
    void forget(const QString &path);
    void setShown(const QString &key, bool shown);
    void quitIfDone();
    // Make sure a clock started from the command line is listed, so it shows
    // up in the manage dialog like any other.
    void ensureListed(const QString &path);

    Registry m_registry;
    bool m_registryLoaded = false;
    QHash<QString, ClockWindow *> m_clocks;  // keyed on canonical config path
    int m_hold = 0;
    bool m_quitting = false;
    // Set while the startup set is being opened, so restoring the saved state
    // does not write it back out again.
    bool m_restoring = false;
};
