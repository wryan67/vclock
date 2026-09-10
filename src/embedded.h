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
// One arm, of a turn and a half, and it ends by becoming the rim.  The radius
// does not climb straight to the edge and stop; over the last eighth of the
// curve it eases to a standstill at r=40, so the arm arrives travelling along
// the rim rather than into it, and then runs on round it for a few more
// degrees.  Since it also tapers to exactly the rim's own width by then, and is
// the same tone, there is no join to see: the arm simply thins away into the
// wall of the dial.  That is the whole trick, and it is why the arm's width and
// the rim's stroke are one number rather than two.
//
// The taper is wide at the hub and fine at the rim, which is what gives the
// sweep a direction -- the eye runs down a wedge from its base to its point --
// and it is also what makes the ending work, since a thick arm could not
// disappear into a thin rim.  SVG Tiny has no variable-width stroke, so the arm
// is a filled outline: up one edge and back down the other, with a circle at
// the hub to round off the wide end.  A hundred and forty samples keeps the
// flats under a third of a pixel on the largest clock the app will build.
//
// The rim is deliberately fine, a single unit.  Everything on this dial is
// meant to sit in one weight -- arm tip, rim and hour marks alike -- so the
// preset picks a mark scale that lands just under it.
inline QByteArray spiralFaceSvg()
{
    return QByteArrayLiteral(
        R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">
  <circle cx="50" cy="50" r="40" fill="#e8e8e8" />
  <path d="M 52.48 47.53 L 53.49 49.95 L 53.46 50.13 L 53.42 50.29 L 53.35 50.44 L 53.28 50.56 L 53.20 50.66 L 53.12 50.74 L 53.05 50.79 L 52.99 50.82 L 52.95 50.84 L 52.92 50.84 L 52.92 50.84 L 52.95 50.84 L 52.99 50.84 L 53.06 50.85 L 53.15 50.87 L 53.27 50.91 L 53.39 50.96 L 53.53 51.04 L 53.68 51.15 L 53.84 51.29 L 54.00 51.45 L 54.16 51.65 L 54.31 51.87 L 54.45 52.13 L 54.58 52.42 L 54.69 52.74 L 54.77 53.09 L 54.84 53.47 L 54.87 53.87 L 54.87 54.29 L 54.83 54.73 L 54.76 55.19 L 54.64 55.66 L 54.48 56.14 L 54.28 56.63 L 54.03 57.11 L 53.73 57.60 L 53.38 58.07 L 52.99 58.54 L 52.54 58.98 L 52.05 59.41 L 51.51 59.81 L 50.93 60.18 L 50.30 60.51 L 49.62 60.81 L 48.91 61.06 L 48.17 61.26 L 47.39 61.41 L 46.58 61.51 L 45.74 61.55 L 44.89 61.53 L 44.01 61.44 L 43.13 61.29 L 42.24 61.06 L 41.35 60.77 L 40.46 60.41 L 39.58 59.97 L 38.72 59.46 L 37.88 58.89 L 37.07 58.23 L 36.29 57.51 L 35.55 56.73 L 34.86 55.87 L 34.21 54.95 L 33.63 53.97 L 33.10 52.93 L 32.64 51.84 L 32.25 50.70 L 31.94 49.52 L 31.71 48.29 L 31.56 47.04 L 31.50 45.75 L 31.53 44.44 L 31.66 43.12 L 31.88 41.79 L 32.19 40.46 L 32.61 39.14 L 33.12 37.83 L 33.73 36.53 L 34.44 35.27 L 35.25 34.04 L 36.16 32.86 L 37.16 31.72 L 38.25 30.64 L 39.42 29.63 L 40.68 28.69 L 42.02 27.83 L 43.44 27.05 L 44.92 26.36 L 46.47 25.77 L 48.07 25.29 L 49.73 24.91 L 51.42 24.64 L 53.15 24.49 L 54.91 24.46 L 56.69 24.54 L 58.47 24.76 L 60.26 25.10 L 62.04 25.57 L 63.80 26.16 L 65.54 26.89 L 67.24 27.74 L 68.89 28.71 L 70.50 29.81 L 72.04 31.03 L 73.50 32.37 L 74.89 33.81 L 76.19 35.37 L 77.38 37.03 L 78.48 38.78 L 79.46 40.62 L 80.32 42.54 L 81.05 44.54 L 81.65 46.60 L 82.12 48.72 L 82.44 50.89 L 82.61 53.09 L 82.63 55.32 L 82.50 57.57 L 82.21 59.82 L 81.77 62.07 L 81.16 64.30 L 80.43 66.53 L 79.59 68.78 L 78.63 71.01 L 77.53 73.21 L 76.27 75.35 L 74.85 77.42 L 73.25 79.39 L 71.47 81.23 L 69.53 82.92 L 67.44 84.44 L 65.21 85.78 L 62.85 86.92 L 60.39 87.85 L 57.86 88.57 L 55.27 89.08 L 52.63 89.39 L 49.98 89.50 L 49.51 89.50 L 49.03 89.49 L 48.55 89.47 L 48.07 89.45 L 47.59 89.43 L 47.10 89.39 L 46.62 89.36 L 46.14 89.31 L 45.66 89.26 L 45.18 89.21 L 44.70 89.14 L 44.23 89.08 L 43.75 89.00 L 43.27 88.92 L 42.80 88.84 L 42.32 88.75 L 41.85 88.65 L 41.38 88.55 L 40.91 88.44 L 40.44 88.33 L 40.21 89.30 L 40.69 89.41 L 41.17 89.53 L 41.65 89.63 L 42.14 89.73 L 42.62 89.82 L 43.11 89.91 L 43.60 89.99 L 44.09 90.07 L 44.58 90.14 L 45.07 90.20 L 45.56 90.26 L 46.05 90.31 L 46.54 90.35 L 47.04 90.39 L 47.53 90.42 L 48.02 90.45 L 48.52 90.47 L 49.01 90.49 L 49.51 90.50 L 50.02 90.50 L 52.75 90.42 L 55.47 90.15 L 58.16 89.66 L 60.80 88.95 L 63.37 88.02 L 65.84 86.86 L 68.20 85.50 L 70.41 83.94 L 72.46 82.19 L 74.35 80.30 L 76.05 78.27 L 77.57 76.13 L 78.92 73.92 L 80.10 71.66 L 81.12 69.37 L 82.02 67.07 L 82.83 64.77 L 83.50 62.43 L 84.01 60.07 L 84.35 57.69 L 84.53 55.32 L 84.55 52.95 L 84.40 50.61 L 84.10 48.31 L 83.65 46.04 L 83.04 43.83 L 82.30 41.68 L 81.41 39.60 L 80.39 37.61 L 79.25 35.70 L 77.99 33.89 L 76.62 32.19 L 75.14 30.60 L 73.58 29.12 L 71.93 27.76 L 70.21 26.53 L 68.42 25.44 L 66.57 24.47 L 64.68 23.64 L 62.75 22.95 L 60.80 22.40 L 58.83 21.98 L 56.86 21.71 L 54.89 21.57 L 52.93 21.57 L 50.99 21.70 L 49.08 21.96 L 47.22 22.35 L 45.40 22.87 L 43.63 23.50 L 41.93 24.24 L 40.31 25.09 L 38.76 26.05 L 37.29 27.09 L 35.91 28.23 L 34.63 29.45 L 33.45 30.74 L 32.37 32.09 L 31.39 33.50 L 30.53 34.96 L 29.77 36.47 L 29.13 38.00 L 28.60 39.56 L 28.19 41.14 L 27.89 42.72 L 27.71 44.31 L 27.63 45.88 L 27.67 47.44 L 27.81 48.98 L 28.06 50.48 L 28.41 51.95 L 28.85 53.37 L 29.39 54.75 L 30.02 56.06 L 30.73 57.32 L 31.51 58.50 L 32.37 59.62 L 33.29 60.66 L 34.28 61.62 L 35.31 62.49 L 36.39 63.28 L 37.51 63.98 L 38.67 64.60 L 39.84 65.12 L 41.04 65.55 L 42.25 65.89 L 43.46 66.14 L 44.67 66.29 L 45.88 66.36 L 47.07 66.34 L 48.23 66.24 L 49.37 66.06 L 50.48 65.79 L 51.55 65.45 L 52.58 65.04 L 53.56 64.56 L 54.49 64.02 L 55.37 63.42 L 56.18 62.76 L 56.93 62.06 L 57.62 61.31 L 58.23 60.53 L 58.78 59.71 L 59.26 58.87 L 59.66 58.00 L 59.99 57.12 L 60.24 56.24 L 60.43 55.34 L 60.54 54.45 L 60.58 53.57 L 60.54 52.70 L 60.44 51.85 L 60.27 51.03 L 60.04 50.23 L 59.75 49.47 L 59.39 48.74 L 58.98 48.06 L 58.52 47.42 L 58.01 46.84 L 57.46 46.31 L 56.87 45.83 L 56.24 45.42 L 55.58 45.08 L 54.90 44.80 L 54.19 44.59 L 53.48 44.46 L 52.76 44.40 L 52.03 44.42 L 51.32 44.52 L 50.62 44.69 L 49.94 44.95 L 49.30 45.29 L 48.70 45.70 L 48.15 46.19 L 47.67 46.74 L 47.26 47.36 L 46.93 48.03 L 46.69 48.74 L 46.55 49.48 L 47.52 52.47 Z" fill="#141414"/>
  <circle cx="50.00" cy="50.00" r="3.50" fill="#141414"/>
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
