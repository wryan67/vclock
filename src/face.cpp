#include "face.h"

#include "config.h"
#include "embedded.h"

#include <QColor>
#include <QFileInfo>
#include <QPainter>
#include <QRegularExpression>
#include <QSvgRenderer>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

// Faces that live inside the program rather than on disk.
//
// A name may carry an argument after a colon, which is how the kaleidoscope
// keeps its seed: "kaleidoscope:12345" is one particular random face, and the
// same digits give the same drawing every time.
// Make the face colour the one the dial is mostly painted in.
//
// A many-hued kaleidoscope has no single colour, but the swatch beside it has
// to show something, and the honest answer is whichever colour covers most of
// the dial.  Rather than read that colour off the picture and put it in the
// swatch -- which would feed straight back into the next drawing and never
// settle -- the picture is changed to agree with the swatch: the colour that
// wins the most ground is replaced by the chosen one everywhere it appears.
// So the colour goes in, and the same colour comes back out as the one a
// glance would name.
//
// Only fills are counted.  The line work is stroked, and it has a swatch of
// its own already.
QByteArray anchorFaceColor(const QByteArray &svg, const QString &faceHex)
{
    const QColor face(faceHex);
    if (!face.isValid())
        return svg;

    QStringList candidates;
    static const QRegularExpression fill(QStringLiteral("fill=\"(#[0-9a-fA-F]{6})\""));
    auto it = fill.globalMatch(QString::fromUtf8(svg));
    while (it.hasNext()) {
        const QString hex = it.next().captured(1).toLower();
        if (!candidates.contains(hex))
            candidates.append(hex);
    }
    if (candidates.size() < 2)
        return svg;

    QSvgRenderer renderer(svg);
    if (!renderer.isValid())
        return svg;
    // Small enough to be cheap, large enough that the shares come out in the
    // same order they would at any size.
    QImage shot(160, 160, QImage::Format_ARGB32_Premultiplied);
    shot.fill(Qt::transparent);
    {
        QPainter painter(&shot);
        renderer.render(&painter, QRectF(0, 0, shot.width(), shot.height()));
    }

    QVector<int> counts(candidates.size(), 0);
    QVector<QRgb> keys;
    keys.reserve(candidates.size());
    for (const QString &hex : candidates)
        keys.append(QColor(hex).rgb() & 0xffffff);
    for (int y = 0; y < shot.height(); ++y) {
        const QRgb *row = reinterpret_cast<const QRgb *>(shot.constScanLine(y));
        for (int x = 0; x < shot.width(); ++x) {
            if (qAlpha(row[x]) < 250)
                continue;
            const int at = keys.indexOf(row[x] & 0xffffff);
            if (at >= 0)
                ++counts[at];
        }
    }

    int winner = 0;
    for (int i = 1; i < counts.size(); ++i)
        if (counts[i] > counts[winner])
            winner = i;
    if (counts[winner] == 0)
        return svg;

    QByteArray out = svg;
    out.replace(QStringLiteral("fill=\"%1\"").arg(candidates[winner]).toUtf8(),
                QStringLiteral("fill=\"%1\"").arg(face.name()).toUtf8());
    return out;
}

QByteArray builtinFaceData(const QString &name, const QString &faceHex = QString(),
                           const QString &wireHex = QString(), bool multiHue = false)
{
    if (name == QLatin1String("icon"))
        return iconFaceSvg();
    if (name == QLatin1String("silver"))
        return silverFaceSvg();
    if (name == QLatin1String("honeycomb"))
        return honeycombFaceSvg();
    if (name == QLatin1String("spiral"))
        return spiralFaceSvg();
    if (name.startsWith(kKaleidoscopeFace + QLatin1Char(':'))) {
        const QByteArray art =
            kaleidoscopeFaceSvg(name.mid(kKaleidoscopeFace.size() + 1).toULongLong(), faceHex,
                                wireHex, multiHue);
        // Only worth doing when the palette is many hues.  A single-hue face is
        // already shades of the chosen colour, and a face drawn in grey for
        // recolouring must keep its greys or the ramp loses its middle.
        return multiHue ? anchorFaceColor(art, faceHex) : art;
    }
    return QByteArray();
}

