#include "clockwindow.h"

#include "clockmanager.h"
#include "embedded.h"
#include "face.h"
#include "manageclocksdialog.h"
#include "render.h"
#include "settingsdialog.h"
#include "timetip.h"
#include "windowgroup.h"

#include <QAction>
#include <QApplication>
#include <QBitmap>
#include <QCloseEvent>
#include <QCursor>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QIcon>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QScreen>
#include <QSignalBlocker>
#include <QStringList>
#include <QTime>
#include <QTimer>
#include <QUrl>
#include <QWheelEvent>
#include <QWindow>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

// How often the hands are re-examined. Stepping, this only has to be short
// enough that a new second shows up promptly; sweeping, it is the frame rate.
constexpr int kSteppedIntervalMs = 200;
constexpr int kSmoothIntervalMs = 17;  // ~60 fps

// How long the pointer has to settle before the date bubble appears is a
// program-wide setting, kept beside the clock list; see registry.h.

#if defined(Q_OS_MACOS)
// macOS users expect Cmd; Qt already maps Qt::ControlModifier onto it.
const char *kCmdLabel = "Cmd";
// The alternates listed in help alongside the primary Cmd+H, for taking a
// single clock off screen.
const char *kHideKeys = "Cmd+W, Esc";
#else
const char *kCmdLabel = "Ctrl";
const char *kHideKeys = "Alt+F4, Esc";
#endif

// The accelerator shown down the right-hand side of a menu entry. Qt renders
// whatever follows a tab in an action's text as the shortcut column, which
// keeps these labels purely cosmetic -- keyPressEvent below stays the one place
// the keys are actually acted on, so the two can never disagree about what a
// key does.
QString menuHotkey(const QString &text, const char *keys)
{
    return text + QLatin1Char('\t') + QLatin1String(kCmdLabel) + QLatin1Char('+')
           + QLatin1String(keys);
}

QPoint globalPosOf(const QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition().toPoint();
#else
    return event->globalPos();
#endif
}

QPointF localPosOf(const QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position();
#else
    return event->localPos();
#endif
}

// A monitor's identity, from its EDID where the platform exposes one.
//
// The connector name ("HDMI-0", "DP-2") is deliberately the last resort: it
// describes the port rather than the panel, so plugging the same monitor into a
// different socket would otherwise look like a brand new screen and lose its
// remembered placement.
QString displayIdentity(const QScreen *screen)
{
    QStringList parts;
    for (const QString &part : {screen->manufacturer(), screen->model(),
                                screen->serialNumber()}) {
        if (!part.trimmed().isEmpty())
            parts << part.trimmed();
    }
    QString id = parts.join(QLatin1Char(' ')).simplified();
    if (id.isEmpty())
        id = screen->name().trimmed();
    if (id.isEmpty())
        id = QStringLiteral("display");
    return id;
}

QString displayKey(const QScreen *screen)
{
    if (!screen)
        return QString();

    const QString id = displayIdentity(screen);

    // Two monitors of the same make and model can report the same EDID string
    // (and some report no serial at all), which would make them share a single
    // record. Where that happens the connector name tells them apart; it is
    // only appended for the ambiguous case, so ordinary single-panel setups
    // keep a key that survives being replugged.
    int matches = 0;
    for (const QScreen *other : QGuiApplication::screens()) {
        if (displayIdentity(other) == id)
            ++matches;
    }
    if (matches > 1 && !screen->name().trimmed().isEmpty())
        return id + QStringLiteral(" @") + screen->name().trimmed();
    return id;
}

}  // namespace

ClockWindow::ClockWindow(const QString &configPath)
    : QWidget(nullptr), m_configPath(configPath.isEmpty() ? ::configPath() : configPath)
{
    m_cfg = loadConfig(m_configPath);

    // Several clocks can be running at once, so the title says which one this
    // is.
    refreshTitle();
    setWindowIcon(QIcon(QPixmap::fromImage(appIconImage(256))));

    // Undecorated, kept out of the taskbar and the window switcher, and painted
    // straight onto a translucent surface so everything outside the artwork is
    // genuinely transparent.
    Qt::WindowFlags flags = Qt::FramelessWindowHint;
#if !defined(Q_OS_MACOS)
    // A tool window is what keeps the widget off the taskbar and pager; on
    // macOS it would also hide whenever the app loses focus, so it is skipped.
    flags |= Qt::Tool;
#else
    flags |= Qt::Window;
#endif
    setWindowFlags(flags);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_AlwaysShowToolTips, true);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(false);
    applyAlwaysOnTop();
    // Each clock stacks on its own, so "always on top" on one of them does not
    // drag the rest up with it.
    detachFromGroup();

    m_tipDelay = new QTimer(this);
    m_tipDelay->setSingleShot(true);
    connect(m_tipDelay, &QTimer::timeout, this, &ClockWindow::showTimeTip);

    m_face = openFace(m_cfg.facePath(), m_cfg.generatorFaceColor(), m_cfg.generatorWireColor(),
                      m_cfg.faceMultiHue());
    if (m_cfg.size <= 0)
        m_cfg.size = defaultSizeOn(startupScreen());
    m_cfg.size = std::min(m_cfg.size, maxSize());
    applySize();
    rebuildRaster();

    buildMenu();

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(500);
    connect(m_saveTimer, &QTimer::timeout, this, [this] { saveConfig(m_cfg, m_configPath); });

    m_rebuildTimer = new QTimer(this);
    m_rebuildTimer->setSingleShot(true);
    m_rebuildTimer->setInterval(60);
    connect(m_rebuildTimer, &QTimer::timeout, this, [this] {
        rebuildRaster();
        update();
    });

    // Polled rather than fired once a second, so the hands land on the new
    // second promptly however far the timer has drifted. In smooth mode, or
    // whenever the face is turning, it runs at 60 fps and every tick repaints,
    // since something on the clock is always moving.
    m_tick = new QTimer(this);
    connect(m_tick, &QTimer::timeout, this, [this] {
        if (m_cfg.smoothSweep || spinning()) {
            update();
            return;
        }
        const int second = QTime::currentTime().second();
        if (second != m_lastSecond) {
            m_lastSecond = second;
            update();
        }
    });
    applyTickRate();
    m_tick->start();

    // A generated face can be asked for again at no cost, so it can be left to
    // change itself on a timer.  Starting with a fresh one matters as much as
    // the timer does: without it a clock that is started and stopped inside the
    // interval shows the same face for ever, which is the opposite of what
    // asking for a new one every few minutes was for.
    m_regenTimer = new QTimer(this);
    connect(m_regenTimer, &QTimer::timeout, this, [this] { regenerateFace(); });
    if (m_cfg.faceRegen)
        regenerateFace();
    syncRegenTimer();

    connect(qApp, &QGuiApplication::screenRemoved, this,
            [this](QScreen *) { handleScreenRemoved(); });
}

ClockWindow::~ClockWindow() = default;

// ------------------------------------------------------------------- config

QSize ClockWindow::pixelSize() const
{
    const int w = std::max(1, m_cfg.size);
    const int h = std::max(1, static_cast<int>(std::lround(w * m_face->aspect())));
    return QSize(w, h);
}

void ClockWindow::applySize()
{
    const QSize size = pixelSize();
    setFixedSize(size);
}

// Re-rasterise the face at the current widget size and apply colours.
// Everything the clock draws over the face turns about the pivot; this is how
// far out any of it reaches.
double ClockWindow::reachRadiusRaw() const
{
    const QSize size = pixelSize();
    const QPointF center = centerPixels();
    const double radius = handRadiusRaw();
    return std::max(radius * kSecondLen,
                    markReach(m_cfg, center.x(), center.y(), radius, size.width(),
                              size.height()));
}

double ClockWindow::reachRadius() const
{
    return reachRadiusRaw() * drawScale();
}

// How far the artwork reaches from the pivot.  A turning face sweeps every
// point of itself all the way round, so the shape it can occupy is a disc of
// this radius -- which is what the hit mask has to cover while it is spinning,
// since working the real outline out afresh sixty times a second is far too
// dear.  The reach is measured once, when the face is rasterised, and kept as
// a fraction of the width so that it survives a resize the raster has not yet
// caught up with.
double ClockWindow::spinRadius() const
{
    return m_spinReach * pixelSize().width() * drawScale();
}

