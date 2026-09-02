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
class QTimer;
class SettingsDialog;

class ClockWindow : public QWidget
{
    Q_OBJECT

public:
    ClockWindow();
    ~ClockWindow() override;

    const Config &cfg() const { return m_cfg; }

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

    // Put the window back where it was last seen, once it has been shown.
    void restorePosition();

    void openSettings();
    void showHelp();
    void showAbout();
    void confirmReset();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    QSize pixelSize() const;
    void applySize();
    void rebuildRaster();
    void scheduleRebuild();
    void queueSave();
    void flushSave();
    void applyAlwaysOnTop();
    void syncAlwaysOnTop();
    void buildMenu();
    void placeDialog(QWidget *dialog);
    void closeSettings();
    void resetDefaults();
    void previewCenter(const QPointF &pos);
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

    QMenu *m_menu = nullptr;
    QAction *m_onTopAction = nullptr;

    QTimer *m_tick = nullptr;
    QTimer *m_saveTimer = nullptr;
    QTimer *m_rebuildTimer = nullptr;
    int m_lastSecond = -1;
};
