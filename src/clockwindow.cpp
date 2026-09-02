#include "clockwindow.h"

#include "embedded.h"
#include "face.h"
#include "render.h"
#include "settingsdialog.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QGuiApplication>
#include <QIcon>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QScreen>
#include <QSignalBlocker>
#include <QStringList>
#include <QTime>
#include <QTimer>
#include <QWindow>

#include <algorithm>
#include <cmath>

namespace {

#if defined(Q_OS_MACOS)
// macOS users expect Cmd; Qt already maps Qt::ControlModifier onto it.
const char *kCmdLabel = "Cmd";
const char *kQuitKeys = "Cmd+Q, Cmd+W, Esc";
#else
const char *kCmdLabel = "Ctrl";
const char *kQuitKeys = "Ctrl+C, Alt+F4, Esc";
#endif

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

ClockWindow::ClockWindow()
    : QWidget(nullptr)
{
    m_cfg = loadConfig();

    setWindowTitle(QStringLiteral("vclock"));
    setWindowIcon(QIcon(QPixmap::fromImage(appIconImage(64))));

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

    m_face = openFace(m_cfg.facePath());
    m_cfg.size = std::min(m_cfg.size, maxSize());
    applySize();
    rebuildRaster();

    buildMenu();

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(500);
    connect(m_saveTimer, &QTimer::timeout, this, [this] { saveConfig(m_cfg); });

    m_rebuildTimer = new QTimer(this);
    m_rebuildTimer->setSingleShot(true);
    m_rebuildTimer->setInterval(60);
    connect(m_rebuildTimer, &QTimer::timeout, this, [this] {
        rebuildRaster();
        update();
    });

    // Polled rather than fired once a second, so the hands land on the new
    // second promptly however far the timer has drifted.
    m_tick = new QTimer(this);
    m_tick->setInterval(200);
    connect(m_tick, &QTimer::timeout, this, [this] {
        const int second = QTime::currentTime().second();
        if (second != m_lastSecond) {
            m_lastSecond = second;
            update();
        }
    });
    m_tick->start();

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

    m_raster = recolor(art, m_cfg.wireColor, m_cfg.faceColor, m_cfg.faceTransparent);
    m_raster.setDevicePixelRatio(dpr);
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

void ClockWindow::queueSave()
{
    m_saveTimer->start();
}

void ClockWindow::flushSave()
{
    m_saveTimer->stop();
    m_rebuildTimer->stop();
    saveConfig(m_cfg);
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
double ClockWindow::handRadius() const
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

// Where a clock with no history goes on a given monitor: the middle of its
// working area, which is visible whatever else is already open.
QPoint ClockWindow::defaultPositionOn(const QScreen *screen) const
{
    const QRect available = screen->availableGeometry();
    return QPoint(available.x() + (available.width() - width()) / 2,
                  available.y() + (available.height() - height()) / 2);
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

void ClockWindow::closeEvent(QCloseEvent *event)
{
    closeSettings();
    flushSave();
    event->accept();
    qApp->quit();
}

void ClockWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_picking) {
            // Click and drag: the preview follows the pointer, nothing is
            // committed until the button comes back up.
            m_draggingCenter = true;
            previewCenter(localPosOf(event));
            event->accept();
            return;
        }
        m_dragOffset = globalPosOf(event) - frameGeometry().topLeft();
        // The window manager moves the window itself where it can, which is the
        // only thing that works on Wayland; elsewhere the fallback tracks the
        // pointer by hand.
        m_dragging = true;
        if (QWindow *handle = windowHandle()) {
            if (handle->startSystemMove())
                m_dragging = false;
        }
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
    if (m_draggingCenter) {
        previewCenter(localPosOf(event));
        event->accept();
        return;
    }
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(globalPosOf(event) - m_dragOffset);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void ClockWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    if (m_draggingCenter) {
        m_draggingCenter = false;
        previewCenter(localPosOf(event));
        m_centerBeforePick.reset();
        m_hadCenterBeforePick = false;
        queueSave();
        stopPicking();
        event->accept();
        return;
    }
    m_dragging = false;
    event->accept();
}

void ClockWindow::keyPressEvent(QKeyEvent *event)
{
    const int key = event->key();
    const Qt::KeyboardModifiers mods = event->modifiers();
    // The "command" modifier: Ctrl everywhere, which Qt already reports as
    // Qt::ControlModifier for Cmd on macOS.  Meta is deliberately not accepted
    // elsewhere, because window managers frequently alias it onto Alt.
    bool command = mods.testFlag(Qt::ControlModifier);
#if defined(Q_OS_MACOS)
    command = command || mods.testFlag(Qt::MetaModifier);
#endif

    if (key == Qt::Key_Escape) {
        if (m_picking)
            cancelPicking();
        else
            close();
        return;
    }
#if !defined(Q_OS_MACOS)
    if (key == Qt::Key_F4 && mods.testFlag(Qt::AltModifier)) {
        close();
        return;
    }
#endif
    if (command) {
        switch (key) {
        case Qt::Key_C:
            close();
            return;
        case Qt::Key_S:
            openSettings();
            return;
        case Qt::Key_H:
            showHelp();
            return;
        case Qt::Key_A:
            showAbout();
            return;
        case Qt::Key_R:
            confirmReset();
            return;
#if defined(Q_OS_MACOS)
        case Qt::Key_Q:
        case Qt::Key_W:
            close();
            return;
#endif
        default:
            break;
        }
    }
    QWidget::keyPressEvent(event);
}

// -------------------------------------------------------------- centre pick

void ClockWindow::startPicking()
{
    m_picking = true;
    m_draggingCenter = false;
    m_centerBeforePick = m_cfg.center;
    m_hadCenterBeforePick = true;
    setCursor(Qt::CrossCursor);
    update();
}

void ClockWindow::stopPicking()
{
    if (!m_picking && !m_draggingCenter)
        return;
    m_picking = false;
    m_draggingCenter = false;
    unsetCursor();
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
    update();
    if (save)
        queueSave();
    if (m_settings)
        m_settings->refreshCenter();
}

// ------------------------------------------------------------------ drawing

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
    if (!m_raster.isNull()) {
        painter.drawImage(QRectF(0, 0, width(), height()), m_raster,
                          QRectF(m_raster.rect()));
    }

    const QPointF center = centerPixels();
    const double radius = handRadius();
    drawMarks(painter, m_cfg, center.x(), center.y(), radius, width(), height());

    const QTime now = QTime::currentTime();
    drawHands(painter, m_cfg, center.x(), center.y(), radius, width(), height(),
              now.hour() % 12, now.minute(), now.second());

    if (m_picking)
        drawPickHint(painter, center.x(), center.y(), radius);
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

    QAction *settings = m_menu->addAction(QStringLiteral("Settings"));
    connect(settings, &QAction::triggered, this, &ClockWindow::openSettings);

    m_onTopAction = m_menu->addAction(QStringLiteral("Always on top"));
    m_onTopAction->setCheckable(true);
    m_onTopAction->setChecked(m_cfg.alwaysOnTop);
    connect(m_onTopAction, &QAction::toggled, this, [this](bool on) {
        if (m_cfg.alwaysOnTop == on)
            return;
        m_cfg.alwaysOnTop = on;
        applyAlwaysOnTop();
        queueSave();
    });

    QAction *reset = m_menu->addAction(QStringLiteral("Reset defaults"));
    connect(reset, &QAction::triggered, this, &ClockWindow::confirmReset);

    QAction *help = m_menu->addAction(QStringLiteral("Help"));
    connect(help, &QAction::triggered, this, &ClockWindow::showHelp);

    QAction *about = m_menu->addAction(QStringLiteral("About"));
    connect(about, &QAction::triggered, this, &ClockWindow::showAbout);

    m_menu->addSeparator();
    QAction *quit = m_menu->addAction(QStringLiteral("Quit"));
    connect(quit, &QAction::triggered, this, [this] { close(); });
}

void ClockWindow::applyAlwaysOnTop()
{
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
    }
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
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("vclock"));
    box.setIcon(QMessageBox::Question);
    box.setText(QStringLiteral("Are you sure?"));
    box.setInformativeText(QStringLiteral(
        "This restores every clock setting to its default, including the built-in "
        "clock face."));
    box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Cancel);
    if (box.exec() == QMessageBox::Ok)
        resetDefaults();
}

