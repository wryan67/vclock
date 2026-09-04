#include "windowgroup.h"

#include <QWidget>

#if defined(VCLOCK_HAVE_XCB)

#include <QGuiApplication>
#include <QtGui/qguiapplication_platform.h>

#include <xcb/xcb.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

// WM_HINTS is nine 32-bit words. The first is a bitmask saying which of the
// rest have been filled in; bit 6 claims the last one, the group leader.
constexpr int kWmHintsWords = 9;
constexpr int kGroupWord = 8;
constexpr std::uint32_t kWindowGroupHint = 1u << 6;

// _NET_WM_STATE client message actions, from the freedesktop spec.
constexpr std::uint32_t kStateRemove = 0;
constexpr std::uint32_t kStateAdd = 1;
// ... and the source indication that marks the request as coming from a
// normal application rather than from a pager.
constexpr std::uint32_t kSourceApplication = 1;

xcb_atom_t lookUp(xcb_connection_t *connection, const char *name)
{
    const xcb_intern_atom_cookie_t cookie =
        xcb_intern_atom(connection, 0, static_cast<std::uint16_t>(std::strlen(name)), name);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection, cookie, nullptr);
    if (!reply)
        return XCB_ATOM_NONE;
    const xcb_atom_t atom = reply->atom;
    std::free(reply);
    return atom;
}

}  // namespace

void windowgroup::detach(QWidget *widget)
{
    if (!widget)
        return;
    // Only X11 has this notion; under Wayland the interface is absent and
    // there is nothing to undo.
    auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11)
        return;
    xcb_connection_t *connection = x11->connection();
    if (!connection)
        return;

    // Asking for the id creates the native window if it does not exist yet,
    // which is what we want: the hints have to be right before the window
    // manager first sees it.
    const auto window = static_cast<xcb_window_t>(widget->winId());

    const xcb_get_property_cookie_t cookie = xcb_get_property(
        connection, 0, window, XCB_ATOM_WM_HINTS, XCB_ATOM_WM_HINTS, 0, kWmHintsWords);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(connection, cookie, nullptr);
    if (!reply)
        return;

    // Read, clear the one flag, write back: the other hints in the record --
    // whether the window takes focus, above all -- have to survive.
    const int words = xcb_get_property_value_length(reply) / 4;
    if (words > 0) {
        std::uint32_t hints[kWmHintsWords] = {0};
        std::memcpy(hints, xcb_get_property_value(reply),
                    static_cast<std::size_t>(std::min(words, kWmHintsWords)) * 4);
        if (hints[0] & kWindowGroupHint) {
            hints[0] &= ~kWindowGroupHint;
            hints[kGroupWord] = 0;
            xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_HINTS,
                                XCB_ATOM_WM_HINTS, 32, kWmHintsWords, hints);
            xcb_flush(connection);
        }
    }
    std::free(reply);
}

bool windowgroup::setAlwaysOnTop(QWidget *widget, bool on)
{
    if (!widget || !widget->testAttribute(Qt::WA_WState_Created))
        return false;
    auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11)
        return false;
    xcb_connection_t *connection = x11->connection();
    if (!connection)
        return false;

    const xcb_atom_t state = lookUp(connection, "_NET_WM_STATE");
    const xcb_atom_t above = lookUp(connection, "_NET_WM_STATE_ABOVE");
    if (state == XCB_ATOM_NONE || above == XCB_ATOM_NONE)
        return false;

    const auto window = static_cast<xcb_window_t>(widget->winId());

    // The request goes to the root of the screen the window is actually on,
    // asked for rather than assumed, so a second X screen would still work.
    xcb_get_geometry_reply_t *geometry =
        xcb_get_geometry_reply(connection, xcb_get_geometry(connection, window), nullptr);
    if (!geometry)
        return false;
    const xcb_window_t root = geometry->root;
    std::free(geometry);

    xcb_client_message_event_t message = {};
    message.response_type = XCB_CLIENT_MESSAGE;
    message.format = 32;
    message.window = window;
    message.type = state;
    message.data.data32[0] = on ? kStateAdd : kStateRemove;
    message.data.data32[1] = above;
    message.data.data32[2] = 0;
    message.data.data32[3] = kSourceApplication;
    message.data.data32[4] = 0;

    xcb_send_event(connection, 0, root,
                   XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                   reinterpret_cast<const char *>(&message));
    xcb_flush(connection);
    return true;
}

#else

void windowgroup::detach(QWidget *) {}

bool windowgroup::setAlwaysOnTop(QWidget *, bool) { return false; }

#endif
