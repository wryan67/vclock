#include "clockmanager.h"

#include "clockwindow.h"
#include "config.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace {

// Paths are the key, so two spellings of the same file must not become two
// clocks.  canonicalFilePath() is empty for a config that has not been written
// yet, which is the common case for a clock the user has only just made.
QString canonicalise(const QString &path)
{
    const QString wanted = path.isEmpty() ? configPath() : path;
    const QString canonical = QFileInfo(wanted).canonicalFilePath();
    return canonical.isEmpty() ? QDir::cleanPath(QFileInfo(wanted).absoluteFilePath())
                               : canonical;
}

}  // namespace

ClockManager &ClockManager::instance()
{
    static ClockManager manager;
    if (!manager.m_registryLoaded) {
        manager.m_registryLoaded = true;
        const bool existed = QFile::exists(registryPath());
        manager.m_registry = loadRegistry();
        // The first run builds the list from whatever configs are lying about.
        // Writing it out straight away means that list is what the user sees
        // and edits from then on, rather than being re-guessed each start.
        if (!existed)
            saveRegistry(manager.m_registry);
    }
    return manager;
}

void ClockManager::setRegistry(const Registry &registry)
{
    m_registry = registry;
    saveRegistry(m_registry);
    // A rename has to reach the window titles of the clocks already up.
    for (auto it = m_clocks.constBegin(); it != m_clocks.constEnd(); ++it)
        it.value()->refreshTitle();
    emit changed();
}

bool ClockManager::isOpen(const QString &path) const
{
    return m_clocks.contains(canonicalise(path));
}

ClockWindow *ClockManager::clockAt(const QString &path) const
{
    return m_clocks.value(canonicalise(path), nullptr);
}

QString ClockManager::nameFor(const QString &path) const
{
    const QString wanted = path.isEmpty() ? configPath() : path;
    if (const ClockEntry *entry = m_registry.findPath(wanted)) {
        if (!entry->name.isEmpty())
            return entry->name;
    }
    return fallbackClockName(wanted);
}

void ClockManager::ensureListed(const QString &path)
{
    const QString wanted = path.isEmpty() ? configPath() : path;
    if (m_registry.indexOfPath(wanted) >= 0)
        return;

    ClockEntry entry;
    const QFileInfo info(wanted);
    // A config inside the config directory is referred to by name; one the
    // user pointed at with a path of their own keeps that path.
    if (QFileInfo(info.absolutePath()) == QFileInfo(configDir()))
        entry.file = info.fileName();
    else
        entry.file = info.absoluteFilePath();
    entry.name = fallbackClockName(wanted);
    // Listed, but not recorded as showing: it was named on the command line
    // for this run, which says nothing about what should come back next time.
    entry.show = false;
    m_registry.clocks.push_back(entry);
    saveRegistry(m_registry);
}

void ClockManager::openClock(const QString &path)
{
    const QString key = canonicalise(path);
    if (ClockWindow *existing = m_clocks.value(key, nullptr)) {
        existing->show();
        existing->raise();
        existing->activateWindow();
        return;
    }

    auto *clock = new ClockWindow(path.isEmpty() ? configPath() : path);
    adopt(clock, key);

    // Placed before the first show(): a window manager applies its own
    // placement policy when a window is mapped, and on X11 that routinely wins
    // over a move() issued afterwards, leaving the clock wherever the WM felt
    // like putting it.  Positioning it while it is still unmapped makes the
    // request part of the initial geometry, which is honoured.
    clock->restorePosition();
    clock->show();
    // The clock is a frameless tool window that deliberately stays out of the
    // taskbar and the window switcher, so a window manager that maps it below
    // the windows already on screen leaves the user with no way to find it and
    // nothing apparently happening.  Asking for the front explicitly makes it
    // visible on launch without forcing "always on top" on for good.
    clock->raise();
    clock->activateWindow();

    setShown(key, true);
    emit changed();
}

void ClockManager::adopt(ClockWindow *clock, const QString &key)
{
    m_clocks.insert(key, clock);
    clock->setAttribute(Qt::WA_DeleteOnClose, true);
    connect(clock, &ClockWindow::closed, this, [this, key] { forget(key); });
}

void ClockManager::forget(const QString &key)
{
    if (m_clocks.remove(key) == 0)
        return;
    // Quitting is not hiding.  A clock closed because the program is stopping
    // is still one the user had on screen, and must come back next time.
    setShown(key, false);
    emit changed();
    quitIfDone();
}

// Record whether a clock is on screen, if that is news.  Does nothing while a
// session is being restored or torn down, so those do not rewrite the very
// state they are acting on.
void ClockManager::setShown(const QString &key, bool shown)
{
    if (m_restoring || m_quitting)
        return;
    const int index = m_registry.indexOfPath(key);
    if (index < 0 || m_registry.clocks[index].show == shown)
        return;
    m_registry.clocks[index].show = shown;
    saveRegistry(m_registry);
}

void ClockManager::closeClock(const QString &path)
{
    if (ClockWindow *clock = m_clocks.value(canonicalise(path), nullptr))
        clock->close();
}

void ClockManager::closeAll()
{
    m_quitting = true;
    // close() deletes the window and calls back into forget(), so iterate a
    // copy rather than the hash being emptied underneath us.
    const QList<ClockWindow *> clocks = m_clocks.values();
    for (ClockWindow *clock : clocks)
        clock->close();
}

// Quit leaves everything as it stands: the clocks on screen stay marked as
// showing, so starting again brings back what was there.  Only the windows go.
void ClockManager::quitNow()
{
    closeAll();
    QCoreApplication::quit();
}

void ClockManager::acquireHold()
{
    ++m_hold;
}

void ClockManager::releaseHold()
{
    if (m_hold > 0)
        --m_hold;
    quitIfDone();
}

void ClockManager::quitIfDone()
{
    if (m_clocks.isEmpty() && m_hold == 0)
        qApp->quit();
}

void ClockManager::openVisible()
{
    // Restoring, not choosing: the flags are already right, and writing them
    // back one clock at a time as each window appears would be pointless.
    m_restoring = true;
    bool any = false;
    for (const ClockEntry &entry : m_registry.clocks) {
        if (!entry.show)
            continue;
        openClock(entry.path());
        any = true;
    }
    m_restoring = false;
    // Hiding every clock and stopping would otherwise leave a program with no
    // windows and no way to reach the menu that fixes it, so the default clock
    // comes back rather than starting invisible.
    if (!any)
        openClock(configPath());
}

void ClockManager::openPaths(const QVector<QString> &paths)
{
    // A clock named with --config is showing for this run only; it does not
    // rewrite what comes back next time.
    m_restoring = true;
    for (const QString &path : paths) {
        ensureListed(path);
        openClock(path);
    }
    m_restoring = false;
}
