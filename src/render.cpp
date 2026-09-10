#include "render.h"

#include "face.h"

#include <QColor>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;

// A flattering time for the preview thumbnails.
constexpr double kPresetHour = 10, kPresetMinute = 9, kPresetSecond = 23;

// Multiplies the painter's opacity for as long as it is in scope.  Multiplied
// rather than assigned so that a caller which has already faded the painter --
// a thumbnail, say -- keeps its own fade.  QPainterStateGuard would do, but it
// arrived in Qt 6.9 and vclock builds against older ones.
class PainterOpacity
{
public:
    PainterOpacity(QPainter &painter, int percent)
        : m_painter(painter), m_previous(painter.opacity())
    {
        m_painter.setOpacity(m_previous * (std::clamp(percent, 0, 100) / 100.0));
    }
    ~PainterOpacity() { m_painter.setOpacity(m_previous); }

    PainterOpacity(const PainterOpacity &) = delete;
    PainterOpacity &operator=(const PainterOpacity &) = delete;

private:
    QPainter &m_painter;
    qreal m_previous;
};

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

        // The four dial presets share everything but their two accent colours.
        // rimColor is the dial's outline (the dark tones in the artwork, hence
        // the wire colour) and bodyColor its filled centre (the light tones).
        const auto dial = [](const QString &name, const QString &tip, const QString &rimColor,
                             const QString &bodyColor, const QString &secondColor) {
            Preset p;
            p.name = name;
            p.tip = tip;
            p.values.faceSvg = kBuiltinFacePrefix + QStringLiteral("icon");
            p.values.faceDefault = false;
            p.values.faceColor = bodyColor;
            p.values.wireColor = rimColor;
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
        out.append(dial(QStringLiteral("Onyx"),
                        QStringLiteral("The gradient dial in black and silver"),
                        QStringLiteral("#c4c4c4"),   // #b0c4de with its hue taken out
                        QStringLiteral("#000000"),   // black; the artwork's own ramp still
                                                     // carries the gradient, up to the rim
                                                     // colour at its lightest
                        QStringLiteral("#ff6666")));  // the other dials answer their body
                                                      // colour with its opposite hue, which a
                                                      // neutral dial has none of; red is the
                                                      // usual second hand on a black face

        // The silver dial is the tonal opposite of the four above: a light body
        // under a dark rim, so its hands are dark and only its marks stay light.
        // Black and white map the artwork's greys straight through, which is
        // what makes it come out as drawn.
        Preset silver;
        silver.name = QStringLiteral("Silver");
        silver.tip = QStringLiteral("A brushed silver dial with a black rim and black hands");
        silver.values.faceSvg = kBuiltinFacePrefix + QStringLiteral("silver");
        silver.values.faceDefault = false;
        silver.values.wireColor = QStringLiteral("#000000");
        silver.values.faceColor = QStringLiteral("#ffffff");
        silver.values.hourColor = QStringLiteral("#000000");
        silver.values.minuteColor = QStringLiteral("#000000");
        silver.values.minuteSameAsHour = true;
        silver.values.hourMarkColor = QStringLiteral("#ffffff");   // these sit on the dark rim
        silver.values.minuteMarkColor = QStringLiteral("#ffffff");
        silver.values.secondColor = QStringLiteral("#ff6666");
        out.append(silver);

        // Honey: the wax walls take the face colour and the cells the wire
        // colour, so the pair is a light gold over a dark amber. The hands go
        // near-black rather than light, because half the face is open cell and
        // a light hand would keep disappearing into the walls.
        Preset comb;
        comb.name = QStringLiteral("Honeycomb");
        comb.tip = QStringLiteral("A honeycomb in wax and honey, with hands in amber and cream");
        comb.values.faceSvg = kBuiltinFacePrefix + QStringLiteral("honeycomb");
        comb.values.faceDefault = false;
        comb.values.wireColor = QStringLiteral("#000000");   // the walls and the rim
        comb.values.faceColor = QStringLiteral("#ffc94d");   // the honey in the cells
        comb.values.hourColor = QStringLiteral("#8f4300");
        comb.values.minuteColor = QStringLiteral("#ffc28c");
        comb.values.minuteSameAsHour = false;
        comb.values.hourMarkColor = QStringLiteral("#ffffff");
        comb.values.minuteMarkColor = QStringLiteral("#9f9f9f");
        comb.values.secondColor = QStringLiteral("#ffec8c");
        out.append(comb);

        // The kaleidoscope is the one preset whose face is different every
        // time it is applied, so what is fixed here is only the seed it starts
        // from; the settings dialog rolls a new one on each click.  Hands are
        // white and the marks are off: the artwork is already busy enough to
        // read as a dial without indices drawn over it, and against a face
        // whose colours are not known in advance white is the one hand colour
        // that stays visible over most of them.
        Preset kal;
        kal.name = QStringLiteral("Kaleidoscope");
        kal.tip = QStringLiteral("A random coloured kaleidoscope; click again for another");
        kal.values.faceSvg =
            kBuiltinFacePrefix + kKaleidoscopeFace + QStringLiteral(":1");
        kal.values.faceDefault = false;
        kal.values.faceRecolor = false;  // full colour: recolouring would flatten it
        // Both colours rolled: on this face that is what makes it a
        // kaleidoscope rather than one hue and its shades.  The pair here is
        // only what the button's thumbnail is drawn in -- clicking it rolls
        // another.
        kal.values.faceColorRandom = true;
        kal.values.wireColorRandom = true;
        kal.values.faceColor = QStringLiteral("#3aa0d8");
        kal.values.wireColor = QStringLiteral("#0b0d14");
        kal.values.markScale = 0;
        kal.values.minuteMarkScale = 0;
        kal.values.hourColor = QStringLiteral("#ffffff");
        kal.values.minuteColor = QStringLiteral("#ffffff");
        kal.values.minuteSameAsHour = true;
        kal.values.secondColor = QStringLiteral("#ffffff");
        out.append(kal);

        // The two glass dials fade the artwork rather than recolouring it, so
        // the desktop shows through and the clock reads as something sitting on
        // the glass rather than painted on it.  They are the first presets to
        // carry opacity: everything above leaves all four parts solid.
        //
        // Smoked is the Onyx dial behind darkened glass -- the same three
        // colours, faded until the wallpaper comes through the face.  Its wire
        // and face are synced, so the whole dial dims as one; the hands and
        // marks stay at roughly twice that to keep the time readable against
        // whatever is behind.
        Preset smoked = dial(QStringLiteral("Smoked glass"),
                             QStringLiteral("The Onyx dial behind dark glass, with the "
                                            "desktop showing through"),
                             QStringLiteral("#c4c4c4"), QStringLiteral("#000000"),
                             QStringLiteral("#ff6666"));
        smoked.values.faceOpacity = 26;
        smoked.values.wireOpacity = 26;
        smoked.values.syncFaceWire = true;
        smoked.values.handOpacity = 54;
        smoked.values.markOpacity = 54;
        out.append(smoked);

        // Clear goes further: the face is turned off outright (opacity 0) and
        // only the pale rim is left, so there is no dial at all -- just hands
        // and a ring of heavy marks over the wallpaper.  With nothing behind
        // them to sit on, the marks grow to 167% and the minute track is
        // dropped entirely, which is what keeps it legible rather than lost.
        Preset clear;
        clear.name = QStringLiteral("Clear glass");
        clear.tip = QStringLiteral("No dial at all: hands and heavy marks over the desktop");
        // The built-in face, kept for its rim: faceDefault and an empty faceSvg
        // are the defaults, and are set here so a preset that follows another
        // one still clears its artwork.
        clear.values.faceDefault = true;
        clear.values.faceSvg.clear();
        clear.values.faceColor = QStringLiteral("#ffffff");
        clear.values.faceOpacity = 0;      // the dial itself, gone
        clear.values.wireColor = QStringLiteral("#dfdfdf");
        clear.values.wireOpacity = 63;     // the rim, left as a faint outline
        clear.values.syncFaceWire = false;
        clear.values.hourColor = QStringLiteral("#808080");
        clear.values.minuteColor = QStringLiteral("#dfdfdf");
        clear.values.minuteSameAsHour = false;
        clear.values.secondColor = QStringLiteral("#ffffff");
        clear.values.hourMarkColor = QStringLiteral("#404040");
        clear.values.minuteMarkColor = QStringLiteral("#000000");
        clear.values.minuteMarkScale = 0;  // no minute track
        clear.values.markScale = 167;
        clear.values.markPosition = 101;
        clear.values.handOpacity = 73;
        clear.values.markOpacity = 73;
        out.append(clear);

        // Spiral: Smoked glass with the spiral in place of the gradient dial.
        // It keeps that preset's colours and its fades untouched, because the
        // spiral was drawn to the same tonal plan as the icon face -- light
        // body, dark line work -- so the same black-and-silver pair lands on it
        // the same way, and the dial shows the desktop through it exactly as
        // Smoked does.
        //
        // What changes is everything laid over the dial, and it all goes the
        // same way: off.  The spiral is already a drawing that fills the face
        // and leads the eye out to the rim, and marks set around that rim only
        // fence it in; the minute track adds sixty more ticks in the rim's own
        // weight, which reads as a second fine ring competing with the one the
        // spiral ends in, and the hours do the same more sparsely.  So neither
        // is drawn, and the dial is left to be the drawing.  Time is read off
        // two hands against the turns of the arm, which is as much as a face
        // like this is for.
        //
        // The second hand goes with them.  It is the one part that moves
        // visibly, and against a still spiral that is the only thing the eye
        // will settle on.
        //
        // The mark settings are still carried, because a preset writes every
        // key it owns and they are what the marks return to if the size is
        // turned back up: just clear of the rim, and in a silver a shade under
        // the hands so that turning them on does not put anything on the dial
        // heavier than the hands are.
        Preset spiral = smoked;
        spiral.name = QStringLiteral("Spiral");
        spiral.tip = QStringLiteral("The smoked glass dial as a spiral, with the desktop "
                                    "showing through");
        spiral.values.faceSvg = kBuiltinFacePrefix + QStringLiteral("spiral");
        spiral.values.markScale = 0;       // no hour marks
        spiral.values.markPosition = 104;
        spiral.values.minuteMarkScale = 0;  // no minute track
        spiral.values.hourMarkColor = QStringLiteral("#dfdfdf");
        spiral.values.showSecond = false;
        out.append(spiral);

        return out;
    }();
    return list;
}

