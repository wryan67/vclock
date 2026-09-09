// Drawing the dial furniture (hour/minute indices, hands, centre pin) and the
// appearance presets shown as thumbnails at the top of Settings.
#pragma once

#include "config.h"

#include <QColor>
#include <QImage>
#include <QPixmap>
#include <QString>
#include <QVector>

class QPainter;

struct Preset
{
    QString name;
    QString tip;
    Config values;  // a complete settings record: defaults plus its overrides
};

const QVector<Preset> &presets();

inline constexpr int kPresetThumb = 58;  // thumbnail size in px

// Draw the hour indices and the finer minute track around the dial.
void drawMarks(QPainter &painter, const Config &cfg, double cx, double cy, double radius,
               double w, double h);

// How far from the pivot the indices reach, edge of the stroke included, or 0
// when none are drawn.  Shared with drawMarks so that anything measuring the
// dial cannot drift from what is actually put on it.
double markReach(const Config &cfg, double cx, double cy, double radius, double w, double h);

// Draw the three hands and the centre pin for a (hour, minute, second).
void drawHands(QPainter &painter, const Config &cfg, double cx, double cy, double radius,
               double w, double h, double hours, double minutes, double seconds);

// Render a preset the way the clock would draw it.
// A preview of one preset.  A see-through clock is drawn on a checkerboard of
// the two given colours so that faded looks faded rather than solid; the caller
// picks them from its palette, which is why they come in rather than being
// worked out here.  Fully opaque presets ignore them.
QPixmap presetThumbnail(const Config &values, int size, qreal devicePixelRatio,
                        const QColor &checkerLight, const QColor &checkerDark);

// Render the application icon at a pixel size.  It is a clock drawn by the
// code above rather than a picture of its own: the config named "icon" if
// there is one, else the "Gradient" preset.  So the icon is sharp at any size
// and is always a real vclock rather than a stale likeness of one.
QImage appIconImage(int size);

// The same icon at a point size, drawn at the display's true pixel density
// so that it is not scaled up afterwards.  Use this anywhere the icon is put
// on screen at a known size; appIconImage() is for QIcon, which asks in
// device pixels itself.
QPixmap appIconPixmap(int size, qreal devicePixelRatio);
