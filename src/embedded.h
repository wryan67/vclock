// Artwork and text that ship inside the program, so it has no external
// dependency on files or an icon theme.
#pragma once

#include <QByteArray>
#include <QColor>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <cmath>

// The built-in default face: used on first run, whenever no config exists,
// and whenever the "default" box is ticked.
inline QByteArray defaultFaceSvg()
{
    return QByteArrayLiteral(
        R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">
  <circle cx="50" cy="50" r="40" stroke="black" stroke-width="3" fill="white" />
</svg>
)SVG");
}

// A second built-in face: the app icon's dial with its painted hands, ticks
// and pin removed, so vclock draws those itself from the settings.  It is
// greyscale because recolor() maps brightness onto the wire/face colours;
// a wide brightness range is what makes the gradient survive recolouring.
//
// Note this is the tonal inverse of the app icon: recolor() reads dark pixels
// as the wire colour and light ones as the face colour, so the rim is dark and
// the body light here even though the icon paints them the other way round.
// The dial presets pass their colours accordingly.
inline QByteArray iconFaceSvg()
{
    return QByteArrayLiteral(
        R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">
  <defs>
    <linearGradient id="clockGradient" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#575757" />
      <stop offset="100%" stop-color="#efefef" />
    </linearGradient>
  </defs>
  <circle cx="50" cy="50" r="40" stroke="#171717" stroke-width="3" fill="url(#clockGradient)" />
</svg>
)SVG");
}

// A third built-in face: a silver dial with a dark rim.  It is the tonal
// opposite of the one above and shares its exact greys, only with the gradient
// running the other way (light at the top left, dark at the bottom right) and a
// dark rim instead of a light one.  Under the Silver preset's black wire and
// white face colours recolor() is the identity, so the artwork renders as
// drawn.  Note the gradient spans the circle's bounding box, so the disc itself
// only shows the middle 1/sqrt(2) of the ramp.
inline QByteArray silverFaceSvg()
{
    return QByteArrayLiteral(
        R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">
  <defs>
    <linearGradient id="clockGradient" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#efefef" />
      <stop offset="100%" stop-color="#575757" />
    </linearGradient>
  </defs>
  <circle cx="50" cy="50" r="40" stroke="#171717" stroke-width="3" fill="url(#clockGradient)" />
</svg>
)SVG");
}