// Restore every setting except the on-screen position.
void ClockWindow::resetDefaults()
{
    closeSettings();
    Config values = m_cfg;
    copyResetKeys(Config(), values);
    values.size = std::min(values.size, maxSize());
    applySettings(values);
    flushSave();
}

void ClockWindow::showHelp()
{
    auto *box = new QMessageBox(this);
    box->setAttribute(Qt::WA_DeleteOnClose, true);
    box->setWindowTitle(QStringLiteral("vclock help"));
    box->setIcon(QMessageBox::Information);
    box->setTextFormat(Qt::RichText);
    box->setText(QStringLiteral("<b>vclock help</b>"));
    box->setInformativeText(
        QStringLiteral(
            "<b>Mouse</b><br>"
            "Left drag &mdash; move the clock<br>"
            "Right click &mdash; menu<br>"
            "<br><b>Keyboard</b><br>"
            "%1+S &mdash; settings<br>"
            "%1+H &mdash; this help<br>"
            "%1+A &mdash; about<br>"
            "%1+R &mdash; reset defaults<br>"
            "%2 &mdash; quit<br>"
            "<br><b>Clock face</b><br>"
            "Any SVG can be used. Within the artwork white is treated as the face "
            "color and black as the wire color, and both can be recolored from "
            "Settings.<br>"
            "<br><b>Hand center</b><br>"
            "Settings &#9656; Pick on clock, then drag on the face. The hands and "
            "marks follow the pointer and settle where you release the button. "
            "Esc cancels.")
            .arg(QLatin1String(kCmdLabel), QLatin1String(kQuitKeys)));
    box->setStandardButtons(QMessageBox::Close);
    box->show();
}

void ClockWindow::showAbout()
{
    auto *box = new QMessageBox(this);
    box->setAttribute(Qt::WA_DeleteOnClose, true);
    box->setWindowTitle(QStringLiteral("About vclock"));
    box->setIconPixmap(QPixmap::fromImage(appIconImage(96)));
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
    const bool newFace =
        values.faceSvg != m_cfg.faceSvg || values.faceDefault != m_cfg.faceDefault;
    const bool changedColor = values.wireColor != m_cfg.wireColor
                              || values.faceColor != m_cfg.faceColor
                              || values.faceTransparent != m_cfg.faceTransparent;
    const bool changedSize = values.size != m_cfg.size;
    const bool changedOnTop = values.alwaysOnTop != m_cfg.alwaysOnTop;

    m_cfg = values;
    if (changedOnTop)
        syncAlwaysOnTop();
    if (newFace)
        m_face = openFace(m_cfg.facePath());
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
    update();
}