// Where the drawing is centred.  Standing still that is the pivot the user
// placed, which is the whole point of being able to place it.  Turning, it is
// the middle of the window: what a spinning clock occupies is a disc about the
// pivot, and the biggest disc a window will hold is the one in the middle of
// it.  A pivot set low, as a face whose hands sit below its middle needs,
// would otherwise throw most of that disc out through the bottom edge.
QPointF ClockWindow::drawCenter() const
{
    if (!spinning())
        return centerPixels();
    const QSize size = pixelSize();
    return QPointF(size.width() / 2.0, size.height() / 2.0);
}

// How much the drawing is shrunk to fit.  The window cannot grow to hold the
// turn: the face is rendered to fill it, so a bigger window means a bigger
// face and the overhang comes back exactly as it was.  Drawing the clock
// smaller within the window it already has is the only thing that works, and
// it leaves the footprint, the placement and the size setting all untouched.
//
// The hands and the marks shrink with the face rather than staying put, since
// the alternative is a full-sized set of hands over a shrunken dial -- the
// clock is drawn smaller, not taken apart.
double ClockWindow::drawScale() const
{
    if (!spinning())
        return 1.0;
    const QSize size = pixelSize();
    // A pixel short of the true half-width, so the outermost of the artwork has
    // somewhere to fade out into.  Fitting exactly would put the antialiased
    // edge on the boundary itself, where half of it is outside the window.
    const double room = std::min(size.width(), size.height()) / 2.0 - 1.0;
    const double want = std::max(m_spinReach * size.width(), reachRadiusRaw());
    if (want <= room || want <= 0.0 || room <= 0.0)
        return 1.0;
    return room / want;
}

// Only the clock takes clicks.  The window has to be a rectangle and a clock is
// not, so without this its empty corners would swallow clicks meant for the
// window or the desktop behind them -- something that is not there to look at
// should not be there to hit either.
//
// The shape comes from the artwork's own coverage rather than from what is on
// screen at the time, so that fading a clock down does not gradually make it
// unclickable: opacity is about what you can see, not what you can reach.
void ClockWindow::applyHitMask()
{
    const QSize widget = size();
    if (m_coverage.isNull() || widget.isEmpty()) {
        clearMask();
        return;
    }

    QImage stencil(widget, QImage::Format_ARGB32_Premultiplied);
    stencil.fill(Qt::transparent);
    {
        QPainter painter(&stencil);
        // A turning face passes through every angle, so what it occupies is a
        // disc about the point it is drawn around, and nothing else.  The
        // artwork's own outline is no use here twice over: it is not where the
        // face is any more, and it would flicker in and out of the mask as the
        // drawing swung past it.
        if (!spinning()) {
            // Laid down nine times, shifted a pixel each way, so the mask ends
            // up a pixel proud of the artwork all round.  A mask even slightly
            // inside it would shave the antialiased edge off the clock.
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy)
                    painter.drawImage(QRect(QPoint(dx, dy), widget), m_coverage);
            }
        }
        // The hands and the indices go on top of the face, and an off-centre
        // pivot or a high index position can carry them past its edge, so the
        // circle they move in is covered whether the artwork fills it or not.
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::white);
        const double r = std::max(reachRadius(), spinning() ? spinRadius() : 0.0) + 1.0;
        painter.drawEllipse(drawCenter(), r, r);
    }

    // Any alpha at all counts.  The artwork fades to nothing at its edge, so a
    // boundary drawn where the alpha runs out falls outside every visible
    // pixel -- which is what lets a hard-edged mask clip an antialiased clock
    // without showing a single jagged step.
    for (int y = 0; y < stencil.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(stencil.scanLine(y));
        for (int x = 0; x < stencil.width(); ++x)
            line[x] = qAlpha(line[x]) > 0 ? 0xffffffffu : 0u;
    }

    fillEnclosedGaps(stencil);
    setMask(QBitmap::fromImage(stencil.createAlphaMask()));

    // The same shape again, painted in an alpha the eye cannot see.  See
    // paintEvent: this is what makes the clock's own gaps clickable on Windows.
    m_hitFill = QImage(widget, QImage::Format_ARGB32_Premultiplied);
    m_hitFill.fill(Qt::transparent);
    for (int y = 0; y < widget.height(); ++y) {
        const auto *src = reinterpret_cast<const QRgb *>(stencil.constScanLine(y));
        auto *dst = reinterpret_cast<QRgb *>(m_hitFill.scanLine(y));
        for (int x = 0; x < widget.width(); ++x)
            dst[x] = qAlpha(src[x]) > 0 ? 0x01000000u : 0u;
    }
}

// Close up the holes in a hit shape: every gap the clock encloses becomes part
// of it, and only the space around the outside stays outside.
//
// A clock is a ring of artwork more often than it is a disc.  Wound down to
// nothing the face is transparent between its markings, a spiral is gaps as
// much as arm, and any face at all is see-through where the artist left it so.
// None of those gaps are holes in the clock as far as anyone using it is
// concerned -- they are part of its front, and pressing one should take hold of
// the clock and not of whatever is behind it.
//
// So rather than ask which pixels the artwork covers, this asks which pixels
// the outside can reach: it floods inwards from the edges of the window through
// transparent pixels, and everything it never arrives at was enclosed by the
// artwork and is filled in.  The boundary that leaves is the outermost line
// where transparent meets drawn, which is exactly where a clock looks like it
// ends.
void ClockWindow::fillEnclosedGaps(QImage &stencil)
{
    const int w = stencil.width();
    const int h = stencil.height();
    if (w <= 0 || h <= 0)
        return;

    // Flooded from every border pixel at once rather than from one corner: a
    // clock is not always drawn in the middle of its window and can run off any
    // side, which would strand the outside in several disconnected pieces.
    std::vector<uint8_t> outside(static_cast<size_t>(w) * h, 0);
    std::vector<int> queue;
    queue.reserve(static_cast<size_t>(w) * h / 4);

    const auto consider = [&](int x, int y) {
        const size_t i = static_cast<size_t>(y) * w + x;
        if (outside[i])
            return;
        const auto *line = reinterpret_cast<const QRgb *>(stencil.constScanLine(y));
        if (qAlpha(line[x]) > 0)
            return;
        outside[i] = 1;
        queue.push_back(static_cast<int>(i));
    };

    for (int x = 0; x < w; ++x) {
        consider(x, 0);
        consider(x, h - 1);
    }
    for (int y = 0; y < h; ++y) {
        consider(0, y);
        consider(w - 1, y);
    }

    // Four-connected, so a gap that only escapes between two diagonally
    // touching pixels counts as enclosed.  The clock is drawn antialiased and
    // its edges are solid well before that; a leak that thin is a rounding
    // artefact rather than a way out.
    for (size_t head = 0; head < queue.size(); ++head) {
        const int i = queue[head];
        const int x = i % w;
        const int y = i / w;
        if (x > 0)
            consider(x - 1, y);
        if (x + 1 < w)
            consider(x + 1, y);
        if (y > 0)
            consider(x, y - 1);
        if (y + 1 < h)
            consider(x, y + 1);
    }

    for (int y = 0; y < h; ++y) {
        auto *line = reinterpret_cast<QRgb *>(stencil.scanLine(y));
        for (int x = 0; x < w; ++x) {
            if (!outside[static_cast<size_t>(y) * w + x])
                line[x] = 0xffffffffu;
        }
    }
}

