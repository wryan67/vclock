// Small toolbar icons, drawn from Font Awesome Free outlines.
//
// The glyphs are Font Awesome Free 6.7.2 by @fontawesome
// (https://fontawesome.com), used under CC BY 4.0
// (https://creativecommons.org/licenses/by/4.0/).  Only the path data is kept
// here, and it is recoloured at paint time rather than shipped in the colours
// it was authored in -- see aboutText() in embedded.h, which carries the
// credit the licence asks for.
#pragma once

#include <QColor>
#include <QIcon>

enum class Glyph {
    Cancel,  // fa-solid fa-square-xmark
    Save,    // fa-regular fa-floppy-disk
    Edit,    // fa-regular fa-pen-to-square
    New,     // fa-solid fa-square-plus
    Delete,    // fa-regular fa-trash-can
    Settings,  // fa-solid fa-gear
    Checked,   // fa-solid fa-check
    Unchecked, // fa-regular fa-square
};

// What an icon means, rather than what colour it is.  The colour is settled at
// paint time from the palette in force then, so an icon drawn on a dark theme
// comes out light without the caller having to know which theme is running,
// and one already sitting on a button follows a theme changed underneath it.
enum class GlyphRole {
    Neutral,  // an ordinary action: takes the theme's foreground
    Go,       // save, add
    Stop,     // cancel, delete
};

QIcon glyphIcon(Glyph glyph, GlyphRole role);

// A tick when on and an empty box when off, in one icon: Qt asks the engine for
// QIcon::On or QIcon::Off, so a checkable button swaps the two by itself.
QIcon checkIcon(GlyphRole role);

// Draws checkbox indicators with the Font Awesome tick and box instead of the
// platform's own, which the desktop theme renders as a heavy filled square.
// Install it once, on the application, and every checkbox follows.
void installGlyphStyle();

// Whether the palette in force is a dark one.
bool darkTheme();

// The colour a role paints in right now, for a caller that needs to match an
// icon with text or a frame of its own.
QColor glyphColor(GlyphRole role);
