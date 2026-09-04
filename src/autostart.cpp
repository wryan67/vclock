#include "autostart.h"

#include "render.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>
#include <QtGlobal>

#if defined(Q_OS_WIN)
#  include <QSettings>
#endif

namespace {

QString g_reason;

// Where the program actually is.  This resolves any symlink it was started
// through, so the entry names the binary that is running rather than a name
// that happens to point at it today.
QString programPath()
{
    return QCoreApplication::applicationFilePath();
}

}  // namespace

// ---------------------------------------------------------------------------
#if defined(Q_OS_WIN)
// ---------------------------------------------------------------------------
//
// A value under HKEY_CURRENT_USER\...\Run, which Windows runs once the desktop
// is up.  Per user, so it needs no administrator rights; the installer clears
// it on uninstall, since a Run entry naming a deleted program is looked for at
// every login.

namespace {

const char *kRunKey =
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const char *kValueName = "vclock";

// The registry stores a command line, not a path, so a program somewhere with
// a space in its name has to be quoted or everything after the space is read
// as an argument.  "C:\Program Files\..." is the normal case, not the corner
// case, on Windows.
QString command()
{
    return QLatin1Char('"') + QDir::toNativeSeparators(programPath()) +
           QLatin1Char('"');
}

}  // namespace

namespace autostart {

bool supported() { return true; }

bool enabled()
{
    QSettings run(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
    return run.contains(QString::fromLatin1(kValueName));
}

bool setEnabled(bool on)
{
    g_reason.clear();

    QSettings run(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
    if (on)
        run.setValue(QString::fromLatin1(kValueName), command());
    else
        run.remove(QString::fromLatin1(kValueName));

    // QSettings reports registry trouble only when asked, and writing to the
    // user's own hive failing at all means something is unusual enough to say
    // out loud rather than swallow.
    run.sync();
    if (run.status() != QSettings::NoError) {
        g_reason = on ? QStringLiteral("could not write the registry Run entry")
                      : QStringLiteral("could not remove the registry Run entry");
        return false;
    }
    return true;
}

}  // namespace autostart

// ---------------------------------------------------------------------------
#elif defined(Q_OS_MACOS)
// ---------------------------------------------------------------------------
//
// A launchd agent.  launchd reads ~/Library/LaunchAgents when the user logs in,
// and RunAtLoad is what distinguishes "start this now" from "start it when
// something asks for it".
//
// The plist is written directly rather than through launchctl so that ticking
// the box has an effect without the program having to shell out, and so that
// the state can be read back from one place.

namespace {

const char *kLabel = "org.vclock.vclock";

QString entryPath()
{
    return QDir::homePath() + QStringLiteral("/Library/LaunchAgents/") +
           QString::fromLatin1(kLabel) + QStringLiteral(".plist");
}

// & < > are the three that matter in an XML text node; a path can legally
// contain all of them.
QString xmlEscaped(const QString &text)
{
    QString out = text;
    out.replace(QLatin1Char('&'), QLatin1String("&amp;"));
    out.replace(QLatin1Char('<'), QLatin1String("&lt;"));
    out.replace(QLatin1Char('>'), QLatin1String("&gt;"));
    return out;
}

}  // namespace

namespace autostart {

bool supported() { return true; }

bool enabled()
{
    return QFile::exists(entryPath());
}

bool setEnabled(bool on)
{
    g_reason.clear();
    const QString path = entryPath();

    if (!on) {
        if (!QFile::exists(path))
            return true;
        if (!QFile::remove(path)) {
            g_reason = QStringLiteral("could not remove %1").arg(path);
            return false;
        }
        return true;
    }

    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        g_reason =
            QStringLiteral("could not create %1").arg(QFileInfo(path).absolutePath());
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        g_reason = QStringLiteral("could not write %1").arg(path);
        return false;
    }
    QTextStream out(&file);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\""
        << " \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        << "<plist version=\"1.0\">\n"
        << "<dict>\n"
        << "\t<key>Label</key>\n"
        << "\t<string>" << QString::fromLatin1(kLabel) << "</string>\n"
        << "\t<key>ProgramArguments</key>\n"
        << "\t<array>\n"
        << "\t\t<string>" << xmlEscaped(programPath()) << "</string>\n"
        << "\t</array>\n"
        << "\t<key>RunAtLoad</key>\n"
        << "\t<true/>\n"
        // Otherwise launchd restarts it every time it exits, so quitting the
        // program would bring it straight back.
        << "\t<key>KeepAlive</key>\n"
        << "\t<false/>\n"
        // Only in a graphical login session.  Without this it would also be
        // started for ssh and cron sessions, where there is no display to draw
        // a clock on.
        << "\t<key>LimitLoadToSessionType</key>\n"
        << "\t<string>Aqua</string>\n"
        << "</dict>\n"
        << "</plist>\n";
    out.flush();
    if (!file.commit()) {
        g_reason = QStringLiteral("could not write %1").arg(path);
        return false;
    }
    return true;
}

}  // namespace autostart

// ---------------------------------------------------------------------------
#elif defined(Q_OS_UNIX)
// ---------------------------------------------------------------------------
//
// The freedesktop autostart convention: a .desktop file in ~/.config/autostart,
// which every desktop with a "startup applications" list reads.

namespace {

QString entryPath()
{
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return base + QStringLiteral("/autostart/vclock.desktop");
}

// A packaged install puts vclock.svg in the icon theme, and naming the theme
// entry is better than naming a file: it lets the desktop pick the size it
// wants and survives the package being upgraded underneath it.
bool themeIconInstalled()
{
    const QStringList dirs =
        QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString &dir : dirs) {
        if (QFile::exists(dir +
                          QStringLiteral("/icons/hicolor/scalable/apps/vclock.svg")))
            return true;
    }
    return false;
}

// Only needed when there is no installed icon to name -- running from a build
// directory, most likely.  The program draws its own icon, so there is no file
// on disk to point the entry at until we make one.
QString drawnIconPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
           QStringLiteral("/vclock/icon.png");
}