void ClockWindow::rebuildRaster()
{
    const QSize size = pixelSize();
    const qreal dpr = devicePixelRatioF() > 0 ? devicePixelRatioF() : 1.0;
    const int pw = std::max(1, static_cast<int>(std::lround(size.width() * dpr)));
    const int ph = std::max(1, static_cast<int>(std::lround(size.height() * dpr)));

    const QImage art = m_face->render(pw, ph);
    const QRect bounds = contentBounds(art);
    // Stored as fractions so the hands stay correct even while the raster is
    // briefly stale during a resize drag.
    m_bounds = QRectF(static_cast<double>(bounds.left()) / pw,
                      static_cast<double>(bounds.top()) / ph,
                      static_cast<double>(bounds.width()) / pw,
                      static_cast<double>(bounds.height()) / ph);
    m_spinReach = farthestCovered(art, QPointF(m_cfg.centerFraction().x() * pw,
                                               m_cfg.centerFraction().y() * ph))
                  / pw;

    // In "original" mode the artwork is its own colour scheme; leave it be.
    m_raster = m_cfg.faceRecolor
                   ? recolor(art, m_cfg.wireColor, m_cfg.faceColor, m_cfg.faceOpacity,
                             m_cfg.wireOpacity)
                   : art;
    m_raster.setDevicePixelRatio(dpr);

    // Kept from the artwork as rendered, before any of the user's opacity is
    // applied: a face faded to nothing is still a clock, and must still be
    // possible to click on.
    m_coverage = art.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                     .convertToFormat(QImage::Format_ARGB32);
    applyHitMask();
}

// Re-raster after the user pauses, coalescing a burst of slider events.
//
// Dragging the size slider fires valueChanged on every pointer motion; a full
// re-raster each time cannot keep up at large sizes, so until things settle the
// existing raster is scaled instead (see paintEvent).
void ClockWindow::scheduleRebuild()
{
    m_rebuildTimer->start();
}

// Ask the face for a different one, the way clicking the Kaleidoscope preset
// again does.  Nothing happens while the settings dialog is open: the dialog
// holds a copy of these values to preview from, and a face changed behind its
// back would be undone by the next control the user touched.
void ClockWindow::regenerateFace()
{
    if (m_settings)
        return;
    Config next = m_cfg;
    rerollFace(next);
    applySettings(next);
    queueSave();
}

// The interval is only read when the timer is started, so it has to be
// restarted whenever the setting moves.
void ClockWindow::syncRegenTimer()
{
    if (!m_regenTimer)
        return;
    if (!m_cfg.faceRegen) {
        m_regenTimer->stop();
        return;
    }
    const int interval = qBound(kRegenMinutesMin, m_cfg.faceRegenMinutes, kRegenMinutesMax)
                         * 60 * 1000;
    if (!m_regenTimer->isActive() || m_regenTimer->interval() != interval) {
        m_regenTimer->setInterval(interval);
        m_regenTimer->start();
    }
}

void ClockWindow::queueSave()
{
    m_saveTimer->start();
}

void ClockWindow::flushSave()
{
    m_saveTimer->stop();
    m_rebuildTimer->stop();
    saveConfig(m_cfg, m_configPath);
}

void ClockWindow::setConfigFilePath(const QString &path)
{
    if (path.isEmpty() || path == m_configPath)
        return;
    m_configPath = path;
    refreshTitle();
}

int ClockWindow::maxSize() const
{
    const QScreen *screen = QGuiApplication::screenAt(frameGeometry().center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return kSizeMaxFallback;
    return std::max(kSizeMin, screen->geometry().height());
}

QPointF ClockWindow::centerPixels() const
{
    const QSize size = pixelSize();
    const QPointF fraction = m_cfg.centerFraction();
    return QPointF(fraction.x() * size.width(), fraction.y() * size.height());
}

// Largest radius that keeps the hands inside the artwork's content.
double ClockWindow::handRadiusRaw() const
{
    const QSize size = pixelSize();
    const QPointF center = centerPixels();
    const double x0 = m_bounds.left() * size.width();
    const double y0 = m_bounds.top() * size.height();
    const double x1 = m_bounds.right() * size.width();
    const double y1 = m_bounds.bottom() * size.height();
    double reach = std::min({center.x() - x0, x1 - center.x(), center.y() - y0,
                             y1 - center.y()});
    if (reach <= 0.0) {  // centre sits outside the artwork; use what room there is
        reach = std::min({center.x(), size.width() - center.x(), center.y(),
                          size.height() - center.y()});
    }
    return std::max(1.0, reach * kHandSpan);
}

double ClockWindow::handRadius() const
{
    return handRadiusRaw() * drawScale();
}

QString ClockWindow::faceLabel() const
{
    return m_face->label();
}

// The monitor the clock is on right now.
QScreen *ClockWindow::currentScreen() const
{
    // Once mapped, the screen under the window's centre is the one the user
    // would say it is on, even when it straddles a boundary.
    if (windowHandle()) {
        if (QScreen *screen = QGuiApplication::screenAt(frameGeometry().center()))
            return screen;
        if (QScreen *screen = windowHandle()->screen())
            return screen;
    }
    if (QScreen *screen = QGuiApplication::screenAt(QCursor::pos()))
        return screen;
    return QGuiApplication::primaryScreen();
}

// The monitor to open on: the one the clock was last used on if it is still
// attached, otherwise wherever the pointer is.
QScreen *ClockWindow::startupScreen() const
{
    if (!m_cfg.lastDisplay.isEmpty()) {
        for (QScreen *screen : QGuiApplication::screens()) {
            if (displayKey(screen) == m_cfg.lastDisplay)
                return screen;
        }
    }
    if (QScreen *screen = QGuiApplication::screenAt(QCursor::pos()))
        return screen;
    return QGuiApplication::primaryScreen();
}

int ClockWindow::maxSizeFor(const QScreen *screen) const
{
    if (!screen)
        return kSizeMaxFallback;
    return std::max(kSizeMin, screen->geometry().height());
}

// The size a clock gets when its config has never recorded one -- a new clock,
// or one whose settings have just been reset.
int ClockWindow::defaultSizeOn(const QScreen *screen) const
{
    if (!screen)
        return kSizeDefaultFallback;
    const int wanted = qRound(screen->geometry().height() * kSizeDefaultFraction);
    return std::clamp(wanted, kSizeMin, maxSizeFor(screen));
}

// Where a clock with no history goes on a given monitor: the top left corner of
// its working area, clear of the panels, where it is out of the way of whatever
// is already open and in the same place every time.
QPoint ClockWindow::defaultPositionOn(const QScreen *screen) const
{
    return screen->availableGeometry().topLeft();
}

// Keep the whole clock inside the monitor's working area, so a remembered
// position taken from a larger screen cannot strand it out of reach.
QPoint ClockWindow::clampToScreen(const QPoint &topLeft, const QScreen *screen) const
{
    const QRect available = screen->availableGeometry();
    int x = topLeft.x();
    int y = topLeft.y();

    if (available.width() > width())
        x = qBound(available.left(), x, available.right() - width() + 1);
    else
        x = available.left();

    if (available.height() > height())
        y = qBound(available.top(), y, available.bottom() - height() + 1);
    else
        y = available.top();

    return QPoint(x, y);
}

void ClockWindow::placeOnScreen(QScreen *screen)
{
    if (!screen)
        return;

    const auto saved = m_cfg.displays.constFind(displayKey(screen));
    if (saved != m_cfg.displays.constEnd()) {
        if (saved->size > 0) {
            const int size = std::min(saved->size, maxSizeFor(screen));
            if (size != m_cfg.size) {
                m_cfg.size = size;
                applySize();
                rebuildRaster();
            }
        }
        // Stored relative to the working area, so the clock lands on the same
        // part of this panel however the monitors are arranged today.
        const QPoint want = screen->availableGeometry().topLeft()
                            + QPoint(saved->x, saved->y);
        move(clampToScreen(want, screen));
    } else {
        const int size = std::min(m_cfg.size, maxSizeFor(screen));
        if (size != m_cfg.size) {
            m_cfg.size = size;
            applySize();
            rebuildRaster();
        }
        move(defaultPositionOn(screen));
    }
    update();
}

// Record where the clock is, and how big, against the monitor it is on.
void ClockWindow::rememberPlacement()
{
    QScreen *screen = currentScreen();
    if (!screen)
        return;
    const QString key = displayKey(screen);
    if (key.isEmpty())
        return;

    const QRect available = screen->availableGeometry();
    DisplayState state;
    state.x = pos().x() - available.x();
    state.y = pos().y() - available.y();
    state.size = m_cfg.size;

    const auto existing = m_cfg.displays.constFind(key);
    if (existing != m_cfg.displays.constEnd() && existing->x == state.x
        && existing->y == state.y && existing->size == state.size
        && m_cfg.lastDisplay == key) {
        return;
    }

    m_cfg.displays.insert(key, state);
    m_cfg.lastDisplay = key;
    queueSave();
}

// A monitor being unplugged can leave the clock on coordinates that no longer
// exist, which -- for a frameless window with no taskbar entry -- is
// indistinguishable from the program having quit. Bring it back onto a screen
// that is still attached.
void ClockWindow::handleScreenRemoved()
{
    if (!isVisible())
        return;
    // Deferred: the window manager does its own reshuffling when a screen goes
    // away, and moving the window before that settles just fights it.
    QTimer::singleShot(0, this, [this] {
        if (!isVisible())
            return;
        if (QGuiApplication::screenAt(frameGeometry().center()))
            return;  // still somewhere valid
        if (QScreen *screen = QGuiApplication::primaryScreen())
            placeOnScreen(screen);
    });
}

// Place the window for this session.
//
// With a position remembered for this monitor the clock goes back exactly where
// it was. Without one the window manager would otherwise be left to place it,
// which on a busy desktop tends to mean the top-left corner underneath whatever
// is already open -- and since this is a frameless tool window that skips the
// taskbar and the pager, a clock parked behind another window is effectively
// invisible and unreachable. Centring it on the target screen keeps a first run
// on any monitor visible.
void ClockWindow::restorePosition()
{
    QScreen *screen = startupScreen();
    if (!screen)
        return;

    // A config written before per-display records existed keeps its absolute
    // position, as long as that still lands on a monitor that is attached. The
    // move below is picked up by moveEvent, which migrates it to a per-display
    // record straight away.
    if (!m_cfg.displays.contains(displayKey(screen)) && m_cfg.x.has_value()
        && m_cfg.y.has_value()) {
        const QRect want(QPoint(*m_cfg.x, *m_cfg.y), size());
        if (QGuiApplication::screenAt(want.center())) {
            move(want.topLeft());
            return;
        }
    }

    placeOnScreen(screen);
}

// ------------------------------------------------------------------- events

void ClockWindow::moveEvent(QMoveEvent *event)
{
    QWidget::moveEvent(event);
    const QPoint pos = this->pos();
    if (m_cfg.x != std::optional<int>(pos.x()) || m_cfg.y != std::optional<int>(pos.y())) {
        m_cfg.x = pos.x();
        m_cfg.y = pos.y();
        queueSave();
    }
    // Dragging the clock onto another monitor records it against that monitor.
    // Its remembered size is deliberately not applied here: resizing the window
    // out from under a drag in progress would be startling, so a per-monitor
    // size only takes effect when the clock opens there.
    rememberPlacement();
}

// Refuse to be minimised, maximised or made full screen.
//
// The window has no title bar to offer those actions, but a window manager can
// still impose them from outside -- "show desktop", a minimise-all shortcut, or
// a tiling keybinding will happily iconify a utility window. Since the clock
// keeps out of the taskbar and the window switcher, being iconified would leave
// no way at all to get it back, and being maximised would stretch a fixed-size
// circular face across the screen.
void ClockWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange && !m_restoringState) {
        const Qt::WindowStates unwanted =
            Qt::WindowMinimized | Qt::WindowMaximized | Qt::WindowFullScreen;
        if (windowState() & unwanted) {
            m_restoringState = true;
            // Deferred: undoing the state while the window manager is still
            // acting on it just gets overwritten.
            QTimer::singleShot(0, this, [this] {
                setWindowState(windowState() & ~(Qt::WindowMinimized | Qt::WindowMaximized
                                                 | Qt::WindowFullScreen));
                if (!isVisible())
                    show();
                raise();
                m_restoringState = false;
            });
        }
    }
    QWidget::changeEvent(event);
}

