#include "face.h"

#include "config.h"
#include "embedded.h"

#include <QColor>
#include <QFileInfo>
#include <QPainter>
#include <QSvgRenderer>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

// Faces that live inside the program rather than on disk.
QByteArray builtinFaceData(const QString &name)
{
    if (name == QLatin1String("icon"))
        return iconFaceSvg();
    if (name == QLatin1String("silver"))
        return silverFaceSvg();
    if (name == QLatin1String("honeycomb"))
        return honeycombFaceSvg();
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
    return name;
}

}  // namespace

Face::Face(const QString &path)
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
        ok = m_renderer->load(builtinFaceData(m_builtin));
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

std::unique_ptr<Face> openFace(const QString &path)
{
    if (!path.isEmpty()) {
        try {
            return std::make_unique<Face>(path);
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

QImage appIconImage(int size)
{
    size = std::max(1, size);
    QSvgRenderer renderer;
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (!renderer.load(appIconSvg()))
        return image;
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&painter, QRectF(0, 0, size, size));
    return image;
}
