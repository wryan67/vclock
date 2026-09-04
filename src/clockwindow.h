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

    // Base name of this clock's config, or empty when it is the default one.
    QString configName() const;

    // The file this clock reads and writes.
    QString configFilePath() const { return m_configPath; }

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
    void manageClocks();

    // Re-read this clock's display name, which the manage dialog can change
    // while the clock is up.
    void refreshTitle();

signals:
    // Emitted from closeEvent, before the window is deleted, so the manager
    // can drop it from the set of running clocks.
    void closed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
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
    QPoint defaultPositionOn(const QScreen *screen) const;
    QPoint clampToScreen(const QPoint &topLeft, const QScreen *screen) const;
    void rememberPlacement();         // record position/size against the current monitor
    void handleScreenRemoved();
    void centerOnCursor();

    void rebuildRaster();
    void scheduleRebuild();
    void queueSave();
    void flushSave();
    void applyAlwaysOnTop();
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

    Config m_cfg;
    std::unique_ptr<Face> m_face;
    QImage m_raster;                    // the recoloured, rasterised face
    QRectF m_bounds{0, 0, 1, 1};        // content bbox of the raster, as fractions

    SettingsDialog *m_settings = nullptr;
    bool m_picking = false;
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

    QTimer *m_tick = nullptr;
    QTimer *m_saveTimer = nullptr;
    QTimer *m_rebuildTimer = nullptr;
    int m_lastSecond = -1;
};