bool writeDrawnIcon()
{
    const QString path = drawnIconPath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        g_reason =
            QStringLiteral("could not create %1").arg(QFileInfo(path).absolutePath());
        return false;
    }
    // Big enough for any menu or dock; the entry is scaled down from here.
    if (!appIconImage(256).save(path, "PNG")) {
        g_reason = QStringLiteral("could not write %1").arg(path);
        return false;
    }
    return true;
}

// Exec is a command line, not a path: a program somewhere with a space in its
// name has to be quoted or the rest of it reads as an argument.
QString execField()
{
    const QString path = programPath();
    if (!path.contains(QLatin1Char(' ')))
        return path;
    QString quoted = path;
    quoted.replace(QLatin1String("\\"), QLatin1String("\\\\"));
    quoted.replace(QLatin1String("\""), QLatin1String("\\\""));
    return QLatin1Char('"') + quoted + QLatin1Char('"');
}

}  // namespace

namespace autostart {

bool supported() { return true; }

bool enabled()
{
    return QFile::exists(entryPath());
}

bool setEnabled(bool on)
{
    g_reason.clear();
    const QString path = entryPath();

    if (!on) {
        if (!QFile::exists(path))
            return true;
        if (!QFile::remove(path)) {
            g_reason = QStringLiteral("could not remove %1").arg(path);
            return false;
        }
        return true;
    }

    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        g_reason =
            QStringLiteral("could not create %1").arg(QFileInfo(path).absolutePath());
        return false;
    }

    QString icon = QStringLiteral("vclock");
    if (!themeIconInstalled()) {
        if (!writeDrawnIcon())
            return false;
        icon = drawnIconPath();
    }

    // Written whole or not at all: a half-written entry is one the desktop
    // would try to run.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        g_reason = QStringLiteral("could not write %1").arg(path);
        return false;
    }
    QTextStream out(&file);
    out << "[Desktop Entry]\n"
        << "Type=Application\n"
        << "Version=1.0\n"
        << "Name=vclock\n"
        << "Comment=A transparent analog desktop clock\n"
        << "Exec=" << execField() << "\n"
        << "Icon=" << icon << "\n"
        << "Terminal=false\n"
        << "StartupNotify=false\n"
        << "StartupWMClass=vclock\n"
        // Written by the program, so it is on the moment the file exists.  Some
        // desktops read only this, others only the file's presence; saying both
        // keeps them in agreement.
        << "Hidden=false\n"
        << "X-GNOME-Autostart-enabled=true\n";
    out.flush();
    if (!file.commit()) {
        g_reason = QStringLiteral("could not write %1").arg(path);
        return false;
    }
    return true;
}

}  // namespace autostart

// ---------------------------------------------------------------------------
#else
// ---------------------------------------------------------------------------
//
// Somewhere with no startup convention this program knows.  Manage clocks asks
// supported() first and leaves the box out entirely, so these are never called.

namespace autostart {

bool supported() { return false; }
bool enabled()   { return false; }

bool setEnabled(bool)
{
    g_reason = QStringLiteral("this platform has no startup mechanism vclock knows");
    return false;
}

}  // namespace autostart

#endif

namespace autostart {

QString reason()
{
    return g_reason;
}

}  // namespace autostart
