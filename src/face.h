// An SVG clock face: its renderer, aspect ratio and rasterising, plus the
// recolouring that maps the artwork onto the user's wire/face colours.
#pragma once

#include <QByteArray>
#include <QImage>
#include <QPointF>
#include <QRect>
#include <QString>

#include <memory>

class QSvgRenderer;

class Face
{
public:
    // Load a face from a file, a "builtin:" name, or the embedded default.
    //
    // The colours are only for a generated face, which is built from them as
    // much as from its name.  A drawn face has its colours already, and takes
    // the user's two later on through recolor().
    explicit Face(const QString &path = QString(), const QString &faceHex = QString(),
                  const QString &wireHex = QString(), bool multiHue = false);
    ~Face();

    Face(const Face &) = delete;
    Face &operator=(const Face &) = delete;

    bool isDefault() const { return m_path.isEmpty(); }
    QString label() const;
    double aspect() const { return m_aspect; }

    // Rasterise at a pixel size, returning a straight-alpha ARGB32 image.
    QImage render(int width, int height) const;

private:
    void loadAspect();

    QString m_path;
    QString m_builtin;
    double m_aspect = 1.0;
    std::unique_ptr<QSvgRenderer> m_renderer;
};

// Load a face from a path, falling back to the embedded default.
std::unique_ptr<Face> openFace(const QString &path, const QString &faceHex = QString(),
                               const QString &wireHex = QString(), bool multiHue = false);

// Map the artwork's dark line work to wireHex and its light body to faceHex.
//
// Anti-aliased pixels are blended between the two, so edges stay smooth.
// facePercent and wirePercent fade the two ends independently, and are blended
// along the same ramp, so a part-lit pixel is faded by the mixture it is drawn
// in rather than snapping to one end's setting.
//
// Every output pixel depends only on the source pixel's brightness (0..255)
// and its alpha, so the blend collapses into a 256-entry lookup table.
QImage recolor(const QImage &art, const QString &wireHex, const QString &faceHex,
               int facePercent, int wirePercent);

// Bounding box (inclusive) of the artwork's non-transparent pixels.
QRect contentBounds(const QImage &art);
// Radius of the disc an image sweeps when turned about the given pivot.
double farthestCovered(const QImage &art, const QPointF &pivot);
// How much hard, high-contrast edge the artwork is made of, nothing up to one.
// A face of broad flat regions scores near nothing; one of alternating bands or
// fine strokes scores high.  See the definition for why a turning face needs to
// know.
double edgeDensity(const QImage &art);