// A fifth built-in face: a spiral.  Greyscale like the rest, so recolor() maps
// it onto the two settings colours -- but note which way round: the disc is the
// light tone and so takes the face colour, while the arm is the dark one and
// takes the wire colour.  That is deliberate, and it is what keeps the spiral
// in the same family as the gradient dial, whose body is likewise the face
// colour and whose rim is the wire.
//
// One arm, wound six full turns, and it ends by becoming the wall of the dial.
// The taper runs the way the arm grows: a hairline where it leaves the centre,
// broadening to four units by the time it reaches the edge.  The widening is
// held back until late -- the width follows the fourth power of the distance
// out -- so the inner turns stay fine and the whole of the thickening happens
// in the last one.  A straight taper does not work here; with six turns the arm
// and the gap between turns end up the same width and the eye reads a bullseye
// rather than a spiral.
//
// The ending is the other half of the trick.  The arm does not slow down or
// aim itself at the wall: it stays on a plain Archimedean course all the way
// and is simply cut off by the outer circle, so along its last stretch the
// arm's outer boundary *is* the rim, and the arm slides out through the wall
// and tapers to nothing as its inner edge follows.  An earlier version eased
// the radius to a standstill just short of the edge instead; that made the arm
// arrive travelling along the wall, which is smooth enough, but it also left a
// blunt radial end where the arm stopped.  Clipping has no end to hide.
//
// So the curve is carried past r=40 and the outline is built in three parts:
// up the outer edge as far as the circle, round the circle to where the inner
// edge leaves it, and back down the inner edge to the hub.  The clip is taken
// at the rim stroke's outer edge rather than at the disc, so the band and the
// rim finish on exactly the same line and there is no seam between them.
//
// SVG Tiny has no clip-path and no variable-width stroke, so both the clipping
// and the taper are done here, in the geometry: the arm is a filled outline
// cut to shape.  Four hundred and sixty samples is the point at which the
// flats stop being distinguishable from a far denser curve at the largest
// clock the app will build; six turns need roughly three times the samples of
// one, since the error goes with the square of the step.
//
// The rim stays fine, a single unit, even though the arm now ends thick -- the
// arm supplies its own edge there.  That matters because everything else on the
// dial is meant to sit in one weight, rim and hour marks alike, so the preset
// picks a mark scale that lands just under it.
inline QByteArray spiralFaceSvg()
{
    return QByteArrayLiteral(
        R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">
  <circle cx="50" cy="50" r="40" fill="#e8e8e8" />
  <path d="M 50.00 50.87 L 49.94 50.87 L 49.88 50.86 L 49.82 50.85 L 49.76 50.83 L 49.71 50.80 L 49.66 50.77 L 49.61 50.74 L 49.57 50.70 L 49.54 50.66 L 49.51 50.62 L 49.48 50.58 L 49.46 50.54 L 49.44 50.50 L 49.43 50.46 L 49.43 50.42 L 49.42 50.39 L 49.42 50.35 L 49.42 50.32 L 49.42 50.29 L 49.43 50.27 L 49.44 50.24 L 49.44 50.22 L 49.45 50.20 L 49.46 50.19 L 49.47 50.17 L 49.48 50.16 L 49.48 50.15 L 49.49 50.14 L 49.50 50.13 L 49.50 50.13 L 49.50 50.13 L 49.51 50.12 L 49.51 50.12 L 49.50 50.12 L 49.50 50.12 L 49.50 50.12 L 49.49 50.12 L 49.48 50.12 L 49.47 50.12 L 49.46 50.12 L 49.44 50.11 L 49.43 50.11 L 49.41 50.10 L 49.39 50.10 L 49.38 50.09 L 49.36 50.08 L 49.33 50.07 L 49.31 50.05 L 49.29 50.04 L 49.27 50.02 L 49.25 50.00 L 49.22 49.97 L 49.20 49.95 L 49.18 49.92 L 49.16 49.89 L 49.14 49.85 L 49.12 49.82 L 49.10 49.78 L 49.09 49.74 L 49.07 49.69 L 49.06 49.65 L 49.05 49.60 L 49.04 49.55 L 49.04 49.50 L 49.04 49.44 L 49.04 49.39 L 49.04 49.33 L 49.05 49.27 L 49.06 49.21 L 49.08 49.15 L 49.09 49.09 L 49.12 49.03 L 49.14 48.96 L 49.18 48.90 L 49.21 48.84 L 49.25 48.78 L 49.29 48.72 L 49.34 48.66 L 49.39 48.60 L 49.45 48.55 L 49.51 48.49 L 49.58 48.44 L 49.65 48.39 L 49.72 48.35 L 49.80 48.31 L 49.88 48.27 L 49.96 48.23 L 50.05 48.20 L 50.14 48.18 L 50.23 48.16 L 50.33 48.14 L 50.43 48.13 L 50.53 48.12 L 50.63 48.12 L 50.74 48.12 L 50.84 48.13 L 50.95 48.15 L 51.06 48.17 L 51.16 48.20 L 51.27 48.24 L 51.38 48.28 L 51.49 48.33 L 51.59 48.38 L 51.69 48.44 L 51.80 48.51 L 51.90 48.58 L 51.99 48.66 L 52.08 48.75 L 52.17 48.84 L 52.26 48.94 L 52.34 49.04 L 52.42 49.15 L 52.49 49.27 L 52.56 49.39 L 52.62 49.51 L 52.67 49.64 L 52.72 49.78 L 52.76 49.92 L 52.79 50.06 L 52.82 50.20 L 52.84 50.35 L 52.85 50.50 L 52.85 50.66 L 52.85 50.81 L 52.83 50.97 L 52.81 51.12 L 52.78 51.28 L 52.74 51.44 L 52.69 51.59 L 52.63 51.75 L 52.57 51.90 L 52.49 52.05 L 52.41 52.20 L 52.31 52.35 L 52.21 52.49 L 52.10 52.63 L 51.98 52.76 L 51.86 52.89 L 51.72 53.01 L 51.58 53.13 L 51.43 53.24 L 51.27 53.34 L 51.11 53.43 L 50.94 53.52 L 50.76 53.60 L 50.58 53.66 L 50.39 53.72 L 50.20 53.77 L 50.01 53.81 L 49.81 53.84 L 49.60 53.86 L 49.40 53.87 L 49.19 53.87 L 48.98 53.86 L 48.77 53.83 L 48.56 53.80 L 48.35 53.75 L 48.14 53.69 L 47.94 53.62 L 47.73 53.54 L 47.53 53.44 L 47.33 53.34 L 47.14 53.22 L 46.95 53.09 L 46.77 52.96 L 46.59 52.81 L 46.42 52.65 L 46.26 52.48 L 46.10 52.30 L 45.96 52.11 L 45.82 51.91 L 45.69 51.70 L 45.58 51.49 L 45.47 51.27 L 45.37 51.04 L 45.29 50.80 L 45.22 50.56 L 45.16 50.31 L 45.11 50.06 L 45.08 49.80 L 45.06 49.54 L 45.06 49.28 L 45.06 49.02 L 45.09 48.75 L 45.12 48.49 L 45.17 48.22 L 45.24 47.96 L 45.32 47.69 L 45.41 47.44 L 45.52 47.18 L 45.64 46.93 L 45.78 46.68 L 45.93 46.44 L 46.10 46.21 L 46.27 45.98 L 46.46 45.77 L 46.67 45.56 L 46.88 45.36 L 47.11 45.17 L 47.35 44.99 L 47.60 44.82 L 47.86 44.67 L 48.12 44.53 L 48.40 44.40 L 48.69 44.29 L 48.98 44.19 L 49.28 44.11 L 49.59 44.04 L 49.90 43.99 L 50.21 43.95 L 50.53 43.94 L 50.85 43.93 L 51.18 43.95 L 51.50 43.98 L 51.82 44.03 L 52.15 44.10 L 52.47 44.18 L 52.78 44.28 L 53.10 44.40 L 53.41 44.54 L 53.71 44.69 L 54.01 44.86 L 54.30 45.05 L 54.58 45.25 L 54.85 45.47 L 55.11 45.71 L 55.35 45.95 L 55.59 46.22 L 55.82 46.49 L 56.03 46.78 L 56.22 47.09 L 56.40 47.40 L 56.57 47.73 L 56.71 48.06 L 56.85 48.41 L 56.96 48.76 L 57.05 49.12 L 57.13 49.49 L 57.19 49.86 L 57.23 50.24 L 57.24 50.62 L 57.24 51.00 L 57.22 51.39 L 57.18 51.77 L 57.11 52.16 L 57.03 52.54 L 56.92 52.92 L 56.80 53.29 L 56.65 53.66 L 56.48 54.03 L 56.30 54.39 L 56.09 54.73 L 55.87 55.07 L 55.62 55.40 L 55.36 55.72 L 55.08 56.02 L 54.79 56.31 L 54.47 56.59 L 54.14 56.85 L 53.80 57.09 L 53.44 57.32 L 53.07 57.53 L 52.68 57.72 L 52.29 57.89 L 51.88 58.04 L 51.47 58.17 L 51.04 58.27 L 50.61 58.36 L 50.17 58.42 L 49.73 58.46 L 49.29 58.48 L 48.84 58.47 L 48.39 58.44 L 47.94 58.38 L 47.49 58.30 L 47.04 58.20 L 46.60 58.08 L 46.17 57.93 L 45.74 57.75 L 45.31 57.55 L 44.90 57.33 L 44.50 57.09 L 44.11 56.83 L 43.73 56.54 L 43.36 56.23 L 43.01 55.90 L 42.68 55.56 L 42.36 55.19 L 42.06 54.80 L 41.79 54.40 L 41.53 53.99 L 41.29 53.55 L 41.07 53.11 L 40.88 52.65 L 40.71 52.18 L 40.57 51.70 L 40.45 51.21 L 40.36 50.71 L 40.29 50.20 L 40.25 49.69 L 40.23 49.18 L 40.25 48.66 L 40.29 48.15 L 40.35 47.63 L 40.45 47.11 L 40.57 46.60 L 40.72 46.10 L 40.90 45.60 L 41.10 45.10 L 41.33 44.62 L 41.59 44.15 L 41.87 43.69 L 42.18 43.24 L 42.51 42.81 L 42.86 42.40 L 43.24 42.00 L 43.64 41.62 L 44.07 41.26 L 44.51 40.92 L 44.97 40.61 L 45.45 40.32 L 45.94 40.05 L 46.46 39.81 L 46.98 39.59 L 47.52 39.40 L 48.07 39.24 L 48.63 39.11 L 49.20 39.01 L 49.77 38.94 L 50.36 38.89 L 50.94 38.88 L 51.53 38.90 L 52.11 38.95 L 52.70 39.03 L 53.29 39.14 L 53.87 39.29 L 54.44 39.46 L 55.01 39.67 L 55.56 39.90 L 56.11 40.17 L 56.64 40.46 L 57.16 40.78 L 57.66 41.14 L 58.15 41.52 L 58.62 41.92 L 59.07 42.35 L 59.49 42.81 L 59.89 43.29 L 60.27 43.79 L 60.62 44.32 L 60.95 44.86 L 61.25 45.42 L 61.52 46.00 L 61.76 46.60 L 61.97 47.21 L 62.14 47.83 L 62.29 48.46 L 62.40 49.11 L 62.48 49.76 L 62.52 50.41 L 62.53 51.07 L 62.51 51.73 L 62.45 52.39 L 62.35 53.05 L 62.22 53.71 L 62.06 54.36 L 61.86 55.01 L 61.62 55.64 L 61.35 56.27 L 61.05 56.88 L 60.72 57.47 L 60.35 58.06 L 59.95 58.62 L 59.52 59.16 L 59.06 59.68 L 58.57 60.18 L 58.06 60.66 L 57.52 61.11 L 56.95 61.53 L 56.36 61.92 L 55.75 62.28 L 55.12 62.61 L 54.46 62.91 L 53.80 63.17 L 53.11 63.40 L 52.41 63.60 L 51.71 63.76 L 50.99 63.88 L 50.26 63.96 L 49.53 64.00 L 48.79 64.01 L 48.05 63.98 L 47.31 63.91 L 46.57 63.80 L 45.84 63.65 L 45.11 63.46 L 44.40 63.23 L 43.69 62.96 L 42.99 62.66 L 42.31 62.32 L 41.65 61.94 L 41.00 61.53 L 40.38 61.08 L 39.77 60.60 L 39.20 60.08 L 38.64 59.54 L 38.12 58.96 L 37.62 58.36 L 37.16 57.72 L 36.72 57.06 L 36.32 56.38 L 35.96 55.68 L 35.63 54.95 L 35.34 54.20 L 35.09 53.44 L 34.88 52.67 L 34.71 51.88 L 34.58 51.08 L 34.49 50.27 L 34.44 49.45 L 34.44 48.64 L 34.48 47.81 L 34.57 46.99 L 34.69 46.18 L 34.86 45.37 L 35.08 44.56 L 35.33 43.77 L 35.63 42.98 L 35.97 42.21 L 36.35 41.46 L 36.78 40.73 L 37.24 40.01 L 37.74 39.32 L 38.27 38.66 L 38.85 38.02 L 39.46 37.41 L 40.10 36.83 L 40.77 36.29 L 41.47 35.77 L 42.20 35.30 L 42.96 34.86 L 43.74 34.46 L 44.55 34.10 L 45.37 33.79 L 46.22 33.51 L 47.08 33.28 L 47.95 33.10 L 48.83 32.96 L 49.73 32.87 L 50.63 32.82 L 51.53 32.82 L 52.44 32.87 L 53.34 32.97 L 54.24 33.11 L 55.14 33.30 L 56.02 33.54 L 56.90 33.83 L 57.76 34.16 L 58.61 34.54 L 59.44 34.97 L 60.24 35.44 L 61.03 35.95 L 61.79 36.50 L 62.52 37.10 L 63.22 37.73 L 63.88 38.41 L 64.52 39.12 L 65.12 39.86 L 65.67 40.64 L 66.19 41.44 L 66.67 42.28 L 67.11 43.14 L 67.50 44.03 L 67.84 44.94 L 68.14 45.87 L 68.39 46.81 L 68.59 47.77 L 68.73 48.75 L 68.83 49.73 L 68.88 50.72 L 68.87 51.71 L 68.81 52.71 L 68.70 53.70 L 68.54 54.69 L 68.33 55.67 L 68.06 56.64 L 67.74 57.60 L 67.37 58.55 L 66.95 59.47 L 66.48 60.38 L 65.96 61.26 L 65.40 62.12 L 64.78 62.95 L 64.13 63.75 L 63.43 64.51 L 62.69 65.24 L 61.91 65.93 L 61.09 66.58 L 60.24 67.19 L 59.35 67.76 L 58.43 68.28 L 57.48 68.75 L 56.51 69.17 L 55.51 69.55 L 54.49 69.87 L 53.46 70.14 L 52.40 70.35 L 51.34 70.51 L 50.26 70.61 L 49.18 70.66 L 48.09 70.65 L 47.01 70.58 L 45.92 70.45 L 44.84 70.27 L 43.77 70.03 L 42.71 69.73 L 41.66 69.38 L 40.63 68.97 L 39.61 68.51 L 38.63 67.99 L 37.66 67.42 L 36.73 66.80 L 35.83 66.13 L 34.96 65.41 L 34.13 64.64 L 33.33 63.83 L 32.58 62.97 L 31.87 62.08 L 31.21 61.14 L 30.60 60.17 L 30.04 59.17 L 29.52 58.13 L 29.07 57.07 L 28.67 55.98 L 28.32 54.87 L 28.03 53.73 L 27.80 52.59 L 27.64 51.42 L 27.53 50.25 L 27.48 49.07 L 27.50 47.89 L 27.58 46.70 L 27.72 45.52 L 27.92 44.34 L 28.19 43.17 L 28.52 42.02 L 28.90 40.88 L 29.35 39.76 L 29.86 38.66 L 30.43 37.58 L 31.06 36.54 L 31.74 35.52 L 32.47 34.54 L 33.26 33.60 L 34.10 32.70 L 34.98 31.84 L 35.92 31.02 L 36.89 30.26 L 37.91 29.54 L 38.97 28.88 L 40.07 28.27 L 41.19 27.72 L 42.35 27.23 L 43.54 26.79 L 44.75 26.42 L 45.98 26.11 L 47.23 25.87 L 48.49 25.69 L 49.77 25.58 L 51.05 25.54 L 52.34 25.56 L 53.63 25.65 L 54.91 25.81 L 56.19 26.03 L 57.46 26.32 L 58.71 26.69 L 59.95 27.11 L 61.16 27.60 L 62.35 28.16 L 63.52 28.78 L 64.65 29.46 L 65.75 30.21 L 66.81 31.01 L 67.83 31.87 L 68.80 32.78 L 69.73 33.74 L 70.61 34.76 L 71.44 35.82 L 72.21 36.93 L 72.93 38.08 L 73.58 39.27 L 74.17 40.50 L 74.70 41.75 L 75.17 43.04 L 75.57 44.35 L 75.90 45.69 L 76.15 47.05 L 76.34 48.42 L 76.46 49.80 L 76.50 51.19 L 76.47 52.58 L 76.37 53.97 L 76.20 55.37 L 75.95 56.75 L 75.62 58.12 L 75.23 59.48 L 74.76 60.81 L 74.22 62.13 L 73.62 63.41 L 72.94 64.67 L 72.20 65.89 L 71.39 67.08 L 70.52 68.22 L 69.59 69.32 L 68.60 70.38 L 67.55 71.38 L 66.45 72.33 L 65.29 73.22 L 64.09 74.05 L 62.85 74.82 L 61.56 75.52 L 60.23 76.16 L 58.87 76.73 L 57.47 77.22 L 56.05 77.65 L 54.61 78.00 L 53.14 78.28 L 51.66 78.47 L 50.17 78.59 L 48.67 78.64 L 47.16 78.60 L 45.65 78.48 L 44.15 78.29 L 42.66 78.01 L 41.18 77.66 L 39.72 77.23 L 38.27 76.72 L 36.86 76.14 L 35.47 75.48 L 34.11 74.74 L 32.80 73.93 L 31.52 73.06 L 30.29 72.11 L 29.10 71.10 L 27.97 70.03 L 26.89 68.90 L 25.88 67.70 L 24.92 66.46 L 24.03 65.16 L 23.20 63.81 L 22.45 62.42 L 21.76 60.98 L 21.15 59.51 L 20.62 58.01 L 20.17 56.47 L 19.80 54.91 L 19.51 53.33 L 19.30 51.73 L 19.17 50.12 L 19.13 48.50 L 19.18 46.88 L 19.31 45.26 L 19.52 43.64 L 19.82 42.03 L 20.21 40.44 L 20.68 38.87 L 21.23 37.31 L 21.87 35.79 L 22.58 34.30 L 23.38 32.84 L 24.25 31.42 L 25.20 30.05 L 26.22 28.73 L 27.31 27.45 L 28.47 26.24 L 29.70 25.08 L 30.99 23.99 L 32.33 22.96 L 33.73 22.01 L 35.19 21.12 L 36.69 20.31 L 38.23 19.58 L 39.82 18.93 L 41.44 18.37 L 43.09 17.89 L 44.77 17.49 L 46.47 17.18 L 48.19 16.96 L 49.93 16.83 L 51.67 16.80 L 53.42 16.85 L 55.16 16.99 L 56.90 17.23 L 58.63 17.56 L 60.34 17.98 L 62.03 18.49 L 63.70 19.09 L 65.34 19.78 L 66.94 20.55 L 68.50 21.41 L 70.02 22.36 L 71.50 23.38 L 72.92 24.48 L 74.28 25.66 L 75.58 26.91 L 76.82 28.23 L 77.99 29.62 L 79.09 31.07 L 80.11 32.58 L 81.05 34.14 L 81.92 35.76 L 82.70 37.42 L 83.39 39.13 L 83.99 40.87 L 84.51 42.65 L 84.93 44.45 L 85.25 46.28 L 85.48 48.13 L 85.61 49.99 L 85.65 51.86 L 85.59 53.73 L 85.42 55.61 L 85.16 57.47 L 84.81 59.33 L 84.35 61.16 L 83.80 62.98 L 83.15 64.77 L 82.40 66.52 L 81.57 68.24 L 80.64 69.91 L 79.62 71.54 L 78.52 73.12 L 77.33 74.64 L 76.06 76.10 L 74.72 77.49 L 73.29 78.81 L 71.80 80.06 L 70.24 81.24 L 70.07 84.60 L 70.07 84.60 L 72.22 83.86 L 73.97 82.65 L 75.65 81.35 L 77.25 79.96 L 78.79 78.49 L 80.24 76.94 L 81.61 75.32 L 82.89 73.63 L 84.09 71.87 L 85.19 70.06 L 86.19 68.18 L 87.09 66.26 L 87.89 64.30 L 88.59 62.29 L 89.18 60.25 L 89.66 58.18 L 90.04 56.09 L 90.30 53.99 L 90.46 51.87 L 90.50 49.75 L 90.43 47.62 L 90.25 45.51 L 89.96 43.41 L 89.53 41.33 L 88.93 39.29 L 88.23 37.29 L 87.43 35.34 L 86.53 33.44 L 85.53 31.59 L 84.44 29.80 L 83.26 28.07 L 81.99 26.41 L 80.64 24.82 L 79.21 23.31 L 77.71 21.88 L 76.14 20.53 L 74.50 19.27 L 72.81 18.10 L 71.06 17.03 L 69.26 16.05 L 67.41 15.16 L 65.53 14.38 L 63.61 13.70 L 61.66 13.12 L 59.69 12.64 L 57.70 12.27 L 55.70 12.01 L 53.69 11.85 L 51.68 11.80 L 49.68 11.85 L 47.68 12.00 L 45.71 12.26 L 43.75 12.63 L 41.82 13.09 L 39.92 13.65 L 38.05 14.31 L 36.23 15.07 L 34.46 15.92 L 32.73 16.86 L 31.06 17.88 L 29.45 18.99 L 27.91 20.18 L 26.43 21.44 L 25.02 22.78 L 23.69 24.19 L 22.44 25.66 L 21.27 27.19 L 20.18 28.77 L 19.18 30.41 L 18.27 32.10 L 17.45 33.82 L 16.72 35.58 L 16.09 37.38 L 15.56 39.20 L 15.12 41.04 L 14.78 42.89 L 14.54 44.76 L 14.40 46.63 L 14.35 48.51 L 14.41 50.38 L 14.56 52.24 L 14.81 54.08 L 15.15 55.91 L 15.59 57.71 L 16.12 59.48 L 16.75 61.21 L 17.46 62.91 L 18.25 64.56 L 19.13 66.17 L 20.09 67.72 L 21.13 69.22 L 22.24 70.66 L 23.43 72.03 L 24.68 73.34 L 25.99 74.58 L 27.37 75.74 L 28.80 76.83 L 30.28 77.83 L 31.81 78.76 L 33.38 79.60 L 34.99 80.36 L 36.64 81.03 L 38.31 81.61 L 40.01 82.11 L 41.72 82.51 L 43.45 82.82 L 45.19 83.04 L 46.94 83.16 L 48.68 83.20 L 50.43 83.14 L 52.16 83.00 L 53.87 82.76 L 55.57 82.43 L 57.25 82.02 L 58.89 81.52 L 60.51 80.93 L 62.08 80.27 L 63.62 79.52 L 65.11 78.70 L 66.56 77.80 L 67.95 76.83 L 69.28 75.79 L 70.56 74.68 L 71.77 73.51 L 72.91 72.28 L 73.99 71.00 L 75.00 69.67 L 75.93 68.29 L 76.79 66.86 L 77.57 65.39 L 78.27 63.89 L 78.89 62.36 L 79.42 60.81 L 79.87 59.23 L 80.24 57.63 L 80.53 56.02 L 80.72 54.40 L 80.84 52.78 L 80.86 51.15 L 80.80 49.54 L 80.66 47.93 L 80.44 46.33 L 80.13 44.76 L 79.74 43.20 L 79.27 41.67 L 78.72 40.17 L 78.09 38.71 L 77.40 37.28 L 76.63 35.90 L 75.79 34.56 L 74.88 33.28 L 73.91 32.04 L 72.88 30.86 L 71.79 29.74 L 70.64 28.68 L 69.45 27.68 L 68.21 26.75 L 66.92 25.89 L 65.59 25.10 L 64.23 24.38 L 62.84 23.74 L 61.41 23.17 L 59.97 22.67 L 58.50 22.26 L 57.02 21.92 L 55.52 21.67 L 54.02 21.49 L 52.51 21.39 L 51.01 21.37 L 49.51 21.43 L 48.01 21.57 L 46.53 21.78 L 45.07 22.07 L 43.63 22.44 L 42.22 22.88 L 40.83 23.40 L 39.48 23.98 L 38.16 24.63 L 36.88 25.35 L 35.64 26.13 L 34.45 26.98 L 33.31 27.88 L 32.22 28.84 L 31.18 29.86 L 30.20 30.92 L 29.28 32.03 L 28.43 33.18 L 27.63 34.38 L 26.90 35.61 L 26.24 36.87 L 25.65 38.17 L 25.13 39.49 L 24.68 40.83 L 24.30 42.19 L 23.99 43.56 L 23.76 44.95 L 23.60 46.34 L 23.52 47.74 L 23.50 49.13 L 23.56 50.52 L 23.70 51.90 L 23.90 53.27 L 24.18 54.62 L 24.52 55.95 L 24.93 57.26 L 25.41 58.54 L 25.96 59.79 L 26.57 61.01 L 27.24 62.19 L 27.97 63.33 L 28.75 64.43 L 29.59 65.48 L 30.48 66.48 L 31.42 67.44 L 32.41 68.34 L 33.44 69.18 L 34.51 69.97 L 35.62 70.70 L 36.76 71.37 L 37.93 71.97 L 39.12 72.51 L 40.35 72.99 L 41.59 73.40 L 42.84 73.75 L 44.11 74.03 L 45.39 74.23 L 46.68 74.38 L 47.97 74.45 L 49.25 74.46 L 50.54 74.40 L 51.81 74.27 L 53.07 74.07 L 54.32 73.82 L 55.54 73.49 L 56.75 73.11 L 57.93 72.66 L 59.08 72.15 L 60.20 71.59 L 61.29 70.96 L 62.34 70.29 L 63.35 69.56 L 64.31 68.78 L 65.24 67.95 L 66.11 67.08 L 66.94 66.17 L 67.71 65.22 L 68.43 64.23 L 69.10 63.21 L 69.71 62.15 L 70.27 61.07 L 70.76 59.97 L 71.20 58.84 L 71.57 57.70 L 71.88 56.54 L 72.13 55.37 L 72.32 54.19 L 72.45 53.00 L 72.51 51.82 L 72.51 50.63 L 72.45 49.45 L 72.33 48.28 L 72.14 47.12 L 71.90 45.98 L 71.60 44.85 L 71.24 43.74 L 70.82 42.66 L 70.35 41.60 L 69.82 40.57 L 69.25 39.58 L 68.62 38.62 L 67.95 37.69 L 67.23 36.81 L 66.46 35.96 L 65.66 35.16 L 64.82 34.40 L 63.94 33.70 L 63.03 33.04 L 62.09 32.43 L 61.12 31.87 L 60.12 31.37 L 59.11 30.92 L 58.07 30.52 L 57.02 30.19 L 55.95 29.90 L 54.88 29.68 L 53.79 29.51 L 52.71 29.40 L 51.62 29.35 L 50.53 29.35 L 49.45 29.41 L 48.38 29.53 L 47.31 29.70 L 46.26 29.93 L 45.23 30.21 L 44.22 30.55 L 43.23 30.94 L 42.26 31.37 L 41.32 31.86 L 40.41 32.39 L 39.53 32.97 L 38.69 33.59 L 37.88 34.25 L 37.11 34.96 L 36.38 35.69 L 35.69 36.47 L 35.04 37.28 L 34.45 38.11 L 33.89 38.98 L 33.39 39.87 L 32.93 40.78 L 32.52 41.71 L 32.17 42.66 L 31.86 43.63 L 31.61 44.60 L 31.41 45.59 L 31.26 46.58 L 31.17 47.57 L 31.12 48.57 L 31.13 49.56 L 31.19 50.55 L 31.30 51.53 L 31.47 52.50 L 31.68 53.46 L 31.94 54.40 L 32.25 55.32 L 32.61 56.23 L 33.02 57.11 L 33.46 57.96 L 33.95 58.79 L 34.48 59.59 L 35.06 60.36 L 35.66 61.09 L 36.31 61.79 L 36.99 62.46 L 37.70 63.08 L 38.44 63.66 L 39.20 64.21 L 39.99 64.71 L 40.81 65.16 L 41.64 65.57 L 42.49 65.94 L 43.36 66.26 L 44.24 66.53 L 45.13 66.76 L 46.03 66.94 L 46.93 67.07 L 47.84 67.15 L 48.74 67.18 L 49.65 67.17 L 50.55 67.11 L 51.44 67.00 L 52.32 66.85 L 53.19 66.65 L 54.04 66.41 L 54.88 66.12 L 55.70 65.79 L 56.50 65.42 L 57.28 65.01 L 58.03 64.56 L 58.75 64.07 L 59.44 63.55 L 60.11 62.99 L 60.74 62.40 L 61.34 61.78 L 61.90 61.13 L 62.43 60.46 L 62.91 59.76 L 63.36 59.04 L 63.77 58.30 L 64.14 57.54 L 64.47 56.77 L 64.75 55.98 L 65.00 55.18 L 65.20 54.37 L 65.35 53.56 L 65.47 52.74 L 65.54 51.92 L 65.56 51.10 L 65.55 50.28 L 65.49 49.46 L 65.38 48.66 L 65.24 47.86 L 65.05 47.07 L 64.83 46.30 L 64.56 45.54 L 64.26 44.80 L 63.92 44.08 L 63.55 43.38 L 63.13 42.71 L 62.69 42.06 L 62.21 41.43 L 61.71 40.84 L 61.17 40.27 L 60.61 39.73 L 60.02 39.23 L 59.41 38.76 L 58.77 38.32 L 58.12 37.92 L 57.45 37.56 L 56.76 37.23 L 56.06 36.94 L 55.35 36.68 L 54.63 36.47 L 53.90 36.30 L 53.16 36.16 L 52.42 36.06 L 51.68 36.01 L 50.94 35.99 L 50.21 36.01 L 49.48 36.07 L 48.75 36.16 L 48.03 36.30 L 47.33 36.47 L 46.63 36.68 L 45.95 36.92 L 45.29 37.20 L 44.65 37.51 L 44.02 37.85 L 43.42 38.22 L 42.83 38.63 L 42.28 39.06 L 41.74 39.52 L 41.24 40.00 L 40.76 40.51 L 40.31 41.04 L 39.89 41.59 L 39.51 42.17 L 39.15 42.75 L 38.83 43.36 L 38.54 43.98 L 38.28 44.61 L 38.06 45.25 L 37.87 45.89 L 37.72 46.55 L 37.61 47.21 L 37.52 47.87 L 37.48 48.53 L 37.47 49.19 L 37.49 49.85 L 37.55 50.51 L 37.64 51.16 L 37.76 51.80 L 37.92 52.43 L 38.11 53.04 L 38.34 53.65 L 38.59 54.24 L 38.87 54.81 L 39.18 55.37 L 39.52 55.91 L 39.88 56.42 L 40.27 56.92 L 40.68 57.39 L 41.12 57.83 L 41.58 58.26 L 42.05 58.65 L 42.55 59.02 L 43.06 59.36 L 43.59 59.67 L 44.13 59.96 L 44.68 60.21 L 45.24 60.43 L 45.81 60.62 L 46.39 60.78 L 46.97 60.91 L 47.56 61.01 L 48.15 61.08 L 48.74 61.11 L 49.32 61.12 L 49.91 61.09 L 50.49 61.04 L 51.06 60.95 L 51.63 60.83 L 52.19 60.69 L 52.73 60.51 L 53.27 60.31 L 53.79 60.08 L 54.29 59.83 L 54.78 59.55 L 55.25 59.25 L 55.71 58.92 L 56.14 58.57 L 56.55 58.20 L 56.95 57.81 L 57.31 57.41 L 57.66 56.98 L 57.98 56.54 L 58.28 56.09 L 58.55 55.62 L 58.79 55.14 L 59.01 54.65 L 59.20 54.15 L 59.36 53.65 L 59.50 53.14 L 59.61 52.62 L 59.69 52.11 L 59.74 51.59 L 59.77 51.07 L 59.77 50.55 L 59.74 50.04 L 59.68 49.53 L 59.60 49.03 L 59.49 48.53 L 59.36 48.04 L 59.20 47.57 L 59.02 47.10 L 58.82 46.65 L 58.59 46.21 L 58.34 45.78 L 58.07 45.37 L 57.78 44.98 L 57.47 44.60 L 57.14 44.24 L 56.79 43.91 L 56.44 43.59 L 56.06 43.29 L 55.67 43.02 L 55.27 42.76 L 54.86 42.53 L 54.44 42.32 L 54.01 42.14 L 53.58 41.98 L 53.14 41.84 L 52.69 41.73 L 52.24 41.64 L 51.79 41.57 L 51.34 41.53 L 50.89 41.52 L 50.44 41.52 L 49.99 41.56 L 49.55 41.61 L 49.12 41.69 L 48.69 41.79 L 48.27 41.91 L 47.86 42.05 L 47.45 42.21 L 47.06 42.40 L 46.69 42.60 L 46.32 42.82 L 45.97 43.06 L 45.63 43.32 L 45.31 43.59 L 45.01 43.88 L 44.72 44.18 L 44.45 44.49 L 44.20 44.82 L 43.97 45.16 L 43.75 45.50 L 43.56 45.86 L 43.39 46.22 L 43.24 46.60 L 43.10 46.97 L 42.99 47.35 L 42.90 47.74 L 42.83 48.12 L 42.78 48.51 L 42.75 48.90 L 42.75 49.29 L 42.76 49.67 L 42.79 50.05 L 42.85 50.43 L 42.92 50.80 L 43.01 51.16 L 43.12 51.52 L 43.25 51.87 L 43.39 52.21 L 43.56 52.54 L 43.73 52.86 L 43.93 53.16 L 44.14 53.46 L 44.36 53.74 L 44.60 54.01 L 44.85 54.26 L 45.11 54.50 L 45.38 54.72 L 45.66 54.93 L 45.95 55.12 L 46.25 55.30 L 46.55 55.45 L 46.86 55.59 L 47.18 55.72 L 47.50 55.82 L 47.82 55.91 L 48.15 55.98 L 48.47 56.03 L 48.80 56.06 L 49.13 56.08 L 49.45 56.08 L 49.77 56.06 L 50.09 56.02 L 50.41 55.97 L 50.72 55.90 L 51.02 55.82 L 51.32 55.72 L 51.61 55.61 L 51.89 55.48 L 52.16 55.34 L 52.42 55.18 L 52.67 55.01 L 52.91 54.83 L 53.14 54.64 L 53.36 54.44 L 53.57 54.22 L 53.76 54.00 L 53.94 53.77 L 54.10 53.54 L 54.25 53.29 L 54.39 53.04 L 54.51 52.78 L 54.62 52.52 L 54.71 52.26 L 54.79 51.99 L 54.86 51.73 L 54.90 51.46 L 54.94 51.19 L 54.96 50.92 L 54.96 50.65 L 54.95 50.38 L 54.93 50.12 L 54.89 49.86 L 54.84 49.60 L 54.78 49.35 L 54.70 49.11 L 54.61 48.87 L 54.51 48.64 L 54.40 48.41 L 54.27 48.20 L 54.14 47.99 L 53.99 47.79 L 53.84 47.60 L 53.68 47.42 L 53.51 47.25 L 53.33 47.09 L 53.14 46.95 L 52.95 46.81 L 52.76 46.69 L 52.55 46.57 L 52.35 46.47 L 52.14 46.38 L 51.93 46.31 L 51.71 46.24 L 51.50 46.19 L 51.28 46.15 L 51.07 46.12 L 50.85 46.10 L 50.64 46.10 L 50.42 46.11 L 50.21 46.13 L 50.00 46.15 L 49.80 46.20 L 49.60 46.25 L 49.41 46.31 L 49.22 46.38 L 49.03 46.46 L 48.86 46.55 L 48.69 46.65 L 48.52 46.76 L 48.37 46.87 L 48.22 46.99 L 48.08 47.12 L 47.95 47.26 L 47.83 47.40 L 47.71 47.55 L 47.61 47.70 L 47.52 47.85 L 47.43 48.01 L 47.36 48.17 L 47.29 48.34 L 47.23 48.50 L 47.19 48.67 L 47.15 48.84 L 47.13 49.01 L 47.11 49.17 L 47.10 49.34 L 47.10 49.51 L 47.12 49.67 L 47.14 49.83 L 47.17 49.99 L 47.20 50.14 L 47.25 50.29 L 47.30 50.44 L 47.37 50.58 L 47.43 50.71 L 47.51 50.85 L 47.59 50.97 L 47.68 51.09 L 47.78 51.20 L 47.88 51.31 L 47.98 51.40 L 48.09 51.49 L 48.20 51.58 L 48.32 51.65 L 48.44 51.72 L 48.56 51.78 L 48.69 51.83 L 48.82 51.88 L 48.94 51.91 L 49.07 51.94 L 49.20 51.96 L 49.33 51.97 L 49.46 51.98 L 49.58 51.97 L 49.71 51.96 L 49.83 51.94 L 49.95 51.91 L 50.07 51.88 L 50.18 51.83 L 50.29 51.79 L 50.40 51.73 L 50.50 51.67 L 50.59 51.60 L 50.68 51.53 L 50.77 51.45 L 50.84 51.36 L 50.92 51.27 L 50.98 51.18 L 51.04 51.08 L 51.09 50.98 L 51.13 50.87 L 51.16 50.76 L 51.19 50.65 L 51.21 50.54 L 51.21 50.43 L 51.21 50.32 L 51.20 50.20 L 51.18 50.09 L 51.15 49.98 L 51.11 49.87 L 51.06 49.77 L 51.00 49.67 L 50.94 49.58 L 50.86 49.49 L 50.77 49.41 L 50.68 49.33 L 50.58 49.27 L 50.47 49.22 L 50.36 49.18 L 50.24 49.15 L 50.12 49.13 L 50.00 49.13 Z" fill="#141414"/>
  <circle cx="50" cy="50" r="40" stroke="#171717" stroke-width="1" fill="none" />
</svg>
)SVG");
}

