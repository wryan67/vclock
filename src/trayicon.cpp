#include "trayicon.h"

#include "manageclocksdialog.h"
#include "clockmanager.h"
#include "render.h"

#include <QAction>
#include <QCoreApplication>
#include <QIcon>
#include <QMenu>
#include <QPixmap>
#include <QSystemTrayIcon>
#include <QTimer>

#if defined(VCLOCK_HAVE_XCB)
#include <QByteArray>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QGuiApplication>
#include <QtGui/qguiapplication_platform.h>

#include <xcb/xcb.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#endif

namespace {

QSystemTrayIcon *s_icon = nullptr;
QMenu *s_menu = nullptr;

// The icon, drawn at the sizes a panel is likely to ask for rather than once
// and scaled down.  It is a real clock render, so each size is drawn for
// itself: 16 for the Windows notification area, 22 and 24 for the usual Linux
// panels, larger for the scaled displays that ask for more.
QIcon trayIcon()
{
    QIcon icon;
    for (int size : {16, 22, 24, 32, 48, 64})
        icon.addPixmap(QPixmap::fromImage(appIconImage(size)));
    return icon;
}

void build()
{
    if (s_icon)
        return;

    // Parentless because a menu is a window in its own right and there is no
    // window here to hang it on; deleted by hand on the way out, below.
    s_menu = new QMenu;

    QAction *manage = s_menu->addAction(QStringLiteral("Manage clocks"));
    QObject::connect(manage, &QAction::triggered, s_menu,
                     [] { ManageClocksDialog::showDialog(nullptr); });
    // The default action, which is both a look and a behaviour: a menu draws
    // it in bold, and a plain click on the icon -- no menu -- does it.  Manage
    // clocks is the one worth reaching that fast, because it is the way to
    // every clock, including the ones that are not on screen to be clicked.
    s_menu->setDefaultAction(manage);

    // Next to Manage clocks because that is where it lands: a new clock is
    // made by being named in the list, so the list comes up with the name
    // waiting to be typed.
    QAction *create = s_menu->addAction(QStringLiteral("New clock"));
    QObject::connect(create, &QAction::triggered, s_menu,
                     [] { ManageClocksDialog::newClockIn(nullptr); });

    s_menu->addSeparator();

    // Clearing the screen without stopping the program, which is only a useful
    // thing to offer because the icon stays behind to undo it.
    QAction *hideAll = s_menu->addAction(QStringLiteral("Hide all clocks"));
    QObject::connect(hideAll, &QAction::triggered, s_menu,
                     [] { ClockManager::instance().hideAll(); });

    // The way out, and the reason the hold below is safe to take.
    QAction *quit = s_menu->addAction(QStringLiteral("Quit"));
    QObject::connect(quit, &QAction::triggered, s_menu,
                     [] { ClockManager::instance().quitNow(); });

    s_icon = new QSystemTrayIcon(qApp);
    s_icon->setIcon(trayIcon());
    // Shown on hover.  The program's name on its own: what the icon does is
    // the menu's business, and the clocks have names of their own.
    s_icon->setToolTip(QStringLiteral("vclock"));
    s_icon->setContextMenu(s_menu);

    // A plain click rather than a right-click.  Desktops differ over whether
    // they send this at all -- some open the menu for either button and it
    // never arrives -- so it is the default action's second route to being
    // run, not its only one.
    QObject::connect(s_icon, &QSystemTrayIcon::activated, s_icon,
                     [](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            ManageClocksDialog::showDialog(nullptr);
    });

    // Taken down while this is still a running program.  Left to the
    // destructors it would go during teardown, with the platform it has to
    // speak to already half gone, which is how trays end up holding icons for
    // programs that have stopped.
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, qApp, [] {
        delete s_icon;
        s_icon = nullptr;
        delete s_menu;
        s_menu = nullptr;
    });

    s_icon->show();

