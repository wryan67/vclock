# vclock

A transparent, borderless analog clock for the desktop, written in C++ with Qt.

The face is any SVG. By default its artwork is recoloured (line work → "wire"
colour, body → "face" colour), or it can be drawn in its own colours for a
picture that already has some; either way it is rasterised at the widget's
current pixel size, so it stays crisp at any scale. The window is undecorated
and painted onto a translucent surface, so everything outside the artwork is
genuinely transparent.

Left-drag moves the widget, double click opens Settings, right-click opens the
menu, `Ctrl`+Q quits. Settings live in a platform-native config directory,
including the last position:
`$XDG_CONFIG_HOME` (or `~/.config`) on Linux, `~/Library/Application Support`
on macOS, and `%APPDATA%` on Windows.

## Building

Requires CMake 3.16+, a C++17 compiler, and Qt 6 (Qt 5.15 also works) with the
Widgets and Svg modules.

On Linux, `build.sh` handles the whole thing — it locates Qt (including the
official installer's `~/Qt/<version>/gcc_64` layout), configures, and builds:

```sh
./build.sh                  # release build into ./build
./build.sh --install-deps   # install the required packages first
./build.sh --type Debug     # debug build into ./build-debug
./build.sh --help           # all options
```

If Qt is somewhere unusual, point the script at it with `--qt-dir` (or the
`QT_PREFIX` environment variable):

```sh
./build.sh --qt-dir ~/Qt/6.5.3/gcc_64
```

To drive CMake yourself, or on macOS and Windows:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/vclock
```

If Qt is not on the default search path, point CMake at it:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.5.3/gcc_64
```

### Dependencies by platform

* **Linux** — `qt6-base-dev qt6-svg-dev` (Debian/Ubuntu) or `qt6-qtbase-devel
  qt6-qtsvg-devel` (Fedora). A compositing window manager is needed for the
  transparent background; without one the window falls back to an opaque
  rectangle.
* **macOS** — Qt from Homebrew (`brew install qt`) or the Qt installer. The
  build produces a `vclock.app` bundle.
* **Windows** — Qt from the Qt installer with MSVC or MinGW. The executable is
  built with `WIN32`, so it does not open a console window.

## Using it

| Action | Result |
| --- | --- |
| Left drag | move the clock |
| Double click | settings |
| Right click | menu (Settings, Move, Always on top, Reset defaults, Help, About, Quit) |
| `Ctrl`/`Cmd`+S | settings |
| `Ctrl`/`Cmd`+M | move mode (see below) |
| `Ctrl`/`Cmd`+H | help |
| `Ctrl`/`Cmd`+A | about |
| `Ctrl`/`Cmd`+R | reset defaults |
| `Ctrl`/`Cmd`+Q | quit |
| `Esc`, `Ctrl`+C, `Alt`+F4 (`Cmd`+W on macOS) | quit as well |

Each menu entry shows its shortcut in a right-hand column.

A drag only begins once the pointer has actually travelled a few pixels, so a
double click that stays put opens Settings instead of being swallowed by the
start of a move. The clock therefore trails the pointer by that small threshold
for the rest of a drag, which is the ordinary feel of dragging anything.

The clock refuses to be minimised, maximised, or made full screen, whether the
request comes from the window manager, a "show desktop" key, or a tiling
shortcut. Because it keeps out of the taskbar and the window switcher, being
iconified would leave no way to get it back.

The clock is kept above other windows by default; *Always on top* in the menu
turns that off. Existing configs keep whatever they already had.

### Move mode

Dragging is awkward when the clock has ended up behind another window or off
under the pointer's usual travel. *Move* — the menu entry or `Ctrl`+M — picks
the clock up and centres it on the mouse, and from then on it follows the
pointer with no button held. Any mouse button sets it down; `Esc` puts it back
where it started. The clock is kept fully on the screen the pointer is on.

### Clock face

Any SVG can be used, in one of two colour modes, chosen with **Coloring** in
Settings — it sits directly above the two swatches it governs.

**Recolor** — the default. Within the artwork white is treated as the face
colour and black as the wire colour, and both can be set from Settings;
anti-aliased pixels blend between the two, so edges stay smooth. Ticking
**transparent** keeps only the line work and fades the body away. The mapping
reads brightness alone, so a drawing that already had colours comes out in
yours instead, its shading intact.

**Original** draws the file exactly as authored. The face and wire colours no
longer mean anything, so they grey out; the hands and the marks still draw on
top as usual.

Which one a given drawing wants is a matter of taste, so vclock does not guess:
the mode is whatever you last set it to and stays there when you change faces.
Recolouring a full-colour picture is a legitimate thing to want — it flattens
the artwork to your own two colours and can look rather good.

Three faces are built in and need no files: the plain default ring, the app
icon's gradient dial (stored in the config as `builtin:icon`), and a silver dial
under a dark rim (`builtin:silver`), which is the same gradient running the other
way.

### Smooth sweep hands

Off by default, the hands step once a second, the way a quartz movement does.
Ticking **Smooth sweep hands** in Settings sweeps them instead: the clock
repaints about every 17 ms (60 fps) and each hand takes the exact angle for the
current millisecond rather than the nearest whole second.

The visible difference is almost entirely the second hand, which travels about
40 px a second on a large clock. The minute hand is correct either way but moves
so slowly — roughly a hundredth of a pixel per frame, a whole pixel every 1.7
seconds — that on its own it cannot look like anything but stationary.

Sweeping means repainting the whole window sixty times a second, which is not
free: on a 1080 px clock it costs roughly a fifth of a core, against almost
nothing when stepping. The cost scales with the clock's area, so a small clock
sweeps cheaply. That is why it is opt-in.

Configs written while this option was briefly called `smooth_minute` are still
read, so the setting survives the rename.

### Reverse time

Ticking **Reverse time** in Settings runs all three hands anticlockwise. The
angles are mirrored about the twelve, so the clock still tells the right time —
you just have to read it in a mirror. It works with either movement: stepping or
sweeping.

Only the hands are mirrored. The dial is not, because sixty evenly spaced marks
look the same either way round.

Like smooth sweep, this is a movement setting rather than an appearance one, so
choosing a preset leaves it alone. *Reset defaults* clears it.

### Hand centre

Settings ▸ *Pick on clock…* hands focus to the clock itself and shows a
crosshair at the current centre. Either drag on the face — the hands and marks
follow the pointer and settle where the button is released — or use the
keyboard: the arrow keys nudge the centre a pixel at a time, `Shift`+arrow moves
ten, `Enter` accepts, and `Esc` cancels and restores the previous centre.
Nothing is written to the config until the pick is accepted. Focus returns to
the Settings window either way.

### Presets

The six thumbnails at the top of Settings are whole default clocks, not just
colour schemes. Clicking one restores every appearance setting, and also puts
the clock size and the hand centre back to their defaults — so a preset always
gives you exactly the clock in the thumbnail. Where the window sits on screen is
left alone, since that is a placement choice rather than a look. As with any
other change, the preset is only a preview until Save; Cancel puts the previous
size and centre back.

### Choosing colours

Every colour swatch in Settings opens a small picker: a hue/saturation wheel,
saturation and brightness sliders under it, a grid of preset swatches, and
`HTML` and `RGB` fields. Everything stays in step — drag the wheel and the
numbers follow; type `#ff8800` into the hex field, or put `255` in the red box,
and the wheel and sliders jump to match.

The wheel covers two of the three axes at once: hue is the angle round it, and
saturation is the distance from the centre to the rim. The `S` slider is that
second axis on its own, so you can wash a colour out or deepen it without
nudging the hue, which is fiddly to do by dragging the handle exactly along a
radius. Move it and you can watch the wheel handle slide straight in or out.
The `B` slider is brightness. Both bars are painted using the other channels'
current values, so each one previews the range it will actually move through.

The presets are nine columns — red, orange, yellow, green, teal, blue, purple,
pink, and a greyscale column — by nine rows. Pink and teal are there because
neither can be reached from a neighbour: pink is a tint of red rather than a
shade of it, and the gap between green and blue is wide enough that teal is a
long way from either.

Every column runs light to dark. The top two rows are tints, the same hue washed
out towards white, which is where the pinks and lavenders live; then the pure
colour; then shades down towards black. One ramp per column means a row reads as
a single weight the whole way across, which a hand-picked list never quite
manages. The greys follow the same run, white at the top and black at the
bottom, so the last column reads with the others rather than against them.

The wheel is tinted by the current brightness so you see the colour in context,
but it never dims past the point where the hues stop being distinguishable. That
floor matters here because the default hand colour is nearly black, so without it
the wheel would open unusable on most of these buttons.

Hue and saturation are remembered even when the colour on its own could not
carry them — black has neither and a grey has no hue, so taking either slider
down to nothing and back used to lose your place on the wheel. The picker keeps
the three channels itself rather than reading them back off the chosen colour,
so the handle stays where you left it.

The swatch beside the fields is split: the right half is the colour you are
choosing, the left half is the colour you started with. Clicking the left half
puts the original back. As with the rest of Settings the clock previews the
colour live, and Cancel restores what was there before.

The layout is modelled on [iro.js](https://github.com/jaames/iro.js), but none of
its code is used — this is a plain Qt widget written from scratch.

### The Settings window

If the screen is not tall or wide enough for the whole dialog, the controls
scroll and the Save and Cancel buttons stay pinned below them, so they are
always reachable. Scrollbars appear only when they are actually needed.

## Configuration

Settings are JSON in `vclock.cfg` inside the platform config directory above.
The file format is unchanged from the earlier Python implementation, and configs
written under the older `~/.config/vclock` or `~/.config/fclock` paths (and the
older `ship_color` / `ship_transparent` key names) are still read once and
migrated on the next save.

### Monitors

The clock remembers a position and a size for each monitor it has been used on,
under the `displays` key, along with the monitor it was last on in
`last_display`:

```json
"displays": {
  "Dell Inc. U2720Q 4M8YJ63": { "x": 568, "y": 199, "size": 400 }
},
"last_display": "Dell Inc. U2720Q 4M8YJ63"
```

A few details worth knowing:

* Monitors are identified by their EDID (maker, model, serial) rather than by
  the connector they are plugged into, so moving a cable between ports keeps
  the monitor's settings. Two panels reporting identical EDID are told apart by
  appending the connector name.
* Positions are stored relative to the monitor's own working area, not to the
  desktop as a whole, so rearranging monitors leaves the clock on the same part
  of the same physical screen.
* The clock opens on the monitor it was last used on. If that monitor is not
  attached it opens on the monitor holding the pointer, and if it has never
  been used there it is centred rather than left wherever the window manager
  would put it. Records for absent monitors are kept, so plugging one back in
  restores its placement.
* Dragging the clock to another monitor records its position there, but its
  remembered size for that monitor is only applied when the clock *opens* on
  it — resizing the window mid-drag would be jarring.
* Unplugging a monitor that the clock was on moves it back onto an attached
  screen. Without this, a frameless window that keeps out of the taskbar and
  the window switcher would be left stranded on coordinates that no longer
  exist, and could not be recovered.

## Source layout

| File | Contents |
| --- | --- |
| `src/embedded.h` | the built-in SVG faces and app icon |
| `src/config.*` | the settings record, its defaults, and JSON load/save |
| `src/face.*` | SVG rasterising, recolouring, and content bounds |
| `src/render.*` | hands, hour/minute indices, and the appearance presets |
| `src/colorbutton.*` | a colour swatch button with a live preview |
| `src/colorpicker.*` | the colour picker: wheel, S/B sliders, hex and RGB fields |
| `src/settingsdialog.*` | the Settings window |
| `src/clockwindow.*` | the translucent clock window itself |

## Notes on the port

This is a port of `vclock.py` (GTK 3 + PyGObject + librsvg + cairo + NumPy) to
C++ and Qt, keeping the behaviour, settings, config format and look. The only
deliberate omission is the Python version's `quiet_stderr()` filter, which
existed solely to suppress Fontconfig chatter from the Linux GTK stack.
