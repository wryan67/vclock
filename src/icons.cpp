#include "icons.h"

#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QHash>
#include <QIconEngine>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QProxyStyle>
#include <QStyleOption>
#include <QString>
#include <QSvgRenderer>

#include <algorithm>
#include <cmath>

namespace {

// Font Awesome Free 6.7.2, CC BY 4.0.  Each entry is the glyph's viewBox
// extent (they are square in the vertical axis at 512) and its path data.
struct GlyphData
{
    int width;
    const char *path;
};

GlyphData glyphData(Glyph glyph)
{
    switch (glyph) {
    case Glyph::Cancel:  // solid/square-xmark
        return {448,
                "M64 32C28.7 32 0 60.7 0 96L0 416c0 35.3 28.7 64 64 64l320 0c35.3 0 64-28.7 "
                "64-64l0-320c0-35.3-28.7-64-64-64L64 32zm79 143c9.4-9.4 24.6-9.4 33.9 0l47 "
                "47 47-47c9.4-9.4 24.6-9.4 33.9 0s9.4 24.6 0 33.9l-47 47 47 47c9.4 9.4 9.4 "
                "24.6 0 33.9s-24.6 9.4-33.9 0l-47-47-47 47c-9.4 9.4-24.6 9.4-33.9 "
                "0s-9.4-24.6 0-33.9l47-47-47-47c-9.4-9.4-9.4-24.6 0-33.9z"};
    case Glyph::Save:  // regular/floppy-disk
        return {448,
                "M48 96l0 320c0 8.8 7.2 16 16 16l320 0c8.8 0 16-7.2 "
                "16-16l0-245.5c0-4.2-1.7-8.3-4.7-11.3l33.9-33.9c12 12 18.7 28.3 18.7 "
                "45.3L448 416c0 35.3-28.7 64-64 64L64 480c-35.3 0-64-28.7-64-64L0 96C0 60.7 "
                "28.7 32 64 32l245.5 0c17 0 33.3 6.7 45.3 18.7l74.5 74.5-33.9 "
                "33.9L320.8 84.7c-.3-.3-.5-.5-.8-.8L320 184c0 13.3-10.7 24-24 24l-192 "
                "0c-13.3 0-24-10.7-24-24L80 80 64 80c-8.8 0-16 7.2-16 16zm80-16l0 80 144 0 "
                "0-80L128 80zm32 240a64 64 0 1 1 128 0 64 64 0 1 1 -128 0z"};
    case Glyph::Edit:  // regular/pen-to-square
        return {512,
                "M441 58.9L453.1 71c9.4 9.4 9.4 24.6 0 33.9L424 134.1 377.9 88 407 "
                "58.9c9.4-9.4 24.6-9.4 33.9 0zM209.8 256.2L344 121.9 390.1 168 255.8 "
                "302.2c-2.9 2.9-6.5 5-10.4 6.1l-58.5 16.7 16.7-58.5c1.1-3.9 3.2-7.5 6.1-10.4z"
                "M373.1 25L175.8 222.2c-8.7 8.7-15 19.4-18.3 31.1l-28.6 100c-2.4 8.4-.1 17.4 "
                "6.1 23.6s15.2 8.5 23.6 6.1l100-28.6c11.8-3.4 22.5-9.7 31.1-18.3L487 "
                "138.9c28.1-28.1 28.1-73.7 0-101.8L474.9 25C446.8-3.1 401.2-3.1 373.1 25zM88 "
                "64C39.4 64 0 103.4 0 152L0 424c0 48.6 39.4 88 88 88l272 0c48.6 0 88-39.4 "
                "88-88l0-112c0-13.3-10.7-24-24-24s-24 10.7-24 24l0 112c0 22.1-17.9 40-40 "
                "40L88 464c-22.1 0-40-17.9-40-40l0-272c0-22.1 17.9-40 40-40l112 0c13.3 0 "
                "24-10.7 24-24s-10.7-24-24-24L88 64z"};
    case Glyph::New:  // solid/square-plus
        return {448,
                "M64 32C28.7 32 0 60.7 0 96L0 416c0 35.3 28.7 64 64 64l320 0c35.3 0 "
                "64-28.7 64-64l0-320c0-35.3-28.7-64-64-64L64 32zM200 344l0-64-64 0c-13.3 "
                "0-24-10.7-24-24s10.7-24 24-24l64 0 0-64c0-13.3 10.7-24 24-24s24 10.7 24 "
                "24l0 64 64 0c13.3 0 24 10.7 24 24s-10.7 24-24 24l-64 0 0 64c0 13.3-10.7 "
                "24-24 24s-24-10.7-24-24z"};
    case Glyph::Delete:  // regular/trash-can
        return {448,
                "M170.5 51.6L151.5 80l145 0-19-28.4c-1.5-2.2-4-3.6-6.7-3.6l-93.7 0c-2.7 "
                "0-5.2 1.3-6.7 3.6zm147-26.6L354.2 80 368 80l48 0 8 0c13.3 0 24 10.7 24 "
                "24s-10.7 24-24 24l-8 0 0 304c0 44.2-35.8 80-80 80l-224 0c-44.2 0-80-35.8-80"
                "-80l0-304-8 0c-13.3 0-24-10.7-24-24S10.7 80 24 80l8 0 48 0 13.8 0 "
                "36.7-55.1C140.9 9.4 158.4 0 177.1 0l93.7 0c18.7 0 36.2 9.4 46.6 24.9zM80 "
                "128l0 304c0 17.7 14.3 32 32 32l224 0c17.7 0 32-14.3 32-32l0-304L80 128zm80 "
                "64l0 208c0 8.8-7.2 16-16 16s-16-7.2-16-16l0-208c0-8.8 7.2-16 16-16s16 7.2 "
                "16 16zm80 0l0 208c0 8.8-7.2 16-16 16s-16-7.2-16-16l0-208c0-8.8 7.2-16 "
                "16-16s16 7.2 16 16zm80 0l0 208c0 8.8-7.2 16-16 16s-16-7.2-16-16l0-208c0-8.8 "
                "7.2-16 16-16s16 7.2 16 16z"};
    case Glyph::Settings:  // solid/gear
        return {512,
                "M495.9 166.6c3.2 8.7 .5 18.4-6.4 24.6l-43.3 39.4c1.1 8.3 1.7 16.8 1.7 "
                "25.4s-.6 17.1-1.7 25.4l43.3 39.4c6.9 6.2 9.6 15.9 6.4 24.6c-4.4 11.9-9.7 "
                "23.3-15.8 34.3l-4.7 8.1c-6.6 11-14 21.4-22.1 31.2c-5.9 7.2-15.7 9.6-24.5 "
                "6.8l-55.7-17.7c-13.4 10.3-28.2 18.9-44 25.4l-12.5 57.1c-2 9.1-9 16.3-18.2 "
                "17.8c-13.8 2.3-28 3.5-42.5 3.5s-28.7-1.2-42.5-3.5c-9.2-1.5-16.2-8.7-18.2"
                "-17.8l-12.5-57.1c-15.8-6.5-30.6-15.1-44-25.4L83.1 425.9c-8.8 2.8-18.6 .3"
                "-24.5-6.8c-8.1-9.8-15.5-20.2-22.1-31.2l-4.7-8.1c-6.1-11-11.4-22.4-15.8"
                "-34.3c-3.2-8.7-.5-18.4 6.4-24.6l43.3-39.4C64.6 273.1 64 264.6 64 256s.6"
                "-17.1 1.7-25.4L22.4 191.2c-6.9-6.2-9.6-15.9-6.4-24.6c4.4-11.9 9.7-23.3 "
                "15.8-34.3l4.7-8.1c6.6-11 14-21.4 22.1-31.2c5.9-7.2 15.7-9.6 24.5-6.8l55.7 "
                "17.7c13.4-10.3 28.2-18.9 44-25.4l12.5-57.1c2-9.1 9-16.3 18.2-17.8C227.3 "
                "1.2 241.5 0 256 0s28.7 1.2 42.5 3.5c9.2 1.5 16.2 8.7 18.2 17.8l12.5 "
                "57.1c15.8 6.5 30.6 15.1 44 25.4l55.7-17.7c8.8-2.8 18.6-.3 24.5 6.8c8.1 "
                "9.8 15.5 20.2 22.1 31.2l4.7 8.1c6.1 11 11.4 22.4 15.8 34.3zM256 336a80 80 "
                "0 1 0 0-160 80 80 0 1 0 0 160z"};
    case Glyph::Checked:  // solid/check
        return {448,
                "M438.6 105.4c12.5 12.5 12.5 32.8 0 45.3l-256 256c-12.5 12.5-32.8 12.5"
                "-45.3 0l-128-128c-12.5-12.5-12.5-32.8 0-45.3s32.8-12.5 45.3 0L160 338.7 "
                "393.4 105.4c12.5-12.5 32.8-12.5 45.3 0z"};
    case Glyph::Unchecked:  // regular/square
        return {448,
                "M384 80c8.8 0 16 7.2 16 16l0 320c0 8.8-7.2 16-16 16L64 432c-8.8 "
                "0-16-7.2-16-16L48 96c0-8.8 7.2-16 16-16l320 0zM64 32C28.7 32 0 60.7 0 "
                "96L0 416c0 35.3 28.7 64 64 64l320 0c35.3 0 64-28.7 64-64l0-320c0-35.3"
                "-28.7-64-64-64L64 32z"};
    }
    return {512, ""};
}

// Font Awesome glyphs are drawn right out to the edges of their viewBox, so a
// glyph fitted exactly to its box has ink on the outermost row of pixels.  At
// a fractional device pixel ratio -- 1.5625 on a display scaled to 150%, say --
// that row lands on a partial pixel and gets shaved off, which is why the pen
// lost its tip and the bin its lid.  A small margin keeps the ink clear of the
// boundary at any scale.
constexpr double kInset = 0.08;

// Renders the glyph at whatever size is asked for rather than scaling a fixed
// pixmap, so the icons stay sharp on a high-DPI screen and at any button size.
class GlyphEngine : public QIconEngine
{
public:
    // Two glyphs, one for each state.  They are the same for an icon that does
    // not change with state, which is most of them.
    GlyphEngine(Glyph off, Glyph on, GlyphRole role) : m_off(off), m_on(on), m_role(role) {}