QString builtinFaceLabel(const QString &name)
{
    if (name == QLatin1String("icon"))
        return QStringLiteral("gradient dial");
    if (name == QLatin1String("silver"))
        return QStringLiteral("silver dial");
    if (name == QLatin1String("honeycomb"))
        return QStringLiteral("honeycomb");
    if (name == QLatin1String("spiral"))
        return QStringLiteral("spiral dial");
    // The seed is part of the name but not worth reading out, so the label
    // says which face it is and leaves the digits in the config.
    if (name.startsWith(kKaleidoscopeFace + QLatin1Char(':')))
        return QStringLiteral("kaleidoscope");
    return name;
}

}  // namespace

Face::Face(const QString &path, const QString &faceHex, const QString &wireHex, bool multiHue)
    : m_path(path)
{
    if (!m_path.isEmpty() && m_path.startsWith(kBuiltinFacePrefix)) {
        const QString name = m_path.mid(kBuiltinFacePrefix.size());
        if (!builtinFaceData(name).isEmpty()) {
            m_builtin = name;
        } else {
            qWarning("WARNING: unknown built-in face %s; using the default",
                     qPrintable(name));
            m_path.clear();
        }
    }

    m_renderer = std::make_unique<QSvgRenderer>();
    bool ok = false;
    if (!m_builtin.isEmpty()) {
        ok = m_renderer->load(builtinFaceData(m_builtin, faceHex, wireHex, multiHue));
    } else if (!m_path.isEmpty()) {
        ok = m_renderer->load(m_path);
    } else {
        ok = m_renderer->load(defaultFaceSvg());
    }
    if (!ok) {
        // A caller that wanted a file gets told about it by openFace(); the
        // embedded artwork is used so there is always something to draw.
        m_renderer->load(defaultFaceSvg());
        if (!m_path.isEmpty())
            throw std::runtime_error("could not load face");
    }
    loadAspect();
}

Face::~Face() = default;

QString Face::label() const
{
    if (m_path.isEmpty())
        return kDefaultFaceLabel;
    if (!m_builtin.isEmpty())
        return builtinFaceLabel(m_builtin);
    return QFileInfo(m_path).fileName();
}

// Height / width of the artwork, from its viewBox or intrinsic size.
void Face::loadAspect()
{
    const QRectF box = m_renderer->viewBoxF();
    if (box.width() > 0.0 && box.height() > 0.0) {
        m_aspect = box.height() / box.width();
        return;
    }
    const QSize size = m_renderer->defaultSize();
    if (size.width() > 0 && size.height() > 0) {
        m_aspect = static_cast<double>(size.height()) / size.width();
        return;
    }
    m_aspect = 1.0;
}

QImage Face::render(int width, int height) const
{
    width = std::max(1, width);
    height = std::max(1, height);
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        m_renderer->render(&painter, QRectF(0, 0, width, height));
    }
    // The recolour works on straight alpha, so undo the premultiplication once
    // here rather than per pixel later.
    return image.convertToFormat(QImage::Format_ARGB32);
}

std::unique_ptr<Face> openFace(const QString &path, const QString &faceHex,
                               const QString &wireHex, bool multiHue)
{
    if (!path.isEmpty()) {
        try {
            return std::make_unique<Face>(path, faceHex, wireHex, multiHue);
        } catch (const std::exception &) {
            qWarning("WARNING: could not load face %s", qPrintable(path));
        }
    }
    return std::make_unique<Face>(QString());
}

