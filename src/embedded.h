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
  <circle cx="50" cy="50" r="38.0" fill="#1a1a1a" />
  <polygon points="30.3,23.4 25.7,20.8 26.9,19.9 28.9,18.4 31.0,17.1 33.2,15.9 35.5,14.9 36.8,14.4 36.8,19.6" fill="#f7f7f7" />
  <polygon points="43.4,23.4 36.8,19.6 36.8,14.4 37.8,14.0 40.2,13.3 42.6,12.7 45.0,12.3 47.5,12.1 50.0,12.0 50.0,19.6" fill="#e8e8e8" />
  <polygon points="56.6,23.4 50.0,19.6 50.0,12.0 52.5,12.1 55.0,12.3 57.4,12.7 59.8,13.3 62.2,14.0 63.2,14.4 63.2,19.6" fill="#dadada" />
  <polygon points="66.8,15.9 69.0,17.1 71.1,18.4 73.1,19.9 74.3,20.8 69.7,23.4 63.2,19.6 63.2,14.4 64.5,14.9" fill="#cbcbcb" />
  <polygon points="23.7,34.8 17.1,31.0 18.4,28.9 19.9,26.9 21.4,24.9 23.1,23.1 24.9,21.4 25.7,20.8 30.3,23.4 30.3,31.0" fill="#f2f2f2" />
  <polygon points="36.8,34.8 30.3,31.0 30.3,23.4 36.8,19.6 43.4,23.4 43.4,31.0" fill="#e3e3e3" />
  <polygon points="50.0,34.8 43.4,31.0 43.4,23.4 50.0,19.6 56.6,23.4 56.6,31.0" fill="#d5d5d5" />
  <polygon points="63.2,34.8 56.6,31.0 56.6,23.4 63.2,19.6 69.7,23.4 69.7,31.0" fill="#c6c6c6" />
  <polygon points="76.3,34.8 69.7,31.0 69.7,23.4 74.3,20.8 75.1,21.4 76.9,23.1 78.6,24.9 80.1,26.9 81.6,28.9 82.9,31.0" fill="#b8b8b8" />
  <polygon points="17.1,46.2 12.6,43.6 12.7,42.6 13.3,40.2 14.0,37.8 14.9,35.5 15.9,33.2 17.1,31.0 23.7,34.8 23.7,42.4" fill="#ececec" />
  <polygon points="30.3,46.2 23.7,42.4 23.7,34.8 30.3,31.0 36.8,34.8 36.8,42.4" fill="#dedede" />
  <polygon points="43.4,46.2 36.8,42.4 36.8,34.8 43.4,31.0 50.0,34.8 50.0,42.4" fill="#cfcfcf" />
  <polygon points="56.6,46.2 50.0,42.4 50.0,34.8 56.6,31.0 63.2,34.8 63.2,42.4" fill="#c1c1c1" />
  <polygon points="69.7,46.2 63.2,42.4 63.2,34.8 69.7,31.0 76.3,34.8 76.3,42.4" fill="#b2b2b2" />
  <polygon points="82.9,31.0 84.1,33.2 85.1,35.5 86.0,37.8 86.7,40.2 87.3,42.6 87.4,43.6 82.9,46.2 76.3,42.4 76.3,34.8" fill="#a4a4a4" />
  <polygon points="12.6,56.4 12.3,55.0 12.1,52.5 12.0,50.0 12.1,47.5 12.3,45.0 12.6,43.6 17.1,46.2 17.1,53.8" fill="#e7e7e7" />
  <polygon points="23.7,57.6 17.1,53.8 17.1,46.2 23.7,42.4 30.3,46.2 30.3,53.8" fill="#d8d8d8" />
  <polygon points="36.8,57.6 30.3,53.8 30.3,46.2 36.8,42.4 43.4,46.2 43.4,53.8" fill="#cacaca" />
  <polygon points="50.0,57.6 43.4,53.8 43.4,46.2 50.0,42.4 56.6,46.2 56.6,53.8" fill="#bcbcbc" />
  <polygon points="63.2,57.6 56.6,53.8 56.6,46.2 63.2,42.4 69.7,46.2 69.7,53.8" fill="#adadad" />
  <polygon points="76.3,57.6 69.7,53.8 69.7,46.2 76.3,42.4 82.9,46.2 82.9,53.8" fill="#9f9f9f" />
  <polygon points="87.7,45.0 87.9,47.5 88.0,50.0 87.9,52.5 87.7,55.0 87.4,56.4 82.9,53.8 82.9,46.2 87.4,43.6" fill="#909090" />
  <polygon points="17.1,69.0 15.9,66.8 14.9,64.5 14.0,62.2 13.3,59.8 12.7,57.4 12.6,56.4 17.1,53.8 23.7,57.6 23.7,65.2" fill="#d3d3d3" />
  <polygon points="30.3,69.0 23.7,65.2 23.7,57.6 30.3,53.8 36.8,57.6 36.8,65.2" fill="#c5c5c5" />
  <polygon points="43.4,69.0 36.8,65.2 36.8,57.6 43.4,53.8 50.0,57.6 50.0,65.2" fill="#b6b6b6" />
  <polygon points="56.6,69.0 50.0,65.2 50.0,57.6 56.6,53.8 63.2,57.6 63.2,65.2" fill="#a8a8a8" />
  <polygon points="69.7,69.0 63.2,65.2 63.2,57.6 69.7,53.8 76.3,57.6 76.3,65.2" fill="#999999" />
  <polygon points="87.3,57.4 86.7,59.8 86.0,62.2 85.1,64.5 84.1,66.8 82.9,69.0 76.3,65.2 76.3,57.6 82.9,53.8 87.4,56.4" fill="#8b8b8b" />
  <polygon points="25.7,79.2 24.9,78.6 23.1,76.9 21.4,75.1 19.9,73.1 18.4,71.1 17.1,69.0 23.7,65.2 30.3,69.0 30.3,76.6" fill="#bfbfbf" />
  <polygon points="36.8,80.4 30.3,76.6 30.3,69.0 36.8,65.2 43.4,69.0 43.4,76.6" fill="#b1b1b1" />
  <polygon points="50.0,80.4 43.4,76.6 43.4,69.0 50.0,65.2 56.6,69.0 56.6,76.6" fill="#a2a2a2" />
  <polygon points="63.2,80.4 56.6,76.6 56.6,69.0 63.2,65.2 69.7,69.0 69.7,76.6" fill="#949494" />
  <polygon points="81.6,71.1 80.1,73.1 78.6,75.1 76.9,76.9 75.1,78.6 74.3,79.2 69.7,76.6 69.7,69.0 76.3,65.2 82.9,69.0" fill="#858585" />
  <polygon points="35.5,85.1 33.2,84.1 31.0,82.9 28.9,81.6 26.9,80.1 25.7,79.2 30.3,76.6 36.8,80.4 36.8,85.6" fill="#acacac" />
  <polygon points="50.0,88.0 47.5,87.9 45.0,87.7 42.6,87.3 40.2,86.7 37.8,86.0 36.8,85.6 36.8,80.4 43.4,76.6 50.0,80.4" fill="#9d9d9d" />
  <polygon points="62.2,86.0 59.8,86.7 57.4,87.3 55.0,87.7 52.5,87.9 50.0,88.0 50.0,80.4 56.6,76.6 63.2,80.4 63.2,85.6" fill="#8f8f8f" />
  <polygon points="73.1,80.1 71.1,81.6 69.0,82.9 66.8,84.1 64.5,85.1 63.2,85.6 63.2,80.4 69.7,76.6 74.3,79.2" fill="#808080" />
  <polygon points="30.8,22.0 26.9,19.8 28.9,18.4 31.0,17.1 33.2,15.9 35.5,14.9 35.5,19.3" fill="#636363" />
  <polygon points="44.0,22.0 39.2,19.3 39.2,13.8 40.1,13.3 40.2,13.3 42.6,12.7 45.0,12.3 46.0,12.2 48.7,13.8 48.7,19.3" fill="#5a5a5a" />
  <polygon points="57.1,22.0 52.4,19.3 52.4,13.8 55.0,12.3 57.4,12.7 59.8,13.3 61.9,13.9 61.9,19.3" fill="#505050" />
  <polygon points="66.8,15.9 69.0,17.1 71.1,18.4 73.1,19.9 73.5,20.2 70.3,22.0 65.6,19.3 65.6,15.4" fill="#474747" />
  <polygon points="24.2,33.4 19.5,30.7 19.5,27.4 19.9,26.9 21.4,24.9 23.1,23.1 23.2,23.1 24.2,22.5 29.0,25.2 29.0,30.7" fill="#5f5f5f" />
  <polygon points="37.4,33.4 32.6,30.7 32.6,25.2 37.4,22.5 42.1,25.2 42.1,30.7" fill="#565656" />
  <polygon points="50.5,33.4 45.8,30.7 45.8,25.2 50.5,22.5 55.3,25.2 55.3,30.7" fill="#4d4d4d" />
  <polygon points="63.7,33.4 59.0,30.7 59.0,25.2 63.7,22.5 68.5,25.2 68.5,30.7" fill="#444444" />
  <polygon points="76.9,33.4 72.1,30.7 72.1,25.2 76.4,22.7 76.9,23.1 78.6,24.9 80.1,26.9 81.6,28.9 81.6,30.7" fill="#3b3b3b" />
  <polygon points="17.6,44.8 12.9,42.1 12.9,41.8 13.3,40.2 14.0,37.8 14.9,35.5 17.6,33.9 22.4,36.6 22.4,42.1" fill="#5c5c5c" />
  <polygon points="30.8,44.8 26.1,42.1 26.1,36.6 30.8,33.9 35.5,36.6 35.5,42.1" fill="#535353" />
  <polygon points="44.0,44.8 39.2,42.1 39.2,36.6 44.0,33.9 48.7,36.6 48.7,42.1" fill="#4a4a4a" />
  <polygon points="57.1,44.8 52.4,42.1 52.4,36.6 57.1,33.9 61.9,36.6 61.9,42.1" fill="#404040" />
  <polygon points="70.3,44.8 65.6,42.1 65.6,36.6 70.3,33.9 75.0,36.6 75.0,42.1" fill="#373737" />
  <polygon points="85.1,35.5 86.0,37.8 86.7,40.2 87.3,42.6 83.5,44.8 78.7,42.1 78.7,36.6 83.5,33.9 84.7,34.6" fill="#2e2e2e" />
  <polygon points="12.4,55.4 12.3,55.0 12.1,52.5 12.0,50.0 12.1,47.5 12.2,46.0 15.8,48.0 15.8,53.5" fill="#595959" />
  <polygon points="24.2,56.2 19.5,53.5 19.5,48.0 24.2,45.3 29.0,48.0 29.0,53.5" fill="#4f4f4f" />
  <polygon points="37.4,56.2 32.6,53.5 32.6,48.0 37.4,45.3 42.1,48.0 42.1,53.5" fill="#464646" />
  <polygon points="50.5,56.2 45.8,53.5 45.8,48.0 50.5,45.3 55.3,48.0 55.3,53.5" fill="#3d3d3d" />
  <polygon points="63.7,56.2 59.0,53.5 59.0,48.0 63.7,45.3 68.5,48.0 68.5,53.5" fill="#343434" />
  <polygon points="76.9,56.2 72.1,53.5 72.1,48.0 76.9,45.3 81.6,48.0 81.6,53.5" fill="#2b2b2b" />
  <polygon points="87.9,47.5 88.0,50.0 87.9,52.5 87.7,54.9 85.3,53.5 85.3,48.0 87.8,46.6" fill="#212121" />
  <polygon points="17.6,67.6 15.8,66.6 14.9,64.5 14.0,62.2 13.3,59.8 13.2,59.3 17.6,56.7 22.4,59.4 22.4,64.9" fill="#4c4c4c" />
  <polygon points="30.8,67.6 26.1,64.9 26.1,59.4 30.8,56.7 35.5,59.4 35.5,64.9" fill="#434343" />
  <polygon points="44.0,67.6 39.2,64.9 39.2,59.4 44.0,56.7 48.7,59.4 48.7,64.9" fill="#3a3a3a" />
  <polygon points="57.1,67.6 52.4,64.9 52.4,59.4 57.1,56.7 61.9,59.4 61.9,64.9" fill="#303030" />
  <polygon points="70.3,67.6 65.6,64.9 65.6,59.4 70.3,56.7 75.0,59.4 75.0,64.9" fill="#272727" />
  <polygon points="86.7,59.8 86.0,62.2 85.1,64.5 84.1,66.8 83.7,67.5 83.5,67.6 78.7,64.9 78.7,59.4 83.5,56.7 87.0,58.7" fill="#1e1e1e" />
  <polygon points="25.0,78.6 24.9,78.6 23.1,76.9 21.4,75.1 19.9,73.1 19.5,72.6 19.5,70.8 24.2,68.1 29.0,70.8 29.0,76.3" fill="#3f3f3f" />
  <polygon points="37.4,79.0 32.6,76.3 32.6,70.8 37.4,68.1 42.1,70.8 42.1,76.3" fill="#363636" />
  <polygon points="50.5,79.0 45.8,76.3 45.8,70.8 50.5,68.1 55.3,70.8 55.3,76.3" fill="#2d2d2d" />
  <polygon points="63.7,79.0 59.0,76.3 59.0,70.8 63.7,68.1 68.5,70.8 68.5,76.3" fill="#242424" />
  <polygon points="81.6,71.1 80.1,73.1 78.6,75.1 76.9,76.9 75.5,78.2 72.1,76.3 72.1,70.8 76.9,68.1 81.6,70.8" fill="#1b1b1b" />
  <polygon points="35.5,85.1 33.2,84.1 31.0,82.9 28.9,81.6 28.1,81.0 30.8,79.5 35.5,82.2" fill="#333333" />
  <polygon points="48.3,87.9 47.5,87.9 45.0,87.7 42.6,87.3 40.2,86.7 39.2,86.4 39.2,82.2 44.0,79.5 48.7,82.2 48.7,87.7" fill="#2a2a2a" />
  <polygon points="59.8,86.7 57.4,87.3 55.0,87.7 52.8,87.9 52.4,87.7 52.4,82.2 57.1,79.5 61.9,82.2 61.9,86.1" fill="#202020" />
  <polygon points="71.1,81.6 69.0,82.9 66.8,84.1 65.6,84.6 65.6,82.2 70.3,79.5 72.4,80.7" fill="#171717" />
  <circle cx="50" cy="50" r="40" fill="none" stroke="#171717" stroke-width="3" />
