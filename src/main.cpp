#include "clockwindow.h"
#include "config.h"
#include "face.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QIcon>
#include <QPixmap>
#include <QSet>
#include <QTimer>

#include <atomic>
#include <csignal>
#include <memory>
#include <vector>

namespace {

std::atomic_bool g_interrupted{false};

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
    app.setOrganizationName(QStringLiteral("vclock"));
    app.setWindowIcon(QIcon(QPixmap::fromImage(appIconImage(64))));
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
    parser.process(app);

    // Two clocks sharing one file would each save over the other, so a repeated
    // config is taken as having been meant once.
    std::vector<QString> paths;
    QSet<QString> seen;
    for (const QString &name : parser.values(configOption)) {
        const QString path = resolveConfigPath(name);
        if (!seen.contains(path)) {
            seen.insert(path);
            paths.push_back(path);
        }
    }
    if (paths.empty())
        paths.push_back(QString());

    std::vector<std::unique_ptr<ClockWindow>> clocks;
    for (const QString &path : paths) {
        auto clock = std::make_unique<ClockWindow>(path);
        // Placed before the first show(): a window manager applies its own
        // placement policy when a window is mapped, and on X11 that routinely
        // wins over a move() issued afterwards, leaving the clock wherever the
        // WM felt like putting it. Positioning it while it is still unmapped
        // makes the request part of the initial geometry, which is honoured.
        clock->restorePosition();
        clock->show();
        // The clock is a frameless tool window that deliberately stays out of
        // the taskbar and the window switcher, so a window manager that maps it
        // below the windows already on screen leaves the user with no way to
        // find it and nothing apparently happening. Asking for the front
        // explicitly makes it visible on launch without forcing "always on top"
        // on for good.
        clock->raise();
        clock->activateWindow();
        clocks.push_back(std::move(clock));
    }

    // Ctrl+C in the launching terminal shuts down the same way the menu does,
    // so the config still gets flushed.  Polling a flag keeps the handler
    // itself async-signal-safe and works on Windows too.
    std::signal(SIGINT, onInterrupt);
#ifdef SIGTERM
    std::signal(SIGTERM, onInterrupt);
#endif
    QTimer interruptPoll;
    interruptPoll.setInterval(200);
    QObject::connect(&interruptPoll, &QTimer::timeout, &app, [&clocks] {
        if (!g_interrupted.load())
            return;
        for (const auto &clock : clocks)
            clock->close();
    });
    interruptPoll.start();

    return app.exec();
}
