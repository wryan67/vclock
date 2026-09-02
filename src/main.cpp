#include "clockwindow.h"
#include "face.h"

#include <QApplication>
#include <QIcon>
#include <QPixmap>
#include <QTimer>

#include <atomic>
#include <csignal>

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

    ClockWindow clock;
    clock.show();
    clock.restorePosition();

    // Ctrl+C in the launching terminal shuts down the same way the menu does,
    // so the config still gets flushed.  Polling a flag keeps the handler
    // itself async-signal-safe and works on Windows too.
    std::signal(SIGINT, onInterrupt);
#ifdef SIGTERM
    std::signal(SIGTERM, onInterrupt);
#endif
    QTimer interruptPoll;
    interruptPoll.setInterval(200);
    QObject::connect(&interruptPoll, &QTimer::timeout, &clock, [&clock] {
        if (g_interrupted.load())
            clock.close();
    });
    interruptPoll.start();

    return app.exec();
}
