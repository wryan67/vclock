# vclock

A transparent, borderless analog clock for the desktop, written in C++ with Qt.

The face is any SVG; its artwork is recoloured (line work → "wire" colour, body
→ "face" colour) and rasterised at the widget's current pixel size, so it stays
crisp at any scale. The window is undecorated and painted onto a translucent
surface, so everything outside the artwork is genuinely transparent.

Left-drag moves the widget, right-click opens the menu, Escape quits. Settings
live in a platform-native config directory, including the last position:
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
| Right click | menu (Settings, Always on top, Reset defaults, Help, About, Quit) |
| `Ctrl`/`Cmd`+S | settings |
| `Ctrl`/`Cmd`+H | help |
| `Ctrl`/`Cmd`+A | about |
| `Ctrl`/`Cmd`+R | reset defaults |
| `Esc`, `Ctrl`+C, `Alt`+F4 (`Cmd`+Q / `Cmd`+W on macOS) | quit |

### Clock face

Any SVG can be used. Within the artwork white is treated as the face colour and
black as the wire colour, and both can be recoloured from Settings; anti-aliased
pixels blend between the two, so edges stay smooth. Ticking **transparent**
keeps only the line work and fades the body away.

Two faces are built in and need no files: the plain default ring, and the app
icon's gradient dial (stored in the config as `builtin:icon`).

### Hand centre

Settings ▸ *Pick on clock…*, then drag on the face. The hands and marks follow
the pointer and settle where you release the button; `Esc` cancels.

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
| `src/settingsdialog.*` | the Settings window |
| `src/clockwindow.*` | the translucent clock window itself |

## Notes on the port

This is a port of `vclock.py` (GTK 3 + PyGObject + librsvg + cairo + NumPy) to
C++ and Qt, keeping the behaviour, settings, config format and look. The only
deliberate omission is the Python version's `quiet_stderr()` filter, which
existed solely to suppress Fontconfig chatter from the Linux GTK stack.