    // Held from here on, so that putting the last clock away no longer ends
    // the program.  Until there was an icon that would have been a trap --
    // nothing on screen, nothing in the taskbar, no way to say stop.  With the
    // icon up there is a way back in and a Quit on it, and hiding every clock
    // becomes a thing the user can mean.  Not taken when there is no tray:
    // then the old rule still holds and the last clock still ends it.
    ClockManager::instance().acquireHold();
}

#if defined(VCLOCK_HAVE_XCB)

// The X screen, which is the number after the dot in DISPLAY -- ":0" and
// ":0.0" are both screen 0.  Read from there rather than from Xlib, whose
// headers define None, Status and Bool as macros and would have to be
// untangled from Qt's own names for the sake of one integer.
int screenNumber()
{
    const QByteArray display = qgetenv("DISPLAY");
    const int dot = display.lastIndexOf('.');
    if (dot > display.lastIndexOf(':')) {
        bool ok = false;
        const int screen = display.mid(dot + 1).toInt(&ok);
        if (ok)
            return screen;
    }
    return 0;
}

// Whether anything on this desktop is offering to hold tray icons.
//
// Asked here rather than through QSystemTrayIcon::isSystemTrayAvailable(),
// which cannot be used for it.  Qt answers that question by building the
// machinery it would send an icon through, and then keeps what it built: ask
// once while nothing is listening and the program gets no tray icon for the
// rest of its life, however many panels turn up afterwards.  That is not a
// guess -- asked again later it says yes, the icon then says it is visible,
// and the panel never hears of it.  So the first thing said to Qt about trays
// has to be said at a moment when the answer is already yes.
bool trayHostPresent()
{
    // The modern way: a panel that takes tray icons owns this name on the
    // session bus.  Every desktop that still has a tray does it like this --
    // GNOME through an extension, the others in the shell itself.
    if (QDBusConnection::sessionBus().isConnected()) {
        if (QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface()) {
            if (bus->isServiceRegistered(QStringLiteral("org.kde.StatusNotifierWatcher")))
                return true;
        }
    }

    // The older way, still what the lighter desktops do: a panel owns an X
    // selection named for the screen it is on.
    auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11)
        return false;  // Wayland, which has no such selection to own
    xcb_connection_t *connection = x11->connection();
    if (!connection)
        return false;

    char name[32];
    std::snprintf(name, sizeof(name), "_NET_SYSTEM_TRAY_S%d", screenNumber());
    // only_if_exists, so asking does not create the atom: a name nobody has
    // ever used is answer enough on its own.
    const xcb_intern_atom_cookie_t cookie =
        xcb_intern_atom(connection, 1, static_cast<std::uint16_t>(std::strlen(name)), name);
    xcb_intern_atom_reply_t *atom = xcb_intern_atom_reply(connection, cookie, nullptr);
    if (!atom)
        return false;
    const xcb_atom_t selection = atom->atom;
    std::free(atom);
    if (selection == XCB_ATOM_NONE)
        return false;

    xcb_get_selection_owner_reply_t *owner = xcb_get_selection_owner_reply(
        connection, xcb_get_selection_owner(connection, selection), nullptr);
    if (!owner)
        return false;
    const bool held = owner->owner != XCB_WINDOW_NONE;
    std::free(owner);
    return held;
}

// How long to keep looking.  Starting at login is a race the program can lose:
// the autostart entry runs while the session is still coming up, and the panel
// that would hold the icon may not have said so yet.  A minute covers that and
// then gives up, so a desktop with no tray at all is not asked about all day.
const int kLookMs = 1000;
const int kLooks = 60;
int s_looks = 0;

#endif  // VCLOCK_HAVE_XCB

}  // namespace

namespace tray {

void install()
{
    if (s_icon)
        return;

#if defined(VCLOCK_HAVE_XCB)
    if (!trayHostPresent()) {
        if (++s_looks >= kLooks)
            return;
        QTimer::singleShot(kLookMs, qApp, [] { install(); });
        return;
    }
#endif

    build();
}

}  // namespace tray