// --- the date bubble ------------------------------------------------------
//
// The pointer arriving is only a hint that the bubble might be wanted; it has
// to stay put for a moment first, so that sweeping the mouse across the desk
// does not leave a trail of them.
void ClockWindow::enterEvent(QEnterEvent *event)
{
    armTimeTip();
    QWidget::enterEvent(event);
}

void ClockWindow::leaveEvent(QEvent *event)
{
    hideTimeTip();
    QWidget::leaveEvent(event);
}

void ClockWindow::hideEvent(QHideEvent *event)
{
    // A bubble left behind by a clock that is no longer there would have
    // nothing to point at.
    hideTimeTip();
    // A window that is going away cannot be left holding the pointer.
    stopZoom();
    QWidget::hideEvent(event);
}

void ClockWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // Being shown may have built a fresh native window, and Qt puts the group
    // hint back every time it does.
    detachFromGroup();
    // Qt sets the window's state from its own flag as it maps it, and that
    // flag stops being the truth the moment the setting is changed on a clock
    // that is already up. Say it again once the mapping has gone through.
    QTimer::singleShot(0, this, [this] { windowgroup::setAlwaysOnTop(this, m_cfg.alwaysOnTop); });
}

// Qt does not rebuild the native window while it is being asked to: the old
// one is dropped and the replacement appears once the event loop comes round
// again, with the group hint freshly set on it. Clearing it now covers the
// window that is there already, and clearing it again on the next turn covers
// the one that is about to be.
void ClockWindow::detachFromGroup()
{
    windowgroup::detach(this);
    QTimer::singleShot(0, this, [this] { windowgroup::detach(this); });
}

void ClockWindow::armTimeTip()
{
    // Placing the hands or carrying the clock on the pointer are both jobs
    // where a bubble under the cursor would be in the way.
    if (m_picking || m_moveMode || m_dragging)
        return;
    // Read the wait each time rather than holding on to it, so that changing
    // it in the manage dialog takes effect on the next hover instead of the
    // next run.  Zero means the bubble is switched off.
    const int delay = ClockManager::instance().registry().hoverDelayMs;
    if (delay <= 0)
        return;
    m_tipDelay->start(delay);
}

void ClockWindow::showTimeTip()
{
    if (m_picking || m_moveMode || m_dragging || !isVisible())
        return;
    // The pointer may have moved on since the delay started, and the bubble
    // belongs where it is now rather than where it came in.
    if (!rect().contains(mapFromGlobal(QCursor::pos())))
        return;
    if (!m_timeTip)
        m_timeTip = new TimeTip(this);
    m_timeTip->popUp(QCursor::pos());
}

void ClockWindow::hideTimeTip()
{
    if (m_tipDelay)
        m_tipDelay->stop();
    if (m_timeTip)
        m_timeTip->hide();
}

void ClockWindow::hideClock()
{
    m_hiding = true;
    close();
}

void ClockWindow::closeEvent(QCloseEvent *event)
{
    hideTimeTip();
    stopZoom();
    closeSettings();
    flushSave();
    event->accept();
    // The manager decides whether this was the last clock; it may be holding
    // the program open for a dialog of its own.
    emit closed(m_hiding);
}

void ClockWindow::refreshTitle()
{
    // Qt appends the application display name to a window title that differs
    // from it, so the name alone reads as "Kitchen — vclock". The default
    // clock under its default name stays plain "vclock", as it always was.
    const QString name = ClockManager::instance().nameFor(m_configPath);
    const bool plain = name.isEmpty()
                       || (m_configPath == ::configPath() && name == QLatin1String("Default"));
    setWindowTitle(plain ? QStringLiteral("vclock") : name);
    if (m_settings)
        m_settings->refreshTitle();
}

QString ClockWindow::configName() const
{
    return configLabel(m_configPath);
}

