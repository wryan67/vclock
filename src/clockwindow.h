// The clock itself: an undecorated, translucent window that paints its hands
// over a rasterised SVG face.
#pragma once

#include "config.h"

#include <QElapsedTimer>
#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QWidget>

#include <memory>
#include <optional>
#include <vector>

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
    void regenerateFace();
    void syncRegenTimer();

    // Largest allowed clock size: the height of the screen it sits on.
    int maxSize() const;

    // How far one notch of the wheel moves a clock of this size.  A fixed
    // share of the size rather than a fixed number of pixels, so a notch is
    // the same visible change whatever the clock is: five pixels is a fifth of
    // a small clock and nothing at all on a large one.
    //
    // The plain wheel takes the big step and Shift the small one, which is the
    // way round a wheel is actually used: you spin it to get somewhere and
    // then want to creep the last bit, and creeping is the part worth holding
    // a key for.
    static int sizeStep(int value, bool fine);

    // Resize by whole notches of the wheel.  Goes through the Settings dialog
    // when there is one, so the number on screen keeps up and Cancel can still
    // put the size back; otherwise it moves the clock itself.
    void nudgeSize(int notches, bool fine);

    // Hold the pointer for the length of a spin of the wheel.  A clock that is
    // shrinking walks out from under the cursor, and the notch after that
    // would go to whatever is now beneath it, so a spin could not reach the
    // small end of the range at all.  The grab keeps every notch coming here
    // until the spinning stops.
    void startZoom();
    void stopZoom();

    // How far round the face has turned by now, in degrees, advancing it by
    // however long it has been since this was last asked.  Integrating real
    // elapsed time rather than counting frames keeps the speed honest when a
    // frame is dropped, and is what makes the turn smooth in the same way a
    // sweeping second hand is.
    double advanceSpin();
    // Whether the face is turning at all, either way round.  Picking the pivot
    // holds it still: that job is placing a point on the artwork, which cannot
    // be done while the artwork is moving and standing somewhere other than
    // where it really sits.
    bool spinning() const { return m_cfg.faceSpin != 0 && !m_picking; }
    // Turns a second, signed; and how far round the face gets between two
    // frames at the rate the clock is currently ticking at.
    double spinTurnsPerSecond() const;
    double spinStepDegrees() const;
    // Whether the turn is fast enough that its own smear hides everything the
    // resampling filter would have smoothed.
    bool spinBlurred() const;
    // The frame interval to animate at, taken from the screen the clock is on.
    int smoothIntervalMs() const;
    // Follow the refresh rate of whichever screen the clock is now on.
    void watchScreen();
    // How many steps the ring of pre-turned faces should have, or zero for no
    // ring at all.  Drop whatever ring is there if it no longer matches.
    int spinFrameCount() const;
    void discardSpinFrames();
    // The face already drawn at the nearest step to this angle, preparing it
    // first if this is the first time that step has come round.  Null when
    // there is no ring, in which case the caller resamples the face itself.
    const QImage *spinFrame(double angle);

    QPointF centerPixels() const;
    double handRadius() const;      // as drawn, after any spin-fit shrink
    double handRadiusRaw() const;   // before it
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
    void newClock();

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
    void wheelEvent(class QWheelEvent *event) override;
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
    double reachRadius() const;     // how far the hands and indices go from the pivot
    double reachRadiusRaw() const;  // the same, before any spin-fit shrink
    double spinRadius() const;      // how far the face sweeps from the pivot when turning
    // Where the drawing is centred and how much it is shrunk to fit.  Both
    // come to nothing unless the face is turning; see their definitions.
    QPointF drawCenter() const;
    double drawScale() const;
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
    static void fillEnclosedGaps(QImage &stencil);

    // The date-and-time bubble shown while the pointer rests on the face.
    void armTimeTip();
    void showTimeTip();
    void hideTimeTip();

    Config m_cfg;
    std::unique_ptr<Face> m_face;
    QImage m_raster;                    // the recoloured, rasterised face
    QImage m_coverage;                  // where the artwork is, before the user's opacity
    QImage m_hitFill;                   // the hit shape at an invisible alpha; see paintEvent
    QRectF m_bounds{0, 0, 1, 1};        // content bbox of the raster, as fractions
    // Farthest the artwork reaches from the pivot, as a fraction of the width;
    // the radius of the disc it sweeps when it turns.
    double m_spinReach = 0.5;
    // How much of the face is strongly contrasted edge; see edgeDensity.
    // Decides whether a fast turn may be resampled coarsely.
    double m_faceDetail = 0.0;

    // Faces already drawn turned, one per step of a coarse ring of angles, so
    // that a turning clock can blit a frame it prepared earlier instead of
    // resampling the whole face afresh sixty times a second.  Entries are
    // filled the first time each angle comes round rather than all at once,
    // which spreads the cost over the first revolution instead of stalling on
    // the way in.  Empty when the ring would not fit the memory budget.
    std::vector<QImage> m_spinFrames;
    // What the ring was built for.  Any of it changing makes every frame in it
    // wrong, so the ring is thrown away and measured again.
    QSize m_spinFramesSize;
    double m_spinFramesScale = 0.0;
    QPointF m_spinFramesCenter;
    QPointF m_spinFramesPivot;
    qint64 m_spinFramesKey = 0;
    bool m_spinFramesSmooth = false;
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
    QTimer *m_regenTimer = nullptr;
    int m_lastSecond = -1;
    // Wheel notches arrive as 120ths of a degree turned, and a trackpad sends
    // a stream of small deltas rather than whole notches, so the remainder is
    // carried between events instead of being rounded away.
    int m_wheelResidue = 0;
    // Whether the pointer is held for the duration of a spin of the wheel;
    // see startZoom.
    bool m_zooming = false;
    QTimer *m_zoomTimer = nullptr;
    // Where the pointer was when the spin began.  It does not move while a
    // clock resizes under it, so travelling away from here is the user leaving
    // rather than anything the resize did.
    QPoint m_zoomAnchor;

    // How far round the face has turned, and when that was last worked out.
    // The angle is carried rather than derived from the time of day so that
    // changing the speed picks up from where the face is, instead of jumping
    // to wherever a faster clock would have got to by now.
    //
    // The timer runs on and the reading it gave last time is kept, rather than
    // the timer being restarted each frame.  Restarting throws away whatever
    // had elapsed since the moment it was read, and at sixty frames a second
    // those slivers add up to a face that turns measurably slow.
    double m_spinAngle = 0.0;
    QElapsedTimer m_spinClock;
    qint64 m_spinLastNs = 0;

    // The screen whose refresh rate the frame timer is following.
    QScreen *m_watchedScreen = nullptr;
    QMetaObject::Connection m_refreshConn;
};
