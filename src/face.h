// An SVG clock face: its renderer, aspect ratio and rasterising, plus the
// recolouring that maps the artwork onto the user's wire/face colours.
#pragma once

#include <QByteArray>
#include <QImage>
#include <QRect>
#include <QString>

#include <memory>

class QSvgRenderer;

class Face
{
public:
    // Load a face from a file, a "builtin:" name, or the embedded default.
    explicit Face(const QString &path = QString());
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
std::unique_ptr<Face> openFace(const QString &path);

// Map the artwork's dark line work to wireHex and its light body to faceHex.
//
// Anti-aliased pixels are blended between the two, so edges stay smooth.
// Every output pixel depends only on the source pixel's brightness (0..255)
// and its alpha, so the blend collapses into a 256-entry lookup table.
QImage recolor(const QImage &art, const QString &wireHex, const QString &faceHex,
               bool faceTransparent);

// True when the artwork is (near enough) greyscale, and so is a candidate for
// recolouring. A full-colour drawing answers false: recolouring one throws all
// of its hues away, since recolor() reads only brightness.
bool isMonochrome(const QImage &art);

// Bounding box (inclusive) of the artwork's non-transparent pixels.
QRect contentBounds(const QImage &art);

// Render the built-in application icon at a pixel size.
QImage appIconImage(int size);