// A fourth built-in face: a honeycomb.  Like the dials above it is greyscale,
// so recolor() maps it onto the two settings colours: the lit wax walls come
// out in the face colour and the cell openings in the wire colour.  Lighting
// runs along the top-left/bottom-right diagonal to give that mapping a wide
// range to work with, which is the same job the gradient does on the dials.
//
// The cells are cut to the disc here rather than left to a clip-path, and are
// filled flat rather than with a gradient, because Qt renders SVG Tiny and has
// neither.  The geometry is generated, so the tiling is regular to the pixel.
inline QByteArray honeycombFaceSvg()
{
    return QByteArrayLiteral(
        R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">
  <circle cx="50" cy="50" r="40.0" fill="#000000" />
  <polygon points="47.4,10.1 50.0,10.0 52.6,10.1 53.2,10.1 50.0,12.0 46.8,10.1" fill="#1f1f1f" />
  <polygon points="30.3,23.4 23.9,19.7 25.6,18.3 27.8,16.7 30.0,15.4 32.3,14.1 34.7,13.0 36.8,12.2 36.8,19.6" fill="#212121" />
  <polygon points="43.4,23.4 36.8,19.6 36.8,12.2 37.1,12.1 39.6,11.4 42.2,10.8 44.8,10.3 46.8,10.1 50.0,12.0 50.0,19.6" fill="#1e1e1e" />
  <polygon points="56.6,23.4 50.0,19.6 50.0,12.0 53.2,10.1 55.2,10.3 57.8,10.8 60.4,11.4 62.9,12.1 63.2,12.2 63.2,19.6" fill="#1a1a1a" />
  <polygon points="74.4,18.3 76.1,19.7 69.7,23.4 63.2,19.6 63.2,12.2 65.3,13.0 67.7,14.1 70.0,15.4 72.2,16.7" fill="#171717" />
  <polygon points="13.9,32.9 14.1,32.3 15.4,30.0 16.7,27.8 17.1,27.3 17.1,31.0" fill="#232323" />
  <polygon points="23.7,34.8 17.1,31.0 17.1,27.3 18.3,25.6 19.9,23.6 21.7,21.7 23.6,19.9 23.9,19.7 30.3,23.4 30.3,31.0" fill="#202020" />
  <polygon points="36.8,34.8 30.3,31.0 30.3,23.4 36.8,19.6 43.4,23.4 43.4,31.0" fill="#1c1c1c" />
  <polygon points="50.0,34.8 43.4,31.0 43.4,23.4 50.0,19.6 56.6,23.4 56.6,31.0" fill="#191919" />
  <polygon points="63.2,34.8 56.6,31.0 56.6,23.4 63.2,19.6 69.7,23.4 69.7,31.0" fill="#161616" />
  <polygon points="76.3,34.8 69.7,31.0 69.7,23.4 76.1,19.7 76.4,19.9 78.3,21.7 80.1,23.6 81.7,25.6 82.9,27.3 82.9,31.0" fill="#121212" />
  <polygon points="83.3,27.8 84.6,30.0 85.9,32.3 86.1,32.9 82.9,31.0 82.9,27.3" fill="#0f0f0f" />
  <polygon points="17.1,46.2 10.7,42.5 10.8,42.2 11.4,39.6 12.1,37.1 13.0,34.7 13.9,32.9 17.1,31.0 23.7,34.8 23.7,42.4" fill="#1f1f1f" />
  <polygon points="30.3,46.2 23.7,42.4 23.7,34.8 30.3,31.0 36.8,34.8 36.8,42.4" fill="#1b1b1b" />
  <polygon points="43.4,46.2 36.8,42.4 36.8,34.8 43.4,31.0 50.0,34.8 50.0,42.4" fill="#181818" />
  <polygon points="56.6,46.2 50.0,42.4 50.0,34.8 56.6,31.0 63.2,34.8 63.2,42.4" fill="#141414" />
  <polygon points="69.7,46.2 63.2,42.4 63.2,34.8 69.7,31.0 76.3,34.8 76.3,42.4" fill="#111111" />
  <polygon points="88.6,39.6 89.2,42.2 89.3,42.5 82.9,46.2 76.3,42.4 76.3,34.8 82.9,31.0 86.1,32.9 87.0,34.7 87.9,37.1" fill="#0d0d0d" />
  <polygon points="10.7,57.5 10.3,55.2 10.1,52.6 10.0,50.0 10.1,47.4 10.3,44.8 10.7,42.5 17.1,46.2 17.1,53.8" fill="#1d1d1d" />
  <polygon points="23.7,57.6 17.1,53.8 17.1,46.2 23.7,42.4 30.3,46.2 30.3,53.8" fill="#1a1a1a" />
  <polygon points="36.8,57.6 30.3,53.8 30.3,46.2 36.8,42.4 43.4,46.2 43.4,53.8" fill="#161616" />
  <polygon points="50.0,57.6 43.4,53.8 43.4,46.2 50.0,42.4 56.6,46.2 56.6,53.8" fill="#131313" />
  <polygon points="63.2,57.6 56.6,53.8 56.6,46.2 63.2,42.4 69.7,46.2 69.7,53.8" fill="#101010" />
  <polygon points="76.3,57.6 69.7,53.8 69.7,46.2 76.3,42.4 82.9,46.2 82.9,53.8" fill="#0c0c0c" />
  <polygon points="89.7,44.8 89.9,47.4 90.0,50.0 89.9,52.6 89.7,55.2 89.3,57.5 82.9,53.8 82.9,46.2 89.3,42.5" fill="#090909" />
  <polygon points="17.1,69.0 13.9,67.1 13.0,65.3 12.1,62.9 11.4,60.4 10.8,57.8 10.7,57.5 17.1,53.8 23.7,57.6 23.7,65.2" fill="#191919" />
  <polygon points="30.3,69.0 23.7,65.2 23.7,57.6 30.3,53.8 36.8,57.6 36.8,65.2" fill="#151515" />
  <polygon points="43.4,69.0 36.8,65.2 36.8,57.6 43.4,53.8 50.0,57.6 50.0,65.2" fill="#121212" />
  <polygon points="56.6,69.0 50.0,65.2 50.0,57.6 56.6,53.8 63.2,57.6 63.2,65.2" fill="#0e0e0e" />
  <polygon points="69.7,69.0 63.2,65.2 63.2,57.6 69.7,53.8 76.3,57.6 76.3,65.2" fill="#0b0b0b" />
  <polygon points="89.2,57.8 88.6,60.4 87.9,62.9 87.0,65.3 86.1,67.1 82.9,69.0 76.3,65.2 76.3,57.6 82.9,53.8 89.3,57.5" fill="#070707" />
  <polygon points="16.7,72.2 15.4,70.0 14.1,67.7 13.9,67.1 17.1,69.0 17.1,72.7" fill="#171717" />
  <polygon points="23.9,80.3 23.6,80.1 21.7,78.3 19.9,76.4 18.3,74.4 17.1,72.7 17.1,69.0 23.7,65.2 30.3,69.0 30.3,76.6" fill="#141414" />
  <polygon points="36.8,80.4 30.3,76.6 30.3,69.0 36.8,65.2 43.4,69.0 43.4,76.6" fill="#101010" />
  <polygon points="50.0,80.4 43.4,76.6 43.4,69.0 50.0,65.2 56.6,69.0 56.6,76.6" fill="#0d0d0d" />
  <polygon points="63.2,80.4 56.6,76.6 56.6,69.0 63.2,65.2 69.7,69.0 69.7,76.6" fill="#0a0a0a" />
  <polygon points="81.7,74.4 80.1,76.4 78.3,78.3 76.4,80.1 76.1,80.3 69.7,76.6 69.7,69.0 76.3,65.2 82.9,69.0 82.9,72.7" fill="#060606" />
  <polygon points="85.9,67.7 84.6,70.0 83.3,72.2 82.9,72.7 82.9,69.0 86.1,67.1" fill="#030303" />
  <polygon points="34.7,87.0 32.3,85.9 30.0,84.6 27.8,83.3 25.6,81.7 23.9,80.3 30.3,76.6 36.8,80.4 36.8,87.8" fill="#0f0f0f" />
  <polygon points="46.8,89.9 44.8,89.7 42.2,89.2 39.6,88.6 37.1,87.9 36.8,87.8 36.8,80.4 43.4,76.6 50.0,80.4 50.0,88.0" fill="#0c0c0c" />
  <polygon points="62.9,87.9 60.4,88.6 57.8,89.2 55.2,89.7 53.2,89.9 50.0,88.0 50.0,80.4 56.6,76.6 63.2,80.4 63.2,87.8" fill="#080808" />
  <polygon points="74.4,81.7 72.2,83.3 70.0,84.6 67.7,85.9 65.3,87.0 63.2,87.8 63.2,80.4 69.7,76.6 76.1,80.3" fill="#050505" />
  <polygon points="52.6,89.9 50.0,90.0 47.4,89.9 46.8,89.9 50.0,88.0 53.2,89.9" fill="#070707" />
  <polygon points="50.0,10.0 51.5,10.1 50.5,10.6 49.5,10.0" fill="#e9e9e9" />
  <polygon points="30.8,22.0 26.1,19.3 26.1,18.0 27.8,16.7 30.0,15.4 32.3,14.1 34.4,13.2 35.5,13.8 35.5,19.3" fill="#f3f3f3" />
  <polygon points="44.0,22.0 39.2,19.3 39.2,13.8 44.0,11.1 48.7,13.8 48.7,19.3" fill="#e3e3e3" />
  <polygon points="57.1,22.0 52.4,19.3 52.4,13.8 57.1,11.1 61.9,13.8 61.9,19.3" fill="#d4d4d4" />
  <polygon points="70.3,22.0 65.6,19.3 65.6,13.8 66.2,13.4 67.7,14.1 70.0,15.4 72.2,16.7 74.4,18.3 75.0,18.8 75.0,19.3" fill="#c5c5c5" />
  <polygon points="14.6,31.4 15.4,30.0 15.8,29.3 15.8,30.7" fill="#fcfcfc" />
  <polygon points="24.2,33.4 19.5,30.7 19.5,25.2 24.2,22.5 29.0,25.2 29.0,30.7" fill="#ededed" />
  <polygon points="37.4,33.4 32.6,30.7 32.6,25.2 37.4,22.5 42.1,25.2 42.1,30.7" fill="#dedede" />
  <polygon points="50.5,33.4 45.8,30.7 45.8,25.2 50.5,22.5 55.3,25.2 55.3,30.7" fill="#cfcfcf" />
  <polygon points="63.7,33.4 59.0,30.7 59.0,25.2 63.7,22.5 68.5,25.2 68.5,30.7" fill="#bfbfbf" />
  <polygon points="76.9,33.4 72.1,30.7 72.1,25.2 76.9,22.5 81.2,25.0 81.6,25.5 81.6,30.7" fill="#b0b0b0" />
  <polygon points="17.6,44.8 12.9,42.1 12.9,36.6 17.6,33.9 22.4,36.6 22.4,42.1" fill="#e8e8e8" />
  <polygon points="30.8,44.8 26.1,42.1 26.1,36.6 30.8,33.9 35.5,36.6 35.5,42.1" fill="#d8d8d8" />
  <polygon points="44.0,44.8 39.2,42.1 39.2,36.6 44.0,33.9 48.7,36.6 48.7,42.1" fill="#c9c9c9" />
  <polygon points="57.1,44.8 52.4,42.1 52.4,36.6 57.1,33.9 61.9,36.6 61.9,42.1" fill="#bababa" />
  <polygon points="70.3,44.8 65.6,42.1 65.6,36.6 70.3,33.9 75.0,36.6 75.0,42.1" fill="#aaaaaa" />
  <polygon points="83.5,44.8 78.7,42.1 78.7,36.6 83.5,33.9 87.5,36.2 87.9,37.1 88.2,38.2 88.2,42.1" fill="#9b9b9b" />
  <polygon points="11.1,56.2 10.4,55.9 10.3,55.2 10.1,52.6 10.0,50.0 10.1,47.4 10.2,45.7 11.1,45.3 15.8,48.0 15.8,53.5" fill="#e2e2e2" />
  <polygon points="24.2,56.2 19.5,53.5 19.5,48.0 24.2,45.3 29.0,48.0 29.0,53.5" fill="#d3d3d3" />
  <polygon points="37.4,56.2 32.6,53.5 32.6,48.0 37.4,45.3 42.1,48.0 42.1,53.5" fill="#c3c3c3" />
  <polygon points="50.5,56.2 45.8,53.5 45.8,48.0 50.5,45.3 55.3,48.0 55.3,53.5" fill="#b4b4b4" />
  <polygon points="63.7,56.2 59.0,53.5 59.0,48.0 63.7,45.3 68.5,48.0 68.5,53.5" fill="#a5a5a5" />
  <polygon points="76.9,56.2 72.1,53.5 72.1,48.0 76.9,45.3 81.6,48.0 81.6,53.5" fill="#959595" />
  <polygon points="89.9,47.4 90.0,50.0 89.9,52.6 89.7,55.2 89.5,55.9 85.3,53.5 85.3,48.0 89.7,45.5" fill="#868686" />
  <polygon points="17.6,67.6 12.9,64.9 12.9,59.4 17.6,56.7 22.4,59.4 22.4,64.9" fill="#cdcdcd" />
  <polygon points="30.8,67.6 26.1,64.9 26.1,59.4 30.8,56.7 35.5,59.4 35.5,64.9" fill="#bebebe" />
  <polygon points="44.0,67.6 39.2,64.9 39.2,59.4 44.0,56.7 48.7,59.4 48.7,64.9" fill="#aeaeae" />
  <polygon points="57.1,67.6 52.4,64.9 52.4,59.4 57.1,56.7 61.9,59.4 61.9,64.9" fill="#9f9f9f" />
  <polygon points="70.3,67.6 65.6,64.9 65.6,59.4 70.3,56.7 75.0,59.4 75.0,64.9" fill="#909090" />
  <polygon points="87.9,62.9 87.0,65.3 86.8,65.7 83.5,67.6 78.7,64.9 78.7,59.4 83.5,56.7 88.2,59.4 88.2,61.8" fill="#808080" />
  <polygon points="24.2,79.0 20.3,76.7 19.9,76.4 19.5,75.8 19.5,70.8 24.2,68.1 29.0,70.8 29.0,76.3" fill="#b8b8b8" />
  <polygon points="37.4,79.0 32.6,76.3 32.6,70.8 37.4,68.1 42.1,70.8 42.1,76.3" fill="#a9a9a9" />
  <polygon points="50.5,79.0 45.8,76.3 45.8,70.8 50.5,68.1 55.3,70.8 55.3,76.3" fill="#999999" />
  <polygon points="63.7,79.0 59.0,76.3 59.0,70.8 63.7,68.1 68.5,70.8 68.5,76.3" fill="#8a8a8a" />
  <polygon points="80.1,76.4 78.4,78.1 76.9,79.0 72.1,76.3 72.1,70.8 76.9,68.1 81.6,70.8 81.6,74.5" fill="#7b7b7b" />
  <polygon points="34.7,87.0 32.3,85.9 30.0,84.6 27.8,83.3 26.2,82.1 30.8,79.5 35.5,82.2 35.5,87.3" fill="#a3a3a3" />
  <polygon points="45.2,89.7 44.8,89.7 42.2,89.2 41.7,89.1 39.2,87.7 39.2,82.2 44.0,79.5 48.7,82.2 48.7,87.7" fill="#949494" />
  <polygon points="60.1,88.7 57.8,89.2 55.7,89.6 52.4,87.7 52.4,82.2 57.1,79.5 61.9,82.2 61.9,87.7" fill="#858585" />
  <polygon points="72.2,83.3 70.0,84.6 67.7,85.9 65.6,86.8 65.6,82.2 70.3,79.5 74.3,81.8" fill="#757575" />
  <circle cx="50" cy="50" r="40" fill="none" stroke="#000000" stroke-width="3" />
</svg>)SVG");
}