    QIconEngine *clone() const override { return new GlyphEngine(m_off, m_on, m_role); }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode,
               QIcon::State state) override
    {
        const GlyphData data = glyphData(state == QIcon::On ? m_on : m_off);
        if (!data.path[0] || rect.isEmpty())
            return;

        QByteArray svg;
        svg.reserve(2048);
        svg += "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 ";
        svg += QByteArray::number(data.width);
        svg += " 512\"><path fill=\"";
        // Resolved here rather than when the icon was made, so an icon already
        // on a button follows a theme that changes under it.
        svg += glyphColor(m_role).name(QColor::HexRgb).toLatin1();
        svg += "\" d=\"";
        svg += data.path;
        svg += "\"/></svg>";

        QSvgRenderer renderer(svg);
        if (!renderer.isValid())
            return;

        // Fit the glyph inside the box the caller gave us, keeping its aspect
        // so the narrower glyphs are not stretched, and holding it off the
        // edges so nothing is lost to rounding.
        const double avail = std::min(rect.width(), rect.height()) * (1.0 - 2.0 * kInset);
        const double scale = std::min(avail / double(data.width), avail / 512.0);
        const QSizeF drawn(data.width * scale, 512.0 * scale);
        const QRectF target(rect.x() + (rect.width() - drawn.width()) / 2.0,
                            rect.y() + (rect.height() - drawn.height()) / 2.0,
                            drawn.width(), drawn.height());

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        if (mode == QIcon::Disabled)
            painter->setOpacity(painter->opacity() * 0.35);
        renderer.render(painter, target);
        painter->restore();
    }

