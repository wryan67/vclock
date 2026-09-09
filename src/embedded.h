// Artwork and text that ship inside the program, so it has no external
// dependency on files or an icon theme.
#pragma once

#include <QByteArray>

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
// Three arms, not one.  A single Archimedean arm wound tightly enough to fill
// the disc does not read as a spiral at all: with the arm and the gap between
// its turns much the same width, the eye gives up on the one continuous curve
// and sees concentric rings instead, and at the sizes a desktop clock actually
// runs at -- 64 to 128 px -- the result is a bullseye.  Three arms of a turn
// and a quarter each break that reading, because the arms are steep enough to
// cross the radius rather than hug it, and the rotation becomes the obvious
// thing about the shape.
//
// Each arm tapers, half a unit at the hub to five at its outer end, which is
// what makes the direction of the sweep legible: the eye follows a wedge from
// its point to its base.  A taper cannot be stroked -- SVG Tiny has no
// variable-width stroke -- so each arm is a filled outline, walked up one edge
// and back down the other, with a circle dropped on the outer end to round off
// what would otherwise be a blunt cut.  Ninety-six samples an arm keeps the
// flats below a third of a pixel even on the largest clock the app will build.
//
// The arms stop at r=36 so the ring of dial inside the rim stays unbroken, and
// the marks -- pushed out past the rim by the preset -- never sit on one.
inline QByteArray spiralFaceSvg()
{
    return QByteArrayLiteral(
        R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">
  <circle cx="50" cy="50" r="40" fill="#e8e8e8" />
  <path d="M 50.25 50.00 L 50.30 49.67 L 50.40 49.36 L 50.56 49.06 L 50.76 48.79 L 51.00 48.54 L 51.29 48.33 L 51.61 48.16 L 51.97 48.03 L 52.36 47.95 L 52.77 47.92 L 53.20 47.94 L 53.64 48.02 L 54.09 48.16 L 54.54 48.36 L 54.99 48.62 L 55.41 48.94 L 55.82 49.33 L 56.20 49.77 L 56.55 50.27 L 56.85 50.82 L 57.10 51.43 L 57.30 52.08 L 57.43 52.78 L 57.50 53.51 L 57.50 54.27 L 57.43 55.05 L 57.27 55.84 L 57.03 56.65 L 56.71 57.45 L 56.30 58.24 L 55.80 59.02 L 55.22 59.76 L 54.56 60.47 L 53.81 61.14 L 52.99 61.75 L 52.09 62.29 L 51.12 62.77 L 50.08 63.17 L 48.99 63.48 L 47.85 63.70 L 46.66 63.82 L 45.44 63.84 L 44.20 63.74 L 42.94 63.54 L 41.68 63.22 L 40.42 62.78 L 39.18 62.22 L 37.97 61.55 L 36.80 60.75 L 35.69 59.84 L 34.63 58.82 L 33.64 57.69 L 32.74 56.46 L 31.94 55.13 L 31.24 53.71 L 30.65 52.21 L 30.18 50.64 L 29.85 49.01 L 29.65 47.33 L 29.59 45.60 L 29.68 43.85 L 29.92 42.09 L 30.32 40.33 L 30.87 38.58 L 31.58 36.85 L 32.44 35.17 L 33.46 33.54 L 34.63 31.98 L 35.94 30.50 L 37.40 29.12 L 38.99 27.85 L 40.70 26.70 L 42.53 25.68 L 44.46 24.81 L 46.48 24.09 L 48.59 23.54 L 50.76 23.16 L 52.99 22.97 L 55.25 22.95 L 57.53 23.13 L 59.82 23.51 L 62.10 24.08 L 64.34 24.85 L 66.55 25.81 L 68.69 26.97 L 70.75 28.32 L 72.71 29.85 L 74.56 31.56 L 76.28 33.44 L 77.85 35.47 L 79.27 37.66 L 80.51 39.98 L 81.57 42.42 L 82.43 44.97 L 83.08 47.60 L 83.52 50.32 L 88.48 49.68 L 87.93 46.57 L 87.13 43.55 L 86.09 40.63 L 84.83 37.84 L 83.35 35.20 L 81.68 32.71 L 79.82 30.39 L 77.80 28.26 L 75.64 26.33 L 73.35 24.61 L 70.95 23.10 L 68.45 21.81 L 65.89 20.74 L 63.29 19.90 L 60.65 19.29 L 58.00 18.91 L 55.36 18.75 L 52.75 18.82 L 50.18 19.09 L 47.68 19.58 L 45.26 20.27 L 42.94 21.14 L 40.73 22.20 L 38.64 23.41 L 36.68 24.79 L 34.88 26.30 L 33.23 27.93 L 31.74 29.68 L 30.43 31.52 L 29.29 33.43 L 28.34 35.41 L 27.56 37.42 L 26.97 39.47 L 26.56 41.53 L 26.33 43.58 L 26.28 45.62 L 26.39 47.62 L 26.68 49.57 L 27.12 51.46 L 27.71 53.27 L 28.44 55.00 L 29.30 56.63 L 30.29 58.15 L 31.38 59.56 L 32.56 60.84 L 33.83 62.00 L 35.17 63.02 L 36.57 63.91 L 38.01 64.66 L 39.48 65.26 L 40.97 65.73 L 42.46 66.06 L 43.95 66.25 L 45.41 66.31 L 46.85 66.24 L 48.24 66.04 L 49.58 65.73 L 50.85 65.32 L 52.06 64.80 L 53.18 64.19 L 54.22 63.50 L 55.17 62.73 L 56.02 61.90 L 56.78 61.02 L 57.43 60.10 L 57.97 59.15 L 58.42 58.18 L 58.76 57.20 L 59.00 56.22 L 59.13 55.25 L 59.17 54.30 L 59.12 53.38 L 58.99 52.50 L 58.77 51.66 L 58.48 50.88 L 58.12 50.16 L 57.70 49.50 L 57.23 48.91 L 56.72 48.39 L 56.18 47.95 L 55.61 47.59 L 55.02 47.31 L 54.43 47.10 L 53.84 46.98 L 53.26 46.93 L 52.70 46.95 L 52.18 47.05 L 51.68 47.21 L 51.23 47.43 L 50.83 47.70 L 50.49 48.02 L 50.21 48.37 L 49.99 48.76 L 49.84 49.16 L 49.76 49.58 L 49.75 50.00 Z" fill="#141414"/>
  <circle cx="86.00" cy="50.00" r="2.50" fill="#141414"/>
  <path d="M 49.88 50.22 L 50.13 50.42 L 50.36 50.67 L 50.54 50.95 L 50.67 51.26 L 50.76 51.60 L 50.80 51.95 L 50.79 52.32 L 50.72 52.69 L 50.60 53.07 L 50.42 53.44 L 50.18 53.80 L 49.89 54.14 L 49.55 54.46 L 49.15 54.75 L 48.70 55.01 L 48.21 55.22 L 47.67 55.38 L 47.10 55.49 L 46.49 55.53 L 45.86 55.52 L 45.21 55.43 L 44.55 55.28 L 43.88 55.05 L 43.21 54.75 L 42.55 54.36 L 41.91 53.91 L 41.30 53.37 L 40.73 52.76 L 40.19 52.08 L 39.71 51.33 L 39.29 50.52 L 38.93 49.64 L 38.65 48.71 L 38.45 47.73 L 38.33 46.71 L 38.31 45.66 L 38.38 44.58 L 38.55 43.49 L 38.83 42.38 L 39.21 41.28 L 39.70 40.20 L 40.30 39.13 L 41.00 38.10 L 41.80 37.12 L 42.71 36.18 L 43.72 35.32 L 44.82 34.52 L 46.01 33.81 L 47.29 33.20 L 48.63 32.68 L 50.05 32.28 L 51.52 31.99 L 53.03 31.83 L 54.59 31.79 L 56.17 31.90 L 57.76 32.14 L 59.35 32.52 L 60.94 33.04 L 62.49 33.71 L 64.01 34.52 L 65.48 35.47 L 66.89 36.57 L 68.22 37.79 L 69.46 39.14 L 70.60 40.62 L 71.63 42.21 L 72.53 43.91 L 73.29 45.70 L 73.92 47.58 L 74.39 49.53 L 74.69 51.54 L 74.83 53.60 L 74.80 55.69 L 74.59 57.80 L 74.20 59.91 L 73.62 62.01 L 72.86 64.08 L 71.92 66.10 L 70.80 68.07 L 69.50 69.95 L 68.03 71.75 L 66.40 73.44 L 64.61 75.00 L 62.67 76.42 L 60.60 77.70 L 58.41 78.81 L 56.10 79.74 L 53.69 80.49 L 51.21 81.04 L 48.65 81.38 L 46.05 81.52 L 43.42 81.44 L 40.78 81.13 L 38.14 80.60 L 35.53 79.85 L 32.97 78.87 L 31.03 83.48 L 34.00 84.56 L 37.02 85.38 L 40.07 85.94 L 43.11 86.24 L 46.15 86.28 L 49.14 86.08 L 52.07 85.63 L 54.92 84.95 L 57.68 84.04 L 60.32 82.92 L 62.83 81.59 L 65.19 80.08 L 67.39 78.40 L 69.42 76.55 L 71.27 74.57 L 72.93 72.47 L 74.38 70.26 L 75.63 67.97 L 76.67 65.61 L 77.50 63.20 L 78.12 60.76 L 78.52 58.31 L 78.72 55.87 L 78.71 53.45 L 78.49 51.07 L 78.09 48.76 L 77.49 46.51 L 76.73 44.35 L 75.79 42.30 L 74.70 40.35 L 73.47 38.54 L 72.11 36.86 L 70.63 35.32 L 69.06 33.94 L 67.39 32.71 L 65.66 31.65 L 63.87 30.75 L 62.04 30.02 L 60.18 29.46 L 58.31 29.06 L 56.45 28.83 L 54.61 28.76 L 52.80 28.85 L 51.03 29.09 L 49.33 29.48 L 47.69 30.00 L 46.13 30.65 L 44.67 31.41 L 43.30 32.29 L 42.04 33.26 L 40.89 34.32 L 39.86 35.44 L 38.95 36.64 L 38.17 37.88 L 37.52 39.15 L 36.99 40.45 L 36.59 41.77 L 36.31 43.08 L 36.16 44.38 L 36.12 45.66 L 36.20 46.91 L 36.39 48.11 L 36.68 49.26 L 37.07 50.36 L 37.54 51.38 L 38.09 52.33 L 38.71 53.20 L 39.39 53.99 L 40.12 54.68 L 40.89 55.29 L 41.69 55.80 L 42.51 56.21 L 43.34 56.53 L 44.17 56.76 L 45.00 56.90 L 45.80 56.95 L 46.58 56.92 L 47.33 56.81 L 48.03 56.63 L 48.69 56.37 L 49.28 56.06 L 49.82 55.70 L 50.29 55.28 L 50.70 54.84 L 51.03 54.36 L 51.29 53.87 L 51.47 53.36 L 51.58 52.85 L 51.61 52.35 L 51.58 51.87 L 51.47 51.42 L 51.31 50.99 L 51.08 50.61 L 50.80 50.28 L 50.48 50.00 L 50.12 49.78 Z" fill="#141414"/>
  <circle cx="32.00" cy="81.18" r="2.50" fill="#141414"/>
  <path d="M 49.88 49.78 L 49.56 49.90 L 49.24 49.97 L 48.91 49.99 L 48.57 49.95 L 48.24 49.86 L 47.91 49.72 L 47.60 49.52 L 47.31 49.28 L 47.05 48.98 L 46.81 48.64 L 46.62 48.26 L 46.47 47.83 L 46.36 47.38 L 46.31 46.89 L 46.31 46.37 L 46.38 45.84 L 46.50 45.30 L 46.70 44.75 L 46.96 44.20 L 47.29 43.66 L 47.69 43.14 L 48.16 42.64 L 48.69 42.17 L 49.29 41.75 L 49.94 41.37 L 50.66 41.04 L 51.43 40.78 L 52.24 40.59 L 53.10 40.47 L 53.99 40.42 L 54.91 40.47 L 55.84 40.60 L 56.79 40.82 L 57.74 41.13 L 58.68 41.54 L 59.60 42.05 L 60.50 42.65 L 61.36 43.35 L 62.18 44.13 L 62.94 45.01 L 63.64 45.98 L 64.26 47.03 L 64.80 48.15 L 65.26 49.34 L 65.61 50.60 L 65.86 51.90 L 65.99 53.26 L 66.01 54.64 L 65.91 56.05 L 65.68 57.48 L 65.32 58.90 L 64.84 60.32 L 64.22 61.71 L 63.47 63.08 L 62.59 64.39 L 61.59 65.65 L 60.46 66.84 L 59.22 67.95 L 57.86 68.96 L 56.40 69.88 L 54.84 70.67 L 53.19 71.34 L 51.46 71.88 L 49.67 72.28 L 47.82 72.53 L 45.93 72.62 L 44.01 72.56 L 42.08 72.32 L 40.14 71.92 L 38.22 71.35 L 36.32 70.62 L 34.47 69.71 L 32.67 68.63 L 30.95 67.40 L 29.32 66.00 L 27.79 64.45 L 26.38 62.76 L 25.09 60.93 L 23.95 58.98 L 22.97 56.91 L 22.15 54.74 L 21.50 52.48 L 21.05 50.15 L 20.78 47.76 L 20.71 45.33 L 20.85 42.88 L 21.19 40.41 L 21.75 37.95 L 22.52 35.53 L 23.49 33.14 L 24.68 30.82 L 26.06 28.59 L 27.65 26.45 L 29.43 24.43 L 31.38 22.55 L 33.51 20.81 L 30.49 16.83 L 28.07 18.87 L 25.85 21.07 L 23.84 23.43 L 22.06 25.92 L 20.50 28.52 L 19.19 31.21 L 18.11 33.98 L 17.27 36.79 L 16.68 39.63 L 16.34 42.48 L 16.23 45.31 L 16.36 48.12 L 16.71 50.86 L 17.29 53.54 L 18.08 56.13 L 19.08 58.62 L 20.26 60.98 L 21.62 63.21 L 23.14 65.29 L 24.81 67.22 L 26.62 68.97 L 28.54 70.54 L 30.56 71.93 L 32.66 73.13 L 34.82 74.14 L 37.03 74.95 L 39.28 75.56 L 41.53 75.97 L 43.78 76.19 L 46.00 76.22 L 48.19 76.06 L 50.33 75.72 L 52.40 75.21 L 54.38 74.53 L 56.28 73.71 L 58.07 72.74 L 59.74 71.63 L 61.29 70.41 L 62.70 69.09 L 63.98 67.67 L 65.11 66.17 L 66.09 64.61 L 66.92 63.00 L 67.59 61.35 L 68.11 59.68 L 68.48 58.00 L 68.69 56.33 L 68.76 54.68 L 68.69 53.05 L 68.48 51.48 L 68.14 49.95 L 67.67 48.50 L 67.10 47.12 L 66.41 45.82 L 65.64 44.61 L 64.77 43.50 L 63.84 42.50 L 62.84 41.61 L 61.79 40.82 L 60.70 40.15 L 59.58 39.60 L 58.44 39.16 L 57.30 38.83 L 56.16 38.62 L 55.03 38.52 L 53.94 38.52 L 52.87 38.62 L 51.85 38.82 L 50.89 39.10 L 49.98 39.47 L 49.14 39.90 L 48.37 40.41 L 47.67 40.97 L 47.06 41.57 L 46.52 42.22 L 46.08 42.89 L 45.71 43.58 L 45.44 44.28 L 45.25 44.98 L 45.14 45.67 L 45.11 46.35 L 45.16 47.00 L 45.28 47.61 L 45.46 48.19 L 45.71 48.71 L 46.01 49.18 L 46.36 49.59 L 46.74 49.94 L 47.16 50.22 L 47.59 50.43 L 48.04 50.57 L 48.49 50.63 L 48.93 50.63 L 49.36 50.56 L 49.76 50.42 L 50.12 50.22 Z" fill="#141414"/>
  <circle cx="32.00" cy="18.82" r="2.50" fill="#141414"/>
  <circle cx="50" cy="50" r="40" stroke="#171717" stroke-width="3" fill="none" />
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