void ClockWindow::mousePressEvent(QMouseEvent *event)
{
    hideTimeTip();
    // A click ends a spin of the wheel.  The pointer is held while one is
    // going on, so the press may well be somewhere else entirely: one that
    // landed outside the clock was meant for whatever is there, and is
    // swallowed rather than starting a drag or opening the menu here.
    if (m_zooming) {
        const bool inside = rect().contains(localPosOf(event).toPoint());
        stopZoom();
        if (!inside) {
            event->accept();
            return;
        }
    }
    // Any button settles a move in progress, and is swallowed so it cannot also
    // start a drag or open the menu.
    if (m_moveMode) {
        stopMoveMode();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        if (m_picking) {
            // Click and drag: the preview follows the pointer, nothing is
            // committed until the button comes back up.
            m_draggingCenter = true;
            previewCenter(localPosOf(event));
            event->accept();
            return;
        }
        // Arm a drag rather than starting one. Handing the window straight to
        // the window manager would swallow the rest of the click pair, so the
        // move waits until the pointer has really travelled.
        m_dragOffset = globalPosOf(event) - frameGeometry().topLeft();
        m_pressPos = globalPosOf(event);
        m_dragArmed = true;
        m_dragging = false;
        event->accept();
        return;
    }
    if (event->button() == Qt::RightButton) {
        m_menu->popup(globalPosOf(event));
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ClockWindow::mouseMoveEvent(QMouseEvent *event)
{
    // The pointer does not move while a clock resizes under it -- the clock is
    // what moves -- so travelling away from where the spin started is the user
    // heading somewhere else, and the grab has served its purpose.  Letting go
    // on the way means the click they are going to make arrives normally.
    if (m_zooming) {
        if ((globalPosOf(event) - m_zoomAnchor).manhattanLength()
            > QApplication::startDragDistance() * 2)
            stopZoom();
        event->accept();
        return;
    }
    if (m_moveMode) {
        centerOnCursor();
        event->accept();
        return;
    }
    if (m_draggingCenter) {
        previewCenter(localPosOf(event));
        event->accept();
        return;
    }
    if ((m_dragArmed || m_dragging) && (event->buttons() & Qt::LeftButton)) {
        if (m_dragArmed) {
            // Ignore the jitter of a click that was meant to stay put.
            if ((globalPosOf(event) - m_pressPos).manhattanLength()
                < QApplication::startDragDistance()) {
                event->accept();
                return;
            }
            m_dragArmed = false;
            // The window manager moves the window itself where it can, which is
            // the only thing that works on Wayland; elsewhere the fallback
            // tracks the pointer by hand. Either way the window trails the
            // pointer by the threshold distance for the rest of the drag, which
            // is the ordinary feel of a drag threshold and far too small to see.
            m_dragging = true;
            if (QWindow *handle = windowHandle()) {
                if (handle->startSystemMove())
                    m_dragging = false;
            }
        }
        if (m_dragging)
            move(globalPosOf(event) - m_dragOffset);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void ClockWindow::mouseReleaseEvent(QMouseEvent *event)
{
    // The release that follows the settling click has nothing left to do.
    if (m_moveMode) {
        event->accept();
        return;
    }
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    if (m_draggingCenter) {
        m_draggingCenter = false;
        previewCenter(localPosOf(event));
        commitPick();
        event->accept();
        return;
    }
    m_dragging = false;
    m_dragArmed = false;
    event->accept();
}

void ClockWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    // A double click anywhere on the face opens Settings. The first click of the
    // pair has already been treated as the start of a drag, which is harmless:
    // the window has not moved unless the pointer did.
    if (m_moveMode || m_picking || event->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    m_dragging = false;
    m_dragArmed = false;
    openSettings();
    event->accept();
}

// The wheel over the clock resizes it.  Held with Ctrl it always does, because
// then the user has said which window they mean and a modified wheel is not
// something anything underneath is waiting for.  On its own it resizes only
// while the clock's Settings dialog is open: outside that the clock is a thing
// sitting on the desktop, and a bare wheel over it should be left for whatever
// is behind.  With Settings open the user is plainly adjusting this clock, and
// the wheel is the quickest way to size it against what is behind it --
// something the slider cannot show, because the dialog is in the way.
void ClockWindow::wheelEvent(QWheelEvent *event)
{
    const bool held = event->modifiers().testFlag(Qt::ControlModifier);
    if ((!m_settings && !held) || m_moveMode || m_picking) {
        QWidget::wheelEvent(event);
        return;
    }
    m_wheelResidue += event->angleDelta().y();
    const int notches = m_wheelResidue / 120;
    if (notches != 0) {
        m_wheelResidue -= notches * 120;
        startZoom();
        nudgeSize(notches, event->modifiers().testFlag(Qt::ShiftModifier));
    }
    event->accept();
}

int ClockWindow::sizeStep(int value, bool fine)
{
    const double share = fine ? 0.02 : 0.10;
    return std::max(1, static_cast<int>(std::lround(value * share)));
}

void ClockWindow::nudgeSize(int notches, bool fine)
{
    // Through the dialog's slider where there is one, so the number on screen
    // keeps up, and so Cancel still restores the size the clock had when
    // Settings opened.
    if (m_settings) {
        m_settings->nudgeSize(notches, fine);
        return;
    }

    const int wanted = std::clamp(m_cfg.size + notches * sizeStep(m_cfg.size, fine),
                                  kSizeMin, maxSize());
    if (wanted == m_cfg.size)
        return;
    Config values = m_cfg;
    values.size = wanted;
    applySettings(values);
    // Saved on a timer rather than at once: a spin of the wheel is a run of
    // these, and only where it stops is worth writing down.
    queueSave();
}

// A clock being made smaller shrinks away from the pointer, and once the
// pointer is outside it the next notch belongs to whatever is underneath --
// so a spin would stop partway down and the small end of the range could not
// be reached by wheel at all.  Growing has the same trouble in reverse at the
// moment the clock is nudged out from under the cursor.  Holding the pointer
// for the length of the spin keeps every notch coming to the clock the user
// started on, which is the one they are looking at.
//
// The grab lets go on its own a moment after the last notch.  It is not ended
// by the pointer leaving, because leaving is the very thing it exists to
// survive; a watchdog is what makes it safe, since a grab that outlived its
// spin would be a desktop that had stopped answering the mouse.
void ClockWindow::startZoom()
{
    if (!m_zoomTimer) {
        m_zoomTimer = new QTimer(this);
        m_zoomTimer->setSingleShot(true);
        // Long enough to ride out the gap between notches of a slow, deliberate
        // spin, short enough that a grab is never left lying around.
        m_zoomTimer->setInterval(600);
        connect(m_zoomTimer, &QTimer::timeout, this, [this] { stopZoom(); });
    }
    if (!m_zooming) {
        hideTimeTip();
        m_zooming = true;
        m_zoomAnchor = QCursor::pos();
        // Tracking as well as the grab, because no button is held and motion
        // would otherwise not be reported at all -- and motion is how the spin
        // knows the user has finished with it.
        setMouseTracking(true);
        grabMouse();
    }
    m_zoomTimer->start();
}

void ClockWindow::stopZoom()
{
    if (!m_zooming)
        return;
    m_zooming = false;
    if (m_zoomTimer)
        m_zoomTimer->stop();
    releaseMouse();
    if (!m_moveMode)
        setMouseTracking(false);
    // Whole notches only while the spin lasts; a remainder left over from one
    // spin must not tip the first notch of the next.
    m_wheelResidue = 0;
}

void ClockWindow::keyPressEvent(QKeyEvent *event)
{
    // Reaching for the keyboard is the end of a spin whatever the key does.
    stopZoom();
    const int key = event->key();
    const Qt::KeyboardModifiers mods = event->modifiers();
    // The "command" modifier: Ctrl everywhere, which Qt already reports as
    // Qt::ControlModifier for Cmd on macOS.  Meta is deliberately not accepted
    // elsewhere, because window managers frequently alias it onto Alt.
    bool command = mods.testFlag(Qt::ControlModifier);
#if defined(Q_OS_MACOS)
    command = command || mods.testFlag(Qt::MetaModifier);
#endif

    // Keyboard pivot placement, while a pick is in progress. These come before
    // everything else so the arrow keys cannot be taken for anything else.
    if (m_picking && !m_draggingCenter) {
        // A coarse step for crossing the face, a single pixel for settling on
        // the exact spot.
        const int step = mods.testFlag(Qt::ShiftModifier) ? 10 : 1;
        switch (key) {
        case Qt::Key_Left:
            nudgeCenter(-step, 0);
            return;
        case Qt::Key_Right:
            nudgeCenter(step, 0);
            return;
        case Qt::Key_Up:
            nudgeCenter(0, -step);
            return;
        case Qt::Key_Down:
            nudgeCenter(0, step);
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            commitPick();
            return;
        default:
            break;
        }
    }

    if (key == Qt::Key_F1) {
        showHelp();
        return;
    }
    if (key == Qt::Key_Escape) {
        if (m_moveMode)
            cancelMoveMode();
        else if (m_picking)
            cancelPicking();
        else
            hideClock();
        return;
    }
#if !defined(Q_OS_MACOS)
    if (key == Qt::Key_F4 && mods.testFlag(Qt::AltModifier)) {
        hideClock();
        return;
    }
#endif
    if (command) {
        switch (key) {
        case Qt::Key_C:
            ClockManager::instance().quitNow();
            return;
        case Qt::Key_S:
            openSettings();
            return;
        case Qt::Key_K:
            manageClocks();
            return;
        case Qt::Key_N:
            newClock();
            return;
        case Qt::Key_M:
            startMoveMode();
            return;
        case Qt::Key_H:
            hideClock();
            return;
        case Qt::Key_A:
            showAbout();
            return;
        case Qt::Key_R:
            confirmReset();
            return;
        case Qt::Key_Q:
            ClockManager::instance().quitNow();
            return;
#if defined(Q_OS_MACOS)
        // Cmd+W closes a window on macOS; here that means hiding this clock.
        case Qt::Key_W:
            hideClock();
            return;
#endif
        default:
            break;
        }
    }
    QWidget::keyPressEvent(event);
}

// -------------------------------------------------------------- move on mouse

// Pick the clock up onto the pointer. It centres on the cursor straight away
// and follows it until any mouse button is pressed, which drops it there.
void ClockWindow::startMoveMode()
{
    hideTimeTip();
    // Move mode takes the pointer for itself, so a spin still holding it has
    // to let go first.
    stopZoom();
    if (m_moveMode)
        return;
    cancelPicking();
    m_dragging = false;
    m_moveMode = true;
    m_positionBeforeMove = pos();

    setCursor(Qt::SizeAllCursor);
    // Without an explicit grab the pointer leaves the window on the first
    // motion and the moves stop arriving; tracking is needed as well because no
    // button is held down.
    setMouseTracking(true);
    grabMouse();
    grabKeyboard();

    centerOnCursor();
}

void ClockWindow::stopMoveMode()
{
    if (!m_moveMode)
        return;
    m_moveMode = false;
    releaseKeyboard();
    releaseMouse();
    setMouseTracking(false);
    unsetCursor();
    rememberPlacement();
    flushSave();
}

// Abandon the move and put the clock back where it was picked up from.
void ClockWindow::cancelMoveMode()
{
    if (!m_moveMode)
        return;
    const QPoint back = m_positionBeforeMove;
    stopMoveMode();
    move(back);
}

// Centre the window on the pointer, kept whole on the screen the pointer is on
// so it cannot be carried off the edge of the desktop.
void ClockWindow::centerOnCursor()
{
    const QPoint cursor = QCursor::pos();
    QPoint topLeft(cursor.x() - width() / 2, cursor.y() - height() / 2);
    const QScreen *screen = QGuiApplication::screenAt(cursor);
    if (!screen)
        screen = currentScreen();
    if (screen)
        topLeft = clampToScreen(topLeft, screen);
    move(topLeft);
}

// -------------------------------------------------------------- centre pick

void ClockWindow::startPicking()
{
    hideTimeTip();
    stopZoom();
    m_picking = true;
    m_draggingCenter = false;
    m_centerBeforePick = m_cfg.center;
    m_hadCenterBeforePick = true;
    setCursor(Qt::CrossCursor);
    // A face that was turning stops here, so the mask and the tick rate both
    // want revisiting.
    applyTickRate();
    applyHitMask();

    // Take the keyboard so the arrow keys drive the pivot. The click that
    // starts a pick lands on the Settings dialog, which would otherwise keep
    // focus and swallow every arrow key into its own widget navigation.
    raise();
    activateWindow();
    setFocus(Qt::OtherFocusReason);

    update();
}

// Settle the pivot where it currently sits and leave pick mode.
void ClockWindow::commitPick()
{
    m_centerBeforePick.reset();
    m_hadCenterBeforePick = false;
    queueSave();
    stopPicking();
}

// Nudge the pivot by whole pixels of the clock face.
//
// Working in pixels rather than in the stored fraction keeps a step the same
// visible distance whatever the clock's size, and matches what the readout in
// Settings shows.
void ClockWindow::nudgeCenter(int dx, int dy)
{
    if (!m_picking)
        return;
    // An unset centre means "middle of the canvas"; starting from the effective
    // position is what makes the first arrow key move from where the crosshair
    // is actually drawn rather than jumping.
    previewCenter(centerPixels() + QPointF(dx, dy));
}

void ClockWindow::stopPicking()
{
    if (!m_picking && !m_draggingCenter)
        return;
    m_picking = false;
    m_draggingCenter = false;
    unsetCursor();
    applyTickRate();
    applyHitMask();
    // Hand the keyboard back so the dialog is immediately usable again.
    if (m_settings) {
        m_settings->raise();
        m_settings->activateWindow();
    }
    update();
}

// Abandon a pick, putting the pivot back where it started.
void ClockWindow::cancelPicking()
{
    if (m_hadCenterBeforePick || m_picking)
        setCenter(m_centerBeforePick, false);
    m_centerBeforePick.reset();
    m_hadCenterBeforePick = false;
    stopPicking();
}

// Move the pivot to a pointer position, without writing the config.
void ClockWindow::previewCenter(const QPointF &pos)
{
    const QSize size = pixelSize();
    const double fx = size.width() > 0
                          ? std::min(1.0, std::max(0.0, pos.x() / size.width()))
                          : 0.5;
    const double fy = size.height() > 0
                          ? std::min(1.0, std::max(0.0, pos.y() / size.height()))
                          : 0.5;
    setCenter(QPointF(fx, fy), false);
}

void ClockWindow::setCenter(const std::optional<QPointF> &center, bool save)
{
    m_cfg.center = sanitizeCenter(center);
    applyHitMask();  // the hands turn about the pivot, so the shape moves with it
    update();
    if (save)
        queueSave();
    if (m_settings)
        m_settings->refreshCenter();
}

// ------------------------------------------------------------------ drawing

void ClockWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // The mask is in widget coordinates, so a stale one would clip the clock
    // to its old shape.  Dragging the size slider resizes long before the
    // raster catches up, so this cannot wait for the rebuild.
    applyHitMask();
}

void ClockWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    // Clear to fully transparent pixels rather than blending over whatever the
    // surface happened to hold.
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // The face is rasterised at the widget's pixel size, so it normally lands
    // 1:1.  Mid-resize the raster may still be the previous size, so it is
    // stretched to fit until it catches up.
    //
    // Only the artwork turns.  The hands and the marks are how the clock is
    // read, and a clock that is spinning is still meant to be telling the
    // time, so they are drawn afterwards on an untouched painter.
    //
    // Turning, the whole drawing moves to the middle of the window and shrinks
    // to fit the disc it sweeps -- see drawCenter and drawScale.  Standing
    // still both come to nothing and the face lands exactly where it always
    // did.
    const QPointF center = drawCenter();
    const double radius = handRadius();
    if (!m_raster.isNull()) {
        const double angle = advanceSpin();
        const double scale = drawScale();
        const QPointF pivot = centerPixels();
        painter.save();
        painter.translate(center);
        if (angle != 0.0)
            painter.rotate(angle);
        if (scale != 1.0)
            painter.scale(scale, scale);
        painter.translate(-pivot);
        painter.drawImage(QRectF(0, 0, width(), height()), m_raster,
                          QRectF(m_raster.rect()));
        painter.restore();
    }

    drawMarks(painter, m_cfg, center.x(), center.y(), radius, width(), height());

    const QTime now = QTime::currentTime();
    // Stepping, the hands sit on whole seconds and whole minutes. Sweeping,
    // each takes the exact angle for this instant: the millisecond folds into a
    // fractional second, which folds into a fractional minute, which the hour
    // hand already trails. So one flag sweeps all three.
    double seconds = now.second();
    double minutes = now.minute();
    if (m_cfg.smoothSweep) {
        seconds = now.second() + now.msec() / 1000.0;
        minutes = now.minute() + seconds / 60.0;
    }
    drawHands(painter, m_cfg, center.x(), center.y(), radius, width(), height(),
              now.hour() % 12, minutes, seconds);

    if (m_picking)
        drawPickHint(painter, center.x(), center.y(), radius);

    // Windows decides what a frameless translucent window can be clicked on by
    // looking at the alpha of each pixel, and a pixel at zero is not there to
    // be hit however the window is shaped -- the mask set above governs the X11
    // build and is ignored.  So every pixel the mask claims is laid down again
    // underneath the drawing at an alpha of one: enough that the pixel exists
    // as far as the window system is concerned, far too little for anyone to
    // see.  Composed underneath, it reaches only the gaps and leaves every
    // pixel that was drawn exactly as it was.
    if (!m_hitFill.isNull()) {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
        painter.drawImage(QRectF(0, 0, width(), height()), m_hitFill,
                          QRectF(m_hitFill.rect()));
    }
}

// Crosshair over the current centre while the user is picking.
void ClockWindow::drawPickHint(QPainter &painter, double cx, double cy, double radius)
{
    QPen pen(QColor(26, 153, 255, 230));
    pen.setWidthF(std::max(1.0, 0.008 * radius));
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const double span = radius * 0.25;
    painter.drawLine(QPointF(cx - span, cy), QPointF(cx + span, cy));
    painter.drawLine(QPointF(cx, cy - span), QPointF(cx, cy + span));
    painter.drawEllipse(QPointF(cx, cy), span * 0.45, span * 0.45);
}

// ----------------------------------------------------------------- settings

void ClockWindow::buildMenu()
{
    m_menu = new QMenu(this);

    m_onTopAction = m_menu->addAction(QStringLiteral("Always on top"));
    m_onTopAction->setCheckable(true);
    m_onTopAction->setChecked(m_cfg.alwaysOnTop);
    connect(m_onTopAction, &QAction::toggled, this,
            [this](bool on) { setAlwaysOnTop(on); });

    // "K" because C, M, S, H, A, R and Q are all spoken for -- Ctrl+C is one
    // of the ways out of the program.
    QAction *manage = m_menu->addAction(menuHotkey(QStringLiteral("Manage clocks"), "K"));
    connect(manage, &QAction::triggered, this, &ClockWindow::manageClocks);

    // Next to Manage clocks because that is where it lands: a new clock is
    // made by being named in the list, so the list comes up with the name
    // waiting to be typed.
    QAction *create = m_menu->addAction(menuHotkey(QStringLiteral("New clock"), "N"));
    connect(create, &QAction::triggered, this, &ClockWindow::newClock);

    QAction *settings = m_menu->addAction(menuHotkey(QStringLiteral("Settings"), "S"));
    connect(settings, &QAction::triggered, this, &ClockWindow::openSettings);

    QAction *move = m_menu->addAction(menuHotkey(QStringLiteral("Move"), "M"));
    connect(move, &QAction::triggered, this, [this] {
        // Deferred until the menu has closed and given up its own mouse grab,
        // which would otherwise fight the one move mode takes.
        QTimer::singleShot(0, this, [this] { startMoveMode(); });
    });

    QAction *reset = m_menu->addAction(menuHotkey(QStringLiteral("Reset defaults"), "R"));
    connect(reset, &QAction::triggered, this, &ClockWindow::confirmReset);

    m_menu->addSeparator();

    // The clock face can be any SVG, and this is where the good free ones are.
    QAction *freesvg = m_menu->addAction(QStringLiteral("Find faces at freesvg.org"));
    connect(freesvg, &QAction::triggered, this, [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://freesvg.org/")));
    });

    // F1 rather than Ctrl+H, which Hide now has; F1 is where a user looks for
    // help anyway.  Written out in full because it takes no modifier.
    QAction *help = m_menu->addAction(QStringLiteral("Help\tF1"));
    connect(help, &QAction::triggered, this, &ClockWindow::showHelp);

    QAction *about = m_menu->addAction(menuHotkey(QStringLiteral("About"), "A"));
    connect(about, &QAction::triggered, this, &ClockWindow::showAbout);

    m_menu->addSeparator();

    // Hide takes this one clock off screen and leaves the rest running; the
    // manage dialog can bring it back.  With nothing else up there is nothing
    // left to run for, so the program ends -- which is what closing the last
    // window has always done.
    QAction *hide = m_menu->addAction(menuHotkey(QStringLiteral("Hide"), "H"));
    connect(hide, &QAction::triggered, this, [this] { hideClock(); });

    // Quit ends the program whatever else is open.  That is the difference
    // between it and Hide, which is only ever about this window.
    QAction *quit = m_menu->addAction(menuHotkey(QStringLiteral("Quit"), "Q"));
    connect(quit, &QAction::triggered, this, [] { ClockManager::instance().quitNow(); });
}

void ClockWindow::manageClocks()
{
    ManageClocksDialog::showDialog(this);
}

void ClockWindow::newClock()
{
    ManageClocksDialog::newClockIn(this);
}

void ClockWindow::applyAlwaysOnTop()
{
    // Where the window manager can simply be told, tell it. Reaching the same
    // end through Qt's window flag makes Qt throw the native window away and
    // build another, which loses the position, drops the mapping for a moment,
    // and hands back the group hint that makes every clock stack as one.
    if (windowgroup::setAlwaysOnTop(this, m_cfg.alwaysOnTop))
        return;

    if (windowFlags().testFlag(Qt::WindowStaysOnTopHint) == m_cfg.alwaysOnTop)
        return;
    const bool wasVisible = isVisible();
    const QPoint where = pos();
    setWindowFlag(Qt::WindowStaysOnTopHint, m_cfg.alwaysOnTop);
    if (wasVisible) {
        // Changing the flags recreates the native window, which loses both the
        // placement and the mapping.
        show();
        move(where);
        detachFromGroup();
    }
}

// The face turns at a percentage of one full turn a second, so 100 is a turn
// a second and 50 is a turn every two.  The sign is the direction: positive
// goes the way the hands go, negative against them, and the arithmetic below
// needs nothing said about it -- a negative rate simply accumulates a negative
// angle, which is what turning the other way is.  The angle is integrated from
// real elapsed time rather than counted in frames: a dropped frame then costs
// a moment of smoothness instead of putting the face permanently behind.
double ClockWindow::advanceSpin()
{
    if (!spinning()) {
        // Wound back to square, so that turning the spin on again starts the
        // face the way it was drawn rather than wherever it was left.
        m_spinAngle = 0.0;
        m_spinClock.invalidate();
        return 0.0;
    }
    if (!m_spinClock.isValid()) {
        m_spinClock.start();
        m_spinLastNs = 0;
        return m_spinAngle;
    }
    const qint64 now = m_spinClock.nsecsElapsed();
    const double seconds = (now - m_spinLastNs) / 1000000000.0;
    m_spinLastNs = now;
    const double turnsPerSecond = kSpinMaxTurns * m_cfg.faceSpin / 100.0;
    m_spinAngle = std::fmod(m_spinAngle + seconds * 360.0 * turnsPerSecond,
                            360.0);
    return m_spinAngle;
}

// A sweeping minute hand has to be redrawn continuously; a stepping one only
// needs to be checked often enough to land on the new second promptly.  A
// turning face has to be redrawn continuously whatever the hands are doing.
void ClockWindow::applyTickRate()
{
    const int interval = (m_cfg.smoothSweep || spinning()) ? kSmoothIntervalMs
                                                           : kSteppedIntervalMs;
    if (m_tick->interval() == interval)
        return;
    m_tick->setInterval(interval);
    // Leaving smooth mode, the cached second is whatever the last sweep frame
    // saw; clearing it makes the next poll repaint rather than wait a second.
    m_lastSecond = -1;
}

void ClockWindow::setAlwaysOnTop(bool on)
{
    if (m_cfg.alwaysOnTop == on)
        return;
    m_cfg.alwaysOnTop = on;
    syncAlwaysOnTop();
    queueSave();
}

// Keep the check mark in step when the setting changes elsewhere.
void ClockWindow::syncAlwaysOnTop()
{
    applyAlwaysOnTop();
    if (m_onTopAction && m_onTopAction->isChecked() != m_cfg.alwaysOnTop) {
        const QSignalBlocker blocker(m_onTopAction);
        m_onTopAction->setChecked(m_cfg.alwaysOnTop);
    }
}

void ClockWindow::openSettings()
{
    if (m_settings) {
        m_settings->raise();
        m_settings->activateWindow();
        return;
    }

    // Snapshot so Cancel can restore the exact previous look.
    const Config snapshot = m_cfg;
    auto *dialog = new SettingsDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    m_settings = dialog;

    connect(dialog, &QDialog::finished, this, [this, dialog, snapshot](int result) {
        if (m_settings != dialog)
            return;  // closeSettings() already took it away
        m_settings = nullptr;
        stopPicking();
        if (result == QDialog::Accepted) {
            applySettings(dialog->values());
            flushSave();
        } else {
            applySettings(snapshot);
            queueSave();
        }
    });

    dialog->show();
    placeDialog(dialog);
}

// Put the dialog beside the clock so it never covers it.
void ClockWindow::placeDialog(QWidget *dialog)
{
    const QScreen *screen = QGuiApplication::screenAt(frameGeometry().center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    const QRect area = screen->availableGeometry();
    const QRect clock = frameGeometry();
    const QSize dlg = dialog->frameGeometry().size();

    int x = clock.x() + clock.width() + 12;
    if (x + dlg.width() > area.x() + area.width())
        x = clock.x() - dlg.width() - 12;  // try the left side
    if (x < area.x()) {
        x = std::min(std::max(clock.x() + clock.width() + 12, area.x()),
                     area.x() + area.width() - dlg.width());
    }
    const int y = std::min(std::max(clock.y(), area.y()),
                           area.y() + std::max(0, area.height() - dlg.height()));
    dialog->move(x, y);
}

// Drop the settings dialog without running its Cancel restore.
void ClockWindow::closeSettings()
{
    SettingsDialog *dialog = m_settings;
    m_settings = nullptr;
    if (dialog) {
        stopPicking();
        dialog->close();
    }
}

void ClockWindow::confirmReset()
{
    if (askReset(this))
        resetDefaults();
}

// The menu's Reset defaults.  The Settings dialog asks its own version, which
// also offers to undo just the changes made while it has been open.
bool ClockWindow::askReset(QWidget *parent)
{
    QMessageBox box(parent);
    box.setWindowTitle(QStringLiteral("vclock"));
    box.setIcon(QMessageBox::Question);
    box.setText(QStringLiteral("Reset this clock?"));
    box.setInformativeText(QStringLiteral("Every setting goes back to its default."));
    box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Cancel);
    return box.exec() == QMessageBox::Ok;
}

// The settings a reset restores: the program's defaults, sized for the monitor
// this clock is on now rather than the one it started on.  Shared with the
// Settings dialog's Reset button so the two cannot restore different things.
Config ClockWindow::defaultConfig() const
{
    Config values = m_cfg;
    copyResetKeys(Config(), values);
    if (values.size <= 0)
        values.size = defaultSizeOn(screen());
    values.size = std::min(values.size, maxSize());
    return values;
}

// Restore every setting except the on-screen position.
void ClockWindow::resetDefaults()
{
    closeSettings();
    applySettings(defaultConfig());
    flushSave();
}

void ClockWindow::showHelp()
{
    auto *box = new QMessageBox(this);
    box->setAttribute(Qt::WA_DeleteOnClose, true);
    box->setWindowTitle(QStringLiteral("vclock help"));
    box->setIconPixmap(appIconPixmap(96, devicePixelRatioF()));
    box->setTextFormat(Qt::RichText);
    box->setText(QStringLiteral("<b>vclock help</b>"));
    box->setInformativeText(
        QStringLiteral(
            "<b>Mouse</b><br>"
            "Left drag &mdash; move the clock<br>"
            "Double click &mdash; settings<br>"
            "Right click &mdash; menu<br>"
            "Wheel &mdash; resize, holding %1 or while settings are open "
            "(Shift for finer steps)<br>"
            "<br><b>Keyboard</b><br>"
            "%1+S &mdash; settings<br>"
            "%1+M &mdash; carry the clock on the pointer<br>"
            "%1+K &mdash; manage clocks<br>"
            "%1+N &mdash; new clock<br>"
            "F1 &mdash; this help<br>"
            "%1+A &mdash; about<br>"
            "%1+R &mdash; reset defaults<br>"
            "%1+H &mdash; hide this clock<br>"
            "%2 &mdash; also hide it<br>"
            "%1+Q &mdash; quit, closing every clock<br>"
            "<br><b>Clock face</b><br>"
            "Any SVG can be used. Within the artwork white is treated as the face "
            "color and black as the wire color, and both can be recolored from "
            "Settings.<br>"
            "<br><b>Hand center</b><br>"
            "Settings &#9656; Pick on clock, then drag on the face. The hands and "
            "marks follow the pointer and settle where you release the button. "
            "The arrow keys move the pivot one pixel at a time, Shift+arrow moves "
            "it ten, Enter accepts and Esc cancels.<br>"
            "<br><b>Move</b><br>"
            "Menu &#9656; Move, or %1+M, picks the clock up onto the pointer. It "
            "centers on the cursor and follows it until any mouse button is "
            "clicked. Esc puts it back where it started.")
            .arg(QLatin1String(kCmdLabel), QLatin1String(kHideKeys)));
    box->setStandardButtons(QMessageBox::Close);
    box->show();
}

void ClockWindow::showAbout()
{
    auto *box = new QMessageBox(this);
    box->setAttribute(Qt::WA_DeleteOnClose, true);
    box->setWindowTitle(QStringLiteral("About vclock"));
    box->setIconPixmap(appIconPixmap(96, devicePixelRatioF()));
    box->setText(QStringLiteral("<b>vclock</b>"));
    box->setInformativeText(QString::fromUtf8(aboutText())
                            + QStringLiteral("\n\nWritten by Wade Ryan\nSeptember, 2026"));
    box->setStandardButtons(QMessageBox::Ok);
    box->setDefaultButton(QMessageBox::Ok);
    box->show();
}

// Apply a settings record to the live widget (used for preview too).
void ClockWindow::applySettings(const Config &values)
{
    // A generated face is built from its colours as well as its name, so a
    // change to either is a new face and not merely a repaint of this one.
    const bool newFace = values.faceSvg != m_cfg.faceSvg
                         || values.faceDefault != m_cfg.faceDefault
                         || (values.generatedFace()
                             && (values.generatorFaceColor() != m_cfg.generatorFaceColor()
                                 || values.generatorWireColor() != m_cfg.generatorWireColor()
                                 || values.faceMultiHue() != m_cfg.faceMultiHue()));
    const bool changedColor = values.wireColor != m_cfg.wireColor
                              || values.faceColor != m_cfg.faceColor
                              || values.faceOpacity != m_cfg.faceOpacity
                              || values.wireOpacity != m_cfg.wireOpacity
                              || values.faceRecolor != m_cfg.faceRecolor;
    const bool changedSize = values.size != m_cfg.size;
    const bool changedOnTop = values.alwaysOnTop != m_cfg.alwaysOnTop;
    const bool changedSmooth = values.smoothSweep != m_cfg.smoothSweep
                               || (values.faceSpin != 0) != (m_cfg.faceSpin != 0);
    const bool changedRegen = values.faceRegen != m_cfg.faceRegen
                              || values.faceRegenMinutes != m_cfg.faceRegenMinutes;

    m_cfg = values;
    if (changedRegen)
        syncRegenTimer();
    if (changedOnTop)
        syncAlwaysOnTop();
    if (changedSmooth)
        applyTickRate();
    if (newFace)
        m_face = openFace(m_cfg.facePath(), m_cfg.generatorFaceColor(), m_cfg.generatorWireColor(),
                      m_cfg.faceMultiHue());
    if (newFace || changedColor) {
        m_rebuildTimer->stop();
        applySize();
        rebuildRaster();
    } else if (changedSize) {
        // Resizing alone is the hot path while dragging the size slider: move
        // the window now and re-raster once the drag settles.
        applySize();
        scheduleRebuild();
    }
    if (changedSize)
        rememberPlacement();  // the size belongs to the monitor it was set on
    // The indices can be moved out past the face, which changes the shape the
    // clock presents to the pointer without changing the artwork at all.
    applyHitMask();
    update();
}
