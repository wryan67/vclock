#include "render.h"

#include "face.h"

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;

// A flattering time for the preview thumbnails.
constexpr double kPresetHour = 10, kPresetMinute = 9, kPresetSecond = 30;

QColor colorOf(const QString &hex)
{
    QColor c(hex);
    return c.isValid() ? c : QColor(Qt::black);
}

void strokeLine(QPainter &painter, const QColor &color, double width, double x1, double y1,
                double x2, double y2)
{
    QPen pen(color);
    pen.setWidthF(std::max(1.0, width));
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
}

}  // namespace

const QVector<Preset> &presets()
{
    static const QVector<Preset> list = [] {
        QVector<Preset> out;

        Preset classic;
        classic.name = QStringLiteral("Classic");
        classic.tip = QStringLiteral("The built-in face with the default hands and colours");
        out.append(classic);

        // The three dial presets share everything but their two accent colours.
        const auto dial = [](const QString &name, const QString &tip, const QString &faceColor,
                             const QString &wireColor, const QString &secondColor) {
            Preset p;
            p.name = name;
            p.tip = tip;
            p.values.faceSvg = kBuiltinFacePrefix + QStringLiteral("icon");
            p.values.faceDefault = false;
            p.values.faceTransparent = false;
            p.values.faceColor = faceColor;
            p.values.wireColor = wireColor;
            p.values.hourColor = QStringLiteral("#f0f0f0");  // the icon's hands and ticks
            p.values.minuteColor = QStringLiteral("#f0f0f0");
            p.values.minuteSameAsHour = true;
            p.values.secondColor = secondColor;
            p.values.hourMarkColor = QStringLiteral("#f0f0f0");
            p.values.minuteMarkColor = QStringLiteral("#f0f0f0");
            return p;
        };

        out.append(dial(QStringLiteral("Gradient"),
                        QStringLiteral("The app icon's dial, with its own line colours"),
                        QStringLiteral("#b0c4de"),   // the icon's rim
                        QStringLiteral("#00008b"),   // the dark end of its gradient
                        QStringLiteral("#ffd166")));
        out.append(dial(QStringLiteral("Ember"),
                        QStringLiteral("The gradient dial with its blues swapped to reds"),
                        QStringLiteral("#dec4b0"),   // #b0c4de with red and blue swapped
                        QStringLiteral("#8b0000"),   // #00008b likewise
                        QStringLiteral("#66d1ff")));
        out.append(dial(QStringLiteral("Emerald"),
                        QStringLiteral("The gradient dial with its blues swapped to greens"),
                        QStringLiteral("#b0dec4"),   // #b0c4de with green and blue swapped
                        QStringLiteral("#006400"),   // darkgreen reads brighter than navy, so
                                                     // it goes darker to match the other dials
                        QStringLiteral("#d166ff")));
        return out;
    }();
    return list;
}

void drawMarks(QPainter &painter, const Config &cfg, double cx, double cy, double radius,
               double w, double h)
{
    const double size = cfg.markScale / 100.0;
    const double position = cfg.markPosition / 100.0;
    const double minuteSize = cfg.minuteMarkScale / 100.0;
    if (size <= 0.0 || position <= 0.0)
        return;

    // The indices hang off a common outer edge and grow inwards.  The edge is
    // clamped to the canvas so a high position cannot slice them off.
    const double hourLen = (kMarkOuter - kMarkInner) * size * radius;
    const double hourWidth = kMarkWidth * size * radius;
    const double limit =
        std::max(1.0, std::min({cx, cy, w - cx, h - cy}) - hourWidth / 2.0);
    const double outer = std::min(kMarkOuter * position * radius, limit);

    // Quarter mode promotes only 12/3/6/9 to full indices; the hours it drops
    // fall back to the minute track rather than vanishing.
    const int step = cfg.quarterMarksOnly ? 3 : 1;
    QSet<int> hours;
    for (int i = 0; i < 12; i += step)
        hours.insert(i * 5);

    const QColor hourColor = colorOf(cfg.hourMarkColor);
    const QColor minuteColor = colorOf(cfg.minuteMarkColor);
    for (int minute = 0; minute < 60; ++minute) {
        double length = 0.0;
        double width = 0.0;
        QColor color;
        if (hours.contains(minute)) {
            length = hourLen;
            width = hourWidth;
            color = hourColor;
        } else if (minuteSize > 0.0) {
            length = hourLen * minuteSize;
            width = hourWidth * minuteSize;
            color = minuteColor;
        } else {
            continue;
        }
        const double inner = std::max(0.0, outer - length);
        const double angle = minute * kPi / 30.0;
        const double s = std::sin(angle);
        const double c = std::cos(angle);
        strokeLine(painter, color, width, cx + inner * s, cy - inner * c, cx + outer * s,
                   cy - outer * c);
    }
}

