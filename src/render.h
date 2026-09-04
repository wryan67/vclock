// Drawing the dial furniture (hour/minute indices, hands, centre pin) and the
// appearance presets shown as thumbnails at the top of Settings.
#pragma once

#include "config.h"

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

// Draw the three hands and the centre pin for a (hour, minute, second).
void drawHands(QPainter &painter, const Config &cfg, double cx, double cy, double radius,
               double w, double h, double hours, double minutes, double seconds);

// Render a preset the way the clock would draw it.
QPixmap presetThumbnail(const Config &values, int size, qreal devicePixelRatio);

// Render the application icon at a pixel size.  It is a clock drawn by the
// code above rather than a picture of its own: the config named "icon" if
// there is one, else the "Gradient" preset.  So the icon is sharp at any size
// and is always a real vclock rather than a stale likeness of one.
QImage appIconImage(int size);