namespace {

// The circle the indices hang from.  Clamped to the canvas so that a high
// index position cannot push them off the edge and slice them in half.
double markOuterEdge(const Config &cfg, double cx, double cy, double radius, double w,
                     double h)
{
    const double size = cfg.markScale / 100.0;
    const double position = cfg.markPosition / 100.0;
    const double hourWidth = kMarkWidth * size * radius;
    const double limit =
        std::max(1.0, std::min({cx, cy, w - cx, h - cy}) - hourWidth / 2.0);
    return std::min(kMarkOuter * position * radius, limit);
}

}  // namespace

double markReach(const Config &cfg, double cx, double cy, double radius, double w, double h)
{
    const double size = cfg.markScale / 100.0;
    if (size <= 0.0 || cfg.markPosition <= 0 || cfg.markOpacity <= 0)
        return 0.0;
    // Half the stroke sits outside the line the index is drawn along.
    return markOuterEdge(cfg, cx, cy, radius, w, h) + kMarkWidth * size * radius / 2.0;
}

void drawMarks(QPainter &painter, const Config &cfg, double cx, double cy, double radius,
               double w, double h)
{
    const double size = cfg.markScale / 100.0;
    const double position = cfg.markPosition / 100.0;
    const double minuteSize = cfg.minuteMarkScale / 100.0;
    if (size <= 0.0 || position <= 0.0 || cfg.markOpacity <= 0)
        return;

    // Set on the painter rather than folded into each colour so that marks
    // which overlap do not show their join through one another.
    const PainterOpacity fade(painter, cfg.markOpacity);

    // The indices hang off a common outer edge and grow inwards.  The edge is
    // clamped to the canvas so a high position cannot slice them off.
    const double hourLen = (kMarkOuter - kMarkInner) * size * radius;
    const double hourWidth = kMarkWidth * size * radius;
    const double outer = markOuterEdge(cfg, cx, cy, radius, w, h);

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
    if (cfg.handOpacity <= 0)
        return;

    const double handScale = cfg.handScale / 100.0;
    const QColor hourColor = colorOf(cfg.hourColor);

    // On the painter, so the hands fade as one piece: fading each colour
    // instead would let the hands show through one another where they cross.
    const PainterOpacity fade(painter, cfg.handOpacity);

    // Hands longer than the canvas would be sliced off by the window edge, so
    // cap each one at the room actually available around the pivot.
    const double margin = kHourWidth * radius * handScale / 2.0;
    const double reach = std::max(1.0, std::min({cx, cy, w - cx, h - cy}) - margin);
    const auto length = [&](double fraction) {
        return std::min(fraction * radius * handScale, reach);
    };
    const auto hand = [&](double angle, double len, double width, const QColor &color) {
        // Reversing mirrors each hand about the 12-6 axis, so they sweep
        // anticlockwise and read against a mirrored dial.  Only the sine flips,
        // because cos(-a) == cos(a).
        const double s = cfg.reverseTime ? -std::sin(angle) : std::sin(angle);
        strokeLine(painter, color, width, cx, cy, cx + len * s, cy - len * std::cos(angle));
    };

    hand((hours + minutes / 60.0) * kPi / 6.0, length(kHourLen),
         kHourWidth * radius * handScale, hourColor);
    hand(minutes * kPi / 30.0, length(kMinuteLen), kMinuteWidth * radius * handScale,
         colorOf(cfg.minuteHandColor()));
    if (cfg.showSecond)
        hand(seconds * kPi / 30.0, length(kSecondLen), kSecondWidth * radius * handScale,
             colorOf(cfg.secondColor));

    painter.setPen(Qt::NoPen);
    painter.setBrush(hourColor);
    const double pin = std::max(1.0, kPinRadius * radius * handScale);
    painter.drawEllipse(QPointF(cx, cy), pin, pin);
    painter.setBrush(Qt::NoBrush);
}