// A sixth built-in face: a kaleidoscope, and the only one not written out
// here but generated when it is asked for.  It is also the only one in full
// colour, so it is meant to be drawn in Original mode; recolor() would flatten
// it back to two tones, which is the one thing this face is not.
//
// The design is random, and so it needs somewhere to keep which random.  That
// is the seed: the config stores "builtin:kaleidoscope:12345" and the face is
// rebuilt from those digits every time it is drawn.  Storing the seed rather
// than the drawing is what lets a two-word setting survive a save, a reload
// and a copy to another clock, and it costs a few hundred microseconds of
// arithmetic to redraw.  The generator is a plain xorshift written out below
// rather than QRandomGenerator, because the seed is saved in the user's config
// and must still mean the same picture after a Qt upgrade.
//
// Two things keep the artwork inside the dial without a clip path, which SVG
// Tiny does not have.  The bands are whole circles drawn largest first, each
// one painting over the middle of the last, so the outer edge is a true circle
// rather than a ring of arcs that have to meet.  The motif is polygons whose
// every vertex is inside the disc -- and a disc being convex, a polygon with
// its corners inside it lies inside it entirely.
//
// The symmetry is the ordinary kaleidoscope one: a motif drawn in half a
// wedge, mirrored onto the other half, and the pair repeated around the
// centre.  An even number of wedges is picked so the mirrors line up opposite
// each other.
//
// faceHex is the colour the fills are built from and wireHex the colour of the
// lines between them, and multiHue decides which of the two things the face
// colour means.  Set, the palette is a scheme of several hues -- analogous,
// triadic, or right round the wheel -- laid out around that colour, which is
// how the face comes out looking like a kaleidoscope.  Clear, the palette is
// that one colour and its shades, and the face comes out in two colours.
//
// Either way the seed decides the pattern alone.  Recolouring a face never
// moves a shape, so a pattern can be settled on and then painted, or a colour
// settled on and the pattern rerolled under it.
inline QByteArray kaleidoscopeFaceSvg(quint64 seed, const QString &faceHex,
                                      const QString &wireHex, bool multiHue)
{
    struct Rng {
        quint64 s;
        explicit Rng(quint64 seed)
            : s(seed * 2685821657736338717ULL + 1442695040888963407ULL)
        {
            if (s == 0)
                s = 0x9E3779B97F4A7C15ULL;
        }
        quint64 next()
        {
            s ^= s << 13;
            s ^= s >> 7;
            s ^= s << 17;
            return s * 0x2545F4914F6CDD1DULL;
        }
        double unit() { return double(next() >> 11) / double(1ULL << 53); }
        int range(int lo, int hi) { return lo + int(next() % quint64(hi - lo + 1)); }
        double uni(double lo, double hi) { return lo + unit() * (hi - lo); }
    } rng(seed);

    const double kR = 40.0, kC = 50.0;
    const auto hex = [](double h, double s, double v) {
        h -= std::floor(h);
        return QColor::fromHsvF(qBound(0.0, h, 0.999999), qBound(0.0, s, 1.0),
                                qBound(0.0, v, 1.0))
            .name();
    };

    // One scheme and one base hue, so the colours belong together however wild
    // the pattern is.  Hues drawn independently give mud, so the base hue is
    // the face colour's and the seed decides only how the rest are spaced from
    // it.
    //
    // The rolls happen either way, multi-hue or not, so that the seed keeps
    // meaning the same shape: changing how a face is coloured must not move
    // anything in it.
    QVector<QString> pal;
    {
        // A grey has no hue at all and reports -1, which would wrap round to
        // red.  Red is as good a base as any to build on, and the saturation
        // stays where it is, so a grey still comes out grey.
        const QColor named = QColor(faceHex).isValid() ? QColor(faceHex) : QColor(Qt::white);
        const double base = std::max(0.0, double(named.hueF()));
        const double sat = double(named.saturationF());
        const double val = double(named.valueF());

        QVector<double> hues;
        switch (rng.range(0, 3)) {
        case 0: {  // analogous
            const double spread = rng.uni(0.06, 0.14);
            for (int i = 0; i < 5; ++i)
                hues.append(base + (i - 2) * spread);
            break;
        }
        case 1:  // triadic, with two shades between
            hues = {base, base + 1.0 / 3.0, base + 2.0 / 3.0, base + 1.0 / 6.0, base + 0.5};
            break;
        case 2:  // a complementary pair, each split
            hues = {base, base + 0.5, base + 0.06, base + 0.56, base + 0.5};
            break;
        default:  // right round the wheel
            for (int i = 0; i < 5; ++i)
                hues.append(base + i / 5.0);
            break;
        }
        // Tone is dealt out rather than rolled per colour, so every palette
        // has a dark end and a light one and no face can come out as five
        // shades of the same thing.  Which hue gets which tone is shuffled.
        double vs[5] = {0.34, 0.50, 0.64, 0.78, 0.92};
        for (int i = 4; i > 0; --i)
            std::swap(vs[i], vs[rng.range(0, i)]);
        QVector<double> sats;
        for (int i = 0; i < hues.size(); ++i)
            sats.append(rng.uni(0.55, 1.0));

        for (int i = 0; i < hues.size(); ++i) {
            if (multiHue) {
                pal.append(hex(hues[i], std::max(0.35, sats[i] * (0.45 + 0.55 * sat)), vs[i]));
            } else {
                // One colour has to fill the same five slots, so it is the tone
                // ladder that does the work and the hue that stays put.  The
                // saturation moves a little with the tone -- a shade that is
                // only darker reads as the same colour under a shadow, where
                // one that is also less saturated reads as a colour of its own.
                //
                // The spread is about the colour's own tone rather than about
                // the middle, so a pale colour gives pale shades and a deep one
                // deep shades, and the colour picked is recognisably the one on
                // the dial.
                const double t = (vs[i] - 0.63) / 0.29;  // roughly -1 .. +1
                pal.append(hex(base, qBound(0.12, sat * (1.0 - 0.35 * t), 1.0),
                               qBound(0.06, val + 0.30 * t, 1.0)));
            }
        }
    }

    // The line work between the shapes.  It is always the wire colour: line
    // work has to read as line work, and a line in one of the five fill colours
    // disappears the moment it lands next to that fill, which is exactly what
    // picking it out of the palette would do.
    const QColor namedWire(wireHex);
    const QString lineColor = namedWire.isValid() ? namedWire.name() : QStringLiteral("#000000");
    // Width grows with the radius the line is drawn at, so the pattern is
    // pencilled in at the hub and inked at the rim.  Without that the middle,
    // where every wedge's shapes crowd together, fills in solid.
    const double lineBase = rng.uni(0.16, 0.34);
    const auto strokeAt = [&](double radius) {
        const double w = lineBase * (0.10 + 0.90 * std::pow(radius / kR, 1.7))
                         * rng.uni(0.75, 1.3);
        return QStringLiteral(" stroke=\"%1\" stroke-width=\"%2\"")
            .arg(lineColor, QString::number(qBound(0.06, w, 1.00), 'f', 3));
    };

    QStringList parts;
    QStringList lines;  // drawn last, over everything, so no fill can cover them

    // The bands, outermost first.  Drawn as full circles rather than rings,
    // so the rim needs no arcs to close it and cannot show a seam.
    QVector<double> edges;
    {
        const int bands = rng.range(3, 5);
        for (int i = 1; i < bands; ++i)
            edges.append(kR * (double(i) / bands) * rng.uni(0.8, 1.2));
        edges.append(kR);
        std::sort(edges.begin(), edges.end());
        int last = -1;
        for (int i = edges.size() - 1; i >= 0; --i) {
            int c = rng.range(0, pal.size() - 1);
            if (c == last)  // never two bands running in the same colour
                c = (c + 1 + rng.range(0, pal.size() - 2)) % pal.size();
            last = c;
            parts << QStringLiteral("<circle cx=\"50\" cy=\"50\" r=\"%1\" fill=\"%2\"/>")
                         .arg(QString::number(edges[i], 'f', 2), pal[c]);
        }
    }

    // The motif, in half a wedge: radius and angle for each corner, kept in
    // polar form so mirroring is a change of sign.
    const int wedges = 6 + 2 * rng.range(0, 3);  // 6, 8, 10 or 12
    const double wedge = 2.0 * M_PI / wedges;
    struct Shape {
        QVector<QPointF> polar;  // x = radius, y = angle
        QString fill;
    };
    QVector<Shape> motif;
    for (int i = 0, n = rng.range(6, 9); i < n; ++i) {
        const double r0 = rng.uni(0.05, 0.95) * kR;
        const double r1 = std::min(kR, r0 + rng.uni(0.12, 0.45) * kR);
        const double a0 = rng.uni(0.0, wedge / 2.0);
        const double a1 = rng.uni(0.0, wedge / 2.0);
        Shape shape;
        switch (rng.range(0, 2)) {
        case 0:  // a spike out from the mirror line
            shape.polar = {{r0, 0.0}, {r1, a0}, {r1, a1}};
            break;
        case 1:  // a panel spanning the band
            shape.polar = {{r0, a0}, {r1, a0}, {r1, a1}, {r0, a1}};
            break;
        default:  // a kite sitting on the mirror line
            shape.polar = {{r0, 0.0}, {(r0 + r1) / 2.0, std::max(a0, a1)}, {r1, 0.0}};
            break;
        }
        shape.fill = pal[rng.range(0, pal.size() - 1)];
        motif.append(shape);
    }

    for (int k = 0; k < wedges; ++k) {
        const double base = k * wedge - M_PI / 2.0;
        for (const double mirror : {1.0, -1.0}) {
            for (const Shape &shape : motif) {
                QStringList pts;
                double far = 0.0;
                for (const QPointF &p : shape.polar) {
                    const double a = base + mirror * p.y();
                    far = std::max(far, p.x());
                    pts << QStringLiteral("%1,%2")
                               .arg(QString::number(kC + p.x() * std::cos(a), 'f', 2),
                                    QString::number(kC + p.x() * std::sin(a), 'f', 2));
                }
                const QString points = pts.join(QLatin1Char(' '));
                parts << QStringLiteral("<polygon points=\"%1\" fill=\"%2\"/>").arg(points,
                                                                                    shape.fill);
                // The outline goes in the top layer rather than on the polygon
                // itself.  A shape drawn later would otherwise cover the
                // outline of the one before it, and half an outline round a
                // shape looks like a mistake rather than a line.
                lines << QStringLiteral("<polygon points=\"%1\" fill=\"none\"%2/>")
                             .arg(points, strokeAt(far));
            }
        }
    }

    // The band edges last of all, so the rings read as whole circles rather
    // than as the gaps between whatever the motif left showing.
    for (const double r : edges)
        lines << QStringLiteral("<circle cx=\"50\" cy=\"50\" r=\"%1\" fill=\"none\"%2/>")
                     .arg(QString::number(r, 'f', 2), strokeAt(r));

    parts += lines;

    return QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" "
                          "height=\"100\">\n%1\n</svg>\n")
        .arg(parts.join(QLatin1Char('\n')))
        .toUtf8();
}

inline const char *aboutText()
{
    return "A transparent, borderless analog clock for the desktop.\n\n"
           "The clock face can be any SVG. In Recolor mode white is treated as "
           "the face color and black as the wire color, and both can be set from "
           "Settings; in Original mode the artwork is drawn as it was authored, "
           "which is what a full-color picture wants.\n\n"
           "Toolbar icons are from Font Awesome Free 6.7.2 by @fontawesome "
           "(https://fontawesome.com), used under CC BY 4.0 "
           "(https://creativecommons.org/licenses/by/4.0/) and recolored to "
           "match the dialogs they appear in.";
}
