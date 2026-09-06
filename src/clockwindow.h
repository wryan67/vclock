// The clock itself: an undecorated, translucent window that paints its hands
// over a rasterised SVG face.
#pragma once

#include "config.h"

#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QWidget>

#include <memory>
#include <optional>

class Face;
class QAction;
class QMenu;
class QScreen;
class QTimer;
class SettingsDialog;
class TimeTip;

class ClockWindow : public QWidget
{
    Q_OBJECT

public:
    // configPath names the file this clock reads and writes; empty means the
    // default one. Each clock owns its own, so several can run side by side
    // without writing over each other.
    explicit ClockWindow(const QString &configPath = QString());
    ~ClockWindow() override;

    const Config &cfg() const { return m_cfg; }

    // Raise or drop this one clock, and write the change out.  The menu item
    // and the box in the manage dialog are two views of it, so both go through
    // here rather than each setting the flag and saving in their own way.
    void setAlwaysOnTop(bool on);

    // Base name of this clock's config, or empty when it is the default one.
    QString configName() const;

    // The file this clock reads and writes.
    QString configFilePath() const { return m_configPath; }

    // Point the clock at a different file, because renaming it moved the one
    // it had.  The settings in hand are the ones that were just moved, so
    // nothing is reloaded -- only where the next save goes changes.
    void setConfigFilePath(const QString &path);

    // Write any pending changes out now rather than when the timer that
    // gathers them comes round.  Needed before anything outside the clock
    // touches its file, so that a save cannot land after the fact.
    void flushSave();

    // Apply a settings record to the live widget (used for preview too).
    void applySettings(const Config &values);

    // Largest allowed clock size: the height of the screen it sits on.
    int maxSize() const;

    QPointF centerPixels() const;
    double handRadius() const;
    QString faceLabel() const;

    // Let the user drag on the face to place the hands' pivot.
    void startPicking();
    void stopPicking();
    void cancelPicking();
    void setCenter(const std::optional<QPointF> &center, bool save);

    // Carry the clock on the pointer until a mouse button settles it.
    void startMoveMode();
    void stopMoveMode();
    void cancelMoveMode();

    // Put the window back where it was last seen, once it has been shown.
    void restorePosition();

    // Place the clock on a specific monitor, using that monitor's remembered
    // position and size when there is one and its default spot when there is not.
    void placeOnScreen(class QScreen *screen);

    void openSettings();
    void showHelp();
    void showAbout();
    void confirmReset();

    // Put the "are you sure?" question without acting on the answer, so the
    // menu item and the Settings dialog's Reset button ask it the same way.
    static bool askReset(class QWidget *parent);

    // What a reset restores: the defaults, sized for the monitor this clock is
    // on now.  Settings uses it to snap its controls to the same values.
    Config defaultConfig() const;

    void manageClocks();

    // Re-read this clock's display name, which the manage dialog can change
    // while the clock is up.
    void refreshTitle();

    // Take this clock off screen, which is a different thing from the window
    // being closed: hiding is a choice the user made about one clock, and is
    // remembered, whereas a close arriving from outside the program is the
    // session ending and must leave the clock marked as showing.  Every way
    // the program itself puts a clock away comes through here so that the two
    // can be told apart in closeEvent, which they otherwise cannot be.
    void hideClock();

signals:
    // Emitted from closeEvent, before the window is deleted, so the manager
    // can drop it from the set of running clocks.  True when the close came
    // from hideClock(), false when it arrived from the window system.
    void closed(bool hiding);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    QString m_configPath;

    QSize pixelSize() const;
    void applySize();

    // --- monitor-aware placement -------------------------------------------
    QScreen *currentScreen() const;   // the monitor the clock is on right now
    QScreen *startupScreen() const;   // the monitor to open on
    int maxSizeFor(const QScreen *screen) const;
    int defaultSizeOn(const QScreen *screen) const;  // for a config that has no size yet
    QPoint defaultPositionOn(const QScreen *screen) const;
    QPoint clampToScreen(const QPoint &topLeft, const QScreen *screen) const;
    void rememberPlacement();         // record position/size against the current monitor
    void handleScreenRemoved();
    void centerOnCursor();

    void rebuildRaster();
    double reachRadius() const;  // how far the hands and indices go from the pivot
    void applyHitMask();         // let clicks off the clock through to what is behind
    void scheduleRebuild();
    void queueSave();
    void applyAlwaysOnTop();
    void detachFromGroup();
    void syncAlwaysOnTop();
    void applyTickRate();
    void buildMenu();
    void placeDialog(QWidget *dialog);
    void closeSettings();
    void resetDefaults();
    void previewCenter(const QPointF &pos);
    void commitPick();
    void nudgeCenter(int dx, int dy);
    void drawPickHint(class QPainter &painter, double cx, double cy, double radius);

    // The date-and-time bubble shown while the pointer rests on the face.
    void armTimeTip();
    void showTimeTip();
    void hideTimeTip();

    Config m_cfg;
    std::unique_ptr<Face> m_face;
    QImage m_raster;                    // the recoloured, rasterised face
    QImage m_coverage;                  // where the artwork is, before the user's opacity
    QRectF m_bounds{0, 0, 1, 1};        // content bbox of the raster, as fractions

    SettingsDialog *m_settings = nullptr;
    bool m_picking = false;
    // Set by hideClock() so closeEvent can tell the user putting this clock
    // away from the window system closing it out from under us.
    bool m_hiding = false;
    bool m_draggingCenter = false;
    std::optional<QPointF> m_centerBeforePick;
    bool m_hadCenterBeforePick = false;

    bool m_dragging = false;
    QPoint m_dragOffset;
    // A left press arms a drag but does not start one: the window manager only
    // takes over once the pointer has actually travelled, so that a click that
    // stays put can still become a double click.
    bool m_dragArmed = false;
    QPoint m_pressPos;

    // Ctrl+M move mode: the clock rides the pointer until a button settles it.
    bool m_moveMode = false;
    QPoint m_positionBeforeMove;
    // Guards the minimize/maximize refusal against re-entering itself.
    bool m_restoringState = false;

    QMenu *m_menu = nullptr;
    QAction *m_onTopAction = nullptr;

    TimeTip *m_timeTip = nullptr;
    QTimer *m_tipDelay = nullptr;

    QTimer *m_tick = nullptr;
    QTimer *m_saveTimer = nullptr;
    QTimer *m_rebuildTimer = nullptr;
    int m_lastSecond = -1;
};
