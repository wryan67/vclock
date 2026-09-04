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