namespace {

// Draw a settings record the way the clock draws it, into a square image.
// Shared by the Settings thumbnails and the application icon so that neither
// can drift away from what the running clock actually looks like.
QImage drawClock(const Config &values, int pixels)
{
    const std::unique_ptr<Face> face =
        openFace(values.facePath(), values.faceColor, values.wireColor,
                 values.faceMultiHue());
    QImage art = face->render(pixels, pixels);
    if (values.faceRecolor)
        art = recolor(art, values.wireColor, values.faceColor, values.faceOpacity,
                      values.wireOpacity);

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
    return canvas;
}

// The clock the application icon is drawn from.  A config named "icon" in the
// config directory wins when there is one, so the icon can be dressed by
// editing that clock like any other; failing that the built-in "Gradient"
// preset stands in.  The preset is found by name rather than by index so that
// reordering the list cannot silently change the icon.
//
// Deliberately not cached: re-reading the file costs nothing at the handful of
// sizes the icon is ever asked for, and it means an edit to the icon clock
// shows up the next time About or Help is opened.
Config iconClock()
{
    const QString path = resolveConfigPath(QStringLiteral("icon"));
    if (QFile::exists(path))
        return loadConfig(path);
    for (const Preset &preset : presets())
        if (preset.name == QLatin1String("Gradient"))
            return preset.values;
    return presets().front().values;
}

}  // namespace

QPixmap presetThumbnail(const Config &values, int size, qreal devicePixelRatio)
{
    const qreal dpr = devicePixelRatio > 0 ? devicePixelRatio : 1.0;
    const int pixels = std::max(1, static_cast<int>(std::lround(size * dpr)));
    QImage canvas = drawClock(values, pixels);
    canvas.setDevicePixelRatio(dpr);
    return QPixmap::fromImage(canvas);
}

QImage appIconImage(int size)
{
    return drawClock(iconClock(), std::max(1, size));
}

QPixmap appIconPixmap(int size, qreal devicePixelRatio)
{
    const qreal dpr = devicePixelRatio > 0 ? devicePixelRatio : 1.0;
    const int pixels = std::max(1, static_cast<int>(std::lround(size * dpr)));
    QImage canvas = drawClock(iconClock(), pixels);
    canvas.setDevicePixelRatio(dpr);
    return QPixmap::fromImage(canvas);
}
