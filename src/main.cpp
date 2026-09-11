#include "clockmanager.h"
#include "icons.h"
#include "clockwindow.h"
#include "config.h"
#include "autostart.h"
#include "face.h"
#include "jumplist.h"
#include "manageclocksdialog.h"
#include "render.h"
#include "singleinstance.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QPixmap>
#include <QSet>
#include <QStringList>
#include <QTextStream>
#include <QTimer>
#include <QVector>

#include <atomic>
#include <csignal>
#include <memory>
#include <vector>

namespace {

std::atomic_bool g_interrupted{false};

// What a launch asks for, and the first word of the message a later launch
// sends the running one.
const char *kManage = "manage";
const char *kDaemon = "daemon";

extern "C" void onInterrupt(int)
{
    // Only async-signal-safe work here; the timer below does the rest.
    g_interrupted.store(true);
}

}  // namespace

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("vclock"));
    app.setApplicationDisplayName(QStringLiteral("vclock"));
    installGlyphStyle();
    installAccentColour();
    app.setOrganizationName(QStringLiteral("vclock"));
    // Names the launcher this program belongs to, so a pinned icon in a dock
    // and the program it started are treated as the same thing.  The same
    // reasoning as the Windows AppUserModelID, and needed for the same reason:
    // without it the desktop matches windows to launchers by guesswork, and a
    // program whose only windows are frameless clocks gives it nothing to
    // guess from.  Ignored on platforms that have no such notion.
    QGuiApplication::setDesktopFileName(QStringLiteral("vclock"));
    app.setWindowIcon(QIcon(QPixmap::fromImage(appIconImage(256))));
    // The clock closes itself (flushing its config first), and its dialogs must
    // not be able to end the program by being the last window shut.
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("A transparent analog clock for the desktop.\n\n"
                       "Settings live in %1, one file per clock. Right-click the "
                       "clock for its menu.")
            .arg(configDir()));
    // Gives both -h and --help.
    parser.addHelpOption();
    QCommandLineOption configOption(
        {QStringLiteral("c"), QStringLiteral("config")},
        QStringLiteral("Read and write NAME instead of the default config. A bare name is a "
                       "file in the config directory, so \"world\" means world.cfg; anything "
                       "with a / in it is a path of your own. Repeat the option to run "
                       "several clocks at once, one per config."),
        QStringLiteral("name"));
    parser.addOption(configOption);
    QCommandLineOption daemonOption(
        {QStringLiteral("d"), QStringLiteral("daemon")},
        QStringLiteral("Put the clocks up and nothing else. This is what starting at "
                       "login does, where a window asking to be dealt with is the last "
                       "thing wanted."));
    parser.addOption(daemonOption);
    QCommandLineOption manageOption(
        QStringLiteral("manage"),
        QStringLiteral("Put the clocks up and open Manage clocks with them. The default, "
                       "so this is only needed to say so explicitly -- the Windows taskbar "
                       "menu uses it."));
    parser.addOption(manageOption);
    parser.process(app);

    // Saying both asks for two different things. Refused rather than resolved:
    // any order this picked would be as good an argument for the other.
    if (parser.isSet(daemonOption) && parser.isSet(manageOption)) {
        QTextStream(stderr)
            << "vclock: --daemon and --manage ask for different things; "
               "give one or neither\n";
        return 2;
    }
    // Started by hand, the program should show what it can do: the clocks, and
    // the list they are kept in.  Started at login it should show the clocks
    // and get out of the way, which is what --daemon is for, and what the
    // autostart entry the program writes for itself asks for.
    const bool manage = !parser.isSet(daemonOption);

    // Two clocks sharing one file would each save over the other, so a repeated
    // config is taken as having been meant once.
    QVector<QString> paths;
    QSet<QString> seen;
    for (const QString &name : parser.values(configOption)) {
        const QString path = resolveConfigPath(name);
        if (!seen.contains(path)) {
            seen.insert(path);
            paths.push_back(path);
        }
    }

    // A second launch is nearly always someone reaching for a program that is
    // already running -- a pinned taskbar button, or Manage clocks off its
    // menu.  Hand the request over and stop, rather than starting a second set
    // of clocks that would save over the first set's configs.
    //
    // Keyed on the config directory, because two instances reading different
    // configs are two different programs and folding them together would be
    // wrong.
    SingleInstance instance(SingleInstance::keyForConfigDir(configDir()));
    if (!instance.isPrimary()) {
        QStringList request;
        request << QString::fromLatin1(manage ? kManage : kDaemon);
        for (const QString &path : paths)
            request << path;
        // Delivered means done.  If it could not be delivered the instance we
        // found has stopped in the meantime, and starting normally is better
        // than reporting a race the user cannot act on.
        if (instance.send(request))
            return 0;
    }

    // Wired up before the clocks are built rather than after.  The socket
    // starts listening in the constructor above, and building the clocks means
    // rasterising SVGs, which is long enough for a second launch to arrive in
    // the middle of it; a request that turned up with nothing connected would
    // be thrown away.
    QObject::connect(&instance, &SingleInstance::received, &app,
                     [](const QStringList &request) {
        if (request.isEmpty())
            return;
        const QStringList wanted = request.mid(1);
        if (wanted.isEmpty()) {
            // No configs named, so the ask is "put my clocks where I can see
            // them": exactly the ones marked to show, raised.
            ClockManager::instance().openVisible();
        } else {
            ClockManager::instance().openPaths(
                QVector<QString>(wanted.begin(), wanted.end()));
        }
        if (request.first() == QLatin1String(kManage))
            ManageClocksDialog::showDialog(nullptr);
    });

    ClockManager &manager = ClockManager::instance();
    // Naming configs on the command line says exactly which clocks to run;
    // otherwise the ones marked to start in the manage dialog come up.
    if (paths.isEmpty())
        manager.openVisible();
    else
        manager.openPaths(paths);
    if (manage)
        ManageClocksDialog::showDialog(nullptr);

    // The entry that starts the program at login names both the binary and how
    // to start it, and both can go stale -- an upgrade that moved the program,
    // or this version, which wants --daemon on it where the last one had no
    // arguments at all.  Rewritten from what is true now, and only when it
    // differs, so a login start does not begin by opening a window.
    autostart::refresh();
    jumplist::install();

    // Ctrl+C in the launching terminal shuts down the same way the menu does,
    // so the config still gets flushed.  Polling a flag keeps the handler
    // itself async-signal-safe and works on Windows too.
    std::signal(SIGINT, onInterrupt);
#ifdef SIGTERM
    std::signal(SIGTERM, onInterrupt);
#endif
    QTimer interruptPoll;
    interruptPoll.setInterval(200);
    QObject::connect(&interruptPoll, &QTimer::timeout, &app, [&manager] {
        if (!g_interrupted.load())
            return;
        manager.closeAll();
    });
    interruptPoll.start();

    return app.exec();
}