    // Qt asks for an exact number of pixels and lays the result out itself,
    // having already folded the display's scaling into the size.  Returning
    // anything other than exactly that many pixels makes Qt rescale the result
    // to fit, which is what was blurring these glyphs and clipping their far
    // edge on a display scaled to 150%.  scaledPixmap() is deliberately left
    // alone: its default hands straight back to here.
    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        if (size.isEmpty())
            return QPixmap();
        QPixmap pm(size);
        pm.fill(Qt::transparent);
        QPainter painter(&pm);
        painter.setRenderHint(QPainter::Antialiasing, true);
        paint(&painter, QRect(QPoint(0, 0), size), mode, state);
        painter.end();
        return pm;
    }

private:
    Glyph m_off;
    Glyph m_on;
    GlyphRole m_role;
};

}  // namespace

bool darkTheme()
{
    if (!qApp)
        return false;
    // The window background is what these icons are seen against, so it, and
    // not the text colour, is what decides which way round they should go.
    return qApp->palette().color(QPalette::Window).lightness() < 128;
}

QColor glyphColor(GlyphRole role)
{
    const bool dark = darkTheme();
    switch (role) {
    case GlyphRole::Neutral:
        // Off-white rather than white on a dark theme: a pure white glyph
        // beside ordinary label text reads as brighter than the text it sits
        // with, and pulls the eye away from the name it belongs to.
        return dark ? QColor(0xdc, 0xe0, 0xe4) : QColor(0x42, 0x4a, 0x53);
    case GlyphRole::Go:
        // The mid-tone greens and reds that read well on a light background
        // go muddy on a dark one, so each theme gets its own.
        return dark ? QColor(0x66, 0xbb, 0x6a) : QColor(0x2e, 0x7d, 0x32);
    case GlyphRole::Stop:
        return dark ? QColor(0xef, 0x53, 0x50) : QColor(0xc6, 0x28, 0x28);
    }
    return Qt::black;
}