// facePercent/wirePercent fade the two ends of the ramp independently, so the
// body of the drawing can wash out while the lines over it stay solid, or the
// other way about.  A face at 0 leaves bare wire over the desktop, which is
// what the old "transparent" tick did.
QImage recolor(const QImage &art, const QString &wireHex, const QString &faceHex,
               int facePercent, int wirePercent)
{
    QImage src = art;
    if (src.format() != QImage::Format_ARGB32)
        src = src.convertToFormat(QImage::Format_ARGB32);

    const QColor wire(wireHex);
    const QColor face(faceHex);
    const int wireR = wire.isValid() ? wire.red() : 0;
    const int wireG = wire.isValid() ? wire.green() : 0;
    const int wireB = wire.isValid() ? wire.blue() : 0;
    const int faceR = face.isValid() ? face.red() : 255;
    const int faceG = face.isValid() ? face.green() : 255;
    const int faceB = face.isValid() ? face.blue() : 255;

    const double faceAlpha = std::clamp(facePercent, 0, 100) / 100.0;
    const double wireAlpha = std::clamp(wirePercent, 0, 100) / 100.0;

    // lut[c][t]: the output channel for a source brightness of t, with lut[3]
    // holding the fraction of the pixel's own alpha that survives.
    quint8 lut[3][256];
    double alphaLut[256];
    for (int t = 0; t < 256; ++t) {
        const double ramp = t / 255.0;
        lut[0][t] = static_cast<quint8>(std::lround(wireR * (1.0 - ramp) + faceR * ramp));
        lut[1][t] = static_cast<quint8>(std::lround(wireG * (1.0 - ramp) + faceG * ramp));
        lut[2][t] = static_cast<quint8>(std::lround(wireB * (1.0 - ramp) + faceB * ramp));
        alphaLut[t] = wireAlpha * (1.0 - ramp) + faceAlpha * ramp;
    }

    const int w = src.width();
    const int h = src.height();
    QImage out(w, h, QImage::Format_ARGB32);
    for (int y = 0; y < h; ++y) {
        const QRgb *in = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        QRgb *dst = reinterpret_cast<QRgb *>(out.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const QRgb p = in[x];
            const int alpha = qAlpha(p);
            // t = 0 for pure black (wire), 255 for pure white (face body)
            const int t = std::max({qRed(p), qGreen(p), qBlue(p)});
            const int a = static_cast<int>(std::lround(alpha * alphaLut[t]));
            dst[x] = qRgba(lut[0][t], lut[1][t], lut[2][t], std::clamp(a, 0, 255));
        }
    }
    return out;
}

QRect contentBounds(const QImage &art)
{
    QImage src = art;
    if (src.format() != QImage::Format_ARGB32)
        src = src.convertToFormat(QImage::Format_ARGB32);

    const int w = src.width();
    const int h = src.height();
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (int y = 0; y < h; ++y) {
        const QRgb *in = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            if (qAlpha(in[x]) > 32) {
                if (x < x0)
                    x0 = x;
                if (x > x1)
                    x1 = x;
                if (y < y0)
                    y0 = y;
                y1 = y;
            }
        }
    }
    if (x1 < 0 || y1 < 0)
        return QRect(0, 0, w, h).adjusted(0, 0, -1, -1);
    return QRect(QPoint(x0, y0), QPoint(x1, y1));
}

// The distance from a pivot to the farthest pixel the artwork actually paints.
// Turned about that pivot, the artwork sweeps a disc of exactly this radius,
// so this is the shape a spinning face presents to the pointer.  The content
// box will not do here: half the diagonal of a box is a good deal wider than a
// round face inside it, and a clock that took clicks in its empty corners
// would be no better than an undecorated rectangle.
double farthestCovered(const QImage &art, const QPointF &pivot)
{
    QImage src = art;
    if (src.format() != QImage::Format_ARGB32)
        src = src.convertToFormat(QImage::Format_ARGB32);

    const int w = src.width();
    const int h = src.height();
    double worst = 0.0;
    for (int y = 0; y < h; ++y) {
        const QRgb *in = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        // Only the first and last painted pixel on a row can be the farthest
        // from the pivot, once the row is fixed: distance grows away from the
        // pivot's column in both directions.  That keeps this to two probes a
        // row instead of a full scan of the image.
        int x0 = -1, x1 = -1;
        for (int x = 0; x < w; ++x) {
            if (qAlpha(in[x]) > 32) {
                if (x0 < 0)
                    x0 = x;
                x1 = x;
            }
        }
        if (x0 < 0)
            continue;
        const double dy = y - pivot.y();
        worst = std::max({worst, std::hypot(x0 - pivot.x(), dy),
                          std::hypot(x1 - pivot.x(), dy)});
    }
    return worst;
}