void drawHands(QPainter &painter, const Config &cfg, double cx, double cy, double radius,
               double w, double h, double hours, double minutes, double seconds)
{
    const double handScale = cfg.handScale / 100.0;
    const QColor hourColor = colorOf(cfg.hourColor);

    // Hands longer than the canvas would be sliced off by the window edge, so
    // cap each one at the room actually available around the pivot.
    const double margin = kHourWidth * radius * handScale / 2.0;
    const double reach = std::max(1.0, std::min({cx, cy, w - cx, h - cy}) - margin);
    const auto length = [&](double fraction) {
        return std::min(fraction * radius * handScale, reach);
    };
    const auto hand = [&](double angle, double len, double width, const QColor &color) {
        strokeLine(painter, color, width, cx, cy, cx + len * std::sin(angle),
                   cy - len * std::cos(angle));
    };

    hand((hours + minutes / 60.0) * kPi / 6.0, length(kHourLen),
         kHourWidth * radius * handScale, hourColor);
    hand(minutes * kPi / 30.0, length(kMinuteLen), kMinuteWidth * radius * handScale,
         colorOf(cfg.minuteHandColor()));
    hand(seconds * kPi / 30.0, length(kSecondLen), kSecondWidth * radius * handScale,
         colorOf(cfg.secondColor));

    painter.setPen(Qt::NoPen);
    painter.setBrush(hourColor);
    const double pin = std::max(1.0, kPinRadius * radius * handScale);
    painter.drawEllipse(QPointF(cx, cy), pin, pin);
    painter.setBrush(Qt::NoBrush);
}

QPixmap presetThumbnail(const Config &values, int size, qreal devicePixelRatio)
{
    const qreal dpr = devicePixelRatio > 0 ? devicePixelRatio : 1.0;
    const int pixels = std::max(1, static_cast<int>(std::lround(size * dpr)));

    const std::unique_ptr<Face> face = openFace(values.facePath());
    QImage art = face->render(pixels, pixels);
    art = recolor(art, values.wireColor, values.faceColor, values.faceTransparent);

    QImage canvas(pixels, pixels, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);
    {
        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.drawImage(0, 0, art);

        // Same radius rule the clock uses: the hands span the artwork's content.
        const QRect bounds = contentBounds(art);
        const double cx = pixels / 2.0;
        const double cy = pixels / 2.0;
        double reach = std::min({cx - bounds.left(), bounds.right() - cx, cy - bounds.top(),
                                 bounds.bottom() - cy});
        if (reach <= 0.0)
            reach = cx;
        const double radius = std::max(1.0, reach * kHandSpan);

        drawMarks(painter, values, cx, cy, radius, pixels, pixels);
        drawHands(painter, values, cx, cy, radius, pixels, pixels, kPresetHour, kPresetMinute,
                  kPresetSecond);
    }
    canvas.setDevicePixelRatio(dpr);
    return QPixmap::fromImage(canvas);
}