</svg>
)SVG");
}

// The application icon, embedded so the program does not depend on a system
// icon theme (which is often absent on Windows and macOS).
inline QByteArray appIconSvg()
{
    return QByteArrayLiteral(
        R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">
  <defs>
    <linearGradient id="clockGradient" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#8a959c" />
      <stop offset="100%" stop-color="#00008b" />
    </linearGradient>
  </defs>

  <circle cx="50" cy="50" r="40" stroke="#b0c4de" stroke-width="3" fill="url(#clockGradient)" />

  <line x1="50" y1="15" x2="50" y2="20" stroke="#f0f0f0" stroke-width="2" transform="rotate(0 50 50)" />
  <line x1="50" y1="15" x2="50" y2="20" stroke="#f0f0f0" stroke-width="2" transform="rotate(30 50 50)" />
  <line x1="50" y1="15" x2="50" y2="20" stroke="#f0f0f0" stroke-width="2" transform="rotate(60 50 50)" />
  <line x1="50" y1="15" x2="50" y2="20" stroke="#f0f0f0" stroke-width="2" transform="rotate(90 50 50)" />
  <line x1="50" y1="15" x2="50" y2="20" stroke="#f0f0f0" stroke-width="2" transform="rotate(120 50 50)" />
  <line x1="50" y1="15" x2="50" y2="20" stroke="#f0f0f0" stroke-width="2" transform="rotate(150 50 50)" />
  <line x1="50" y1="15" x2="50" y2="20" stroke="#f0f0f0" stroke-width="2" transform="rotate(180 50 50)" />
  <line x1="50" y1="15" x2="50" y2="20" stroke="#f0f0f0" stroke-width="2" transform="rotate(210 50 50)" />
  <line x1="50" y1="15" x2="50" y2="20" stroke="#f0f0f0" stroke-width="2" transform="rotate(240 50 50)" />
  <line x1="50" y1="15" x2="50" y2="20" stroke="#f0f0f0" stroke-width="2" transform="rotate(270 50 50)" />
  <line x1="50" y1="15" x2="50" y2="20" stroke="#f0f0f0" stroke-width="2" transform="rotate(300 50 50)" />
  <line x1="50" y1="15" x2="50" y2="20" stroke="#f0f0f0" stroke-width="2" transform="rotate(330 50 50)" />

  <line x1="50" y1="50" x2="50" y2="30" stroke="#f0f0f0" stroke-width="3" stroke-linecap="round" transform="rotate(305 50 50)" />
  <line x1="50" y1="50" x2="50" y2="18" stroke="#f0f0f0" stroke-width="2" stroke-linecap="round" transform="rotate(60 50 50)" />

  <circle cx="50" cy="50" r="3" fill="#f0f0f0" />
</svg>
)SVG");
}

inline const char *aboutText()
{
    return "A transparent, borderless analog clock for the desktop.\n\n"
           "The clock face can be any SVG. In Recolor mode white is treated as "
           "the face color and black as the wire color, and both can be set from "
           "Settings; in Original mode the artwork is drawn as it was authored, "
           "which is what a full-color picture wants.";
}
