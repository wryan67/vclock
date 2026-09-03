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