namespace {

// The engine resolves its colour as it paints, so one icon per glyph pair and
// role serves every theme.
QIcon cachedIcon(Glyph off, Glyph on, GlyphRole role)
{
    static QHash<QString, QIcon> cache;
    const QString key = QStringLiteral("%1/%2/%3")
                            .arg(int(off))
                            .arg(int(on))
                            .arg(int(role));
    auto it = cache.constFind(key);
    if (it != cache.constEnd())
        return it.value();
    const QIcon icon(new GlyphEngine(off, on, role));
    cache.insert(key, icon);
    return icon;
}

}  // namespace

QIcon glyphIcon(Glyph glyph, GlyphRole role)
{
    return cachedIcon(glyph, glyph, role);
}

QIcon checkIcon(GlyphRole role)
{
    return cachedIcon(Glyph::Unchecked, Glyph::Checked, role);
}

namespace {

// Only the check indicator is taken over; everything else is left to whatever
// style the desktop is running.
class GlyphStyle : public QProxyStyle
{
public:
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                       const QWidget *widget) const override
    {
        if (element == PE_IndicatorCheckBox || element == PE_IndicatorItemViewItemCheck) {
            const bool on = option->state & State_On;
            const bool partial = option->state & State_NoChange;
            // A tick when set, an empty box when not.  A tristate box in its
            // middle state keeps the box, since there is no third glyph.
            const Glyph glyph = (on || partial) ? Glyph::Checked : Glyph::Unchecked;
            QIcon::Mode mode =
                (option->state & State_Enabled) ? QIcon::Normal : QIcon::Disabled;
            glyphIcon(glyph, GlyphRole::Neutral)
                .paint(painter, option->rect, Qt::AlignCenter, mode);
            return;
        }
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }

    int pixelMetric(PixelMetric metric, const QStyleOption *option,
                    const QWidget *widget) const override
    {
        // The tick has no box around it, so at the theme's indicator size it
        // reads smaller than the box it replaces; a little more evens them up.
        if (metric == PM_IndicatorWidth || metric == PM_IndicatorHeight)
            return QProxyStyle::pixelMetric(metric, option, widget) + 2;
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
};

}  // namespace

void installGlyphStyle()
{
    QApplication::setStyle(new GlyphStyle);
}

// Sliders and other highlighted controls are drawn by the platform style in
// whatever colour the desktop nominates: the theme's highlight on Linux, and on
// Windows the system accent colour, which the Windows 11 style paints slider
// grooves with directly.  That makes the same dialog a different colour on
// every machine, and red on one whose owner picked red for the taskbar.  The
// clock is a piece of decoration whose whole point is how it looks, so it
// settles this itself rather than inheriting an unrelated choice.  The colour
// is Fusion's default highlight, which is what the Linux build has always
// shown.
void installAccentColour()
{
    const QColor accent(0x30, 0x8c, 0xc6);

    QPalette palette = QApplication::palette();
    palette.setColor(QPalette::Highlight, accent);
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    // The role the Windows 11 style actually reads; it does not exist before
    // 6.6, where Highlight alone covers it.
    palette.setColor(QPalette::Accent, accent);
#endif
    QApplication::setPalette(palette);
}
