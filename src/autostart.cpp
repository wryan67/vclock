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

namespace {

QString g_reason;

QString entryPath()
{
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return base + QStringLiteral("/autostart/vclock.desktop");
}

// The icon is drawn rather than shipped, so there is no file on disk to point
// the entry at until we make one.  It is written beside the program's data so
// that removing the entry can leave it alone: it costs nothing and saves
// redrawing it every time the box is ticked.
QString iconPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + QStringLiteral("/vclock/icon.png");
}

// Exec is a command line, not a path: a program somewhere with a space in its
// name has to be quoted or the rest of it reads as an argument.
QString execField()
{
    const QString path = QCoreApplication::applicationFilePath();
    if (!path.contains(QLatin1Char(' ')))
        return path;
    QString quoted = path;
    quoted.replace(QLatin1String("\\"), QLatin1String("\\\\"));
    quoted.replace(QLatin1String("\""), QLatin1String("\\\""));
    return QLatin1Char('"') + quoted + QLatin1Char('"');
}

bool writeIcon()
{
    const QString path = iconPath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        g_reason = QStringLiteral("could not create %1").arg(QFileInfo(path).absolutePath());
        return false;
    }
    // Big enough for any menu or dock; the entry is scaled down from here.
    if (!appIconImage(256).save(path, "PNG")) {
        g_reason = QStringLiteral("could not write %1").arg(path);
        return false;
    }
    return true;
}

}  // namespace

namespace autostart {

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
        g_reason = QStringLiteral("could not create %1").arg(QFileInfo(path).absolutePath());
        return false;
    }
    if (!writeIcon())
        return false;

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
        << "Icon=" << iconPath() << "\n"
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

QString reason()
{
    return g_reason;
}

}  // namespace autostart
