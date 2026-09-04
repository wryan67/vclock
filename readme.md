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
./build.sh --check-deps     # report what is missing, and stop
./build.sh --install-deps   # install what is missing, then build
./build.sh --type Debug     # debug build into ./build-debug
./build.sh --help           # all options
```

If anything is missing the script names it and, where it recognises the
distribution, prints the exact install command for it — Debian, Ubuntu, RHEL,
Fedora, Arch, openSUSE and Alpine, plus their derivatives via `ID_LIKE`. Only
the packages actually missing are listed.

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

* **Linux** — `qt6-base-dev qt6-svg-dev` (Debian/Ubuntu), `qt6-qtbase-devel
  qt6-qtsvg-devel` (Fedora, RHEL via EPEL), `qt6-base qt6-svg` (Arch),
  `qt6-base-devel qt6-svg-devel` (openSUSE) or `qt6-qtbase-dev qt6-qtsvg-dev`
  (Alpine); `./build.sh --check-deps` works this out for you. A compositing
  window manager is needed for the transparent background; without one the
  window falls back to an opaque rectangle.
* **macOS** — Qt from Homebrew (`brew install qt`) or the Qt installer. The
  build produces a `vclock.app` bundle.
* **Windows** — Qt from the Qt installer with MSVC or MinGW. The executable is
  built with `WIN32`, so it does not open a console window.

## Packaging

`./build.sh --distro` builds installable packages rather than a binary in the
build tree:

```sh
./build.sh --distro deb                  # a .deb for this machine
./build.sh --distro deb --arch arm64     # a .deb for aarch64
./build.sh --distro rpm,windows          # more than one target
./build.sh --distro all                  # everything this host can build
```

Packages land in `distro/out`. `--arch` takes `amd64`, `arm64` or `all`, and
accepts `x86_64`, `x64` and `aarch64` as names for the same two things; it
defaults to this machine's architecture, except with `--distro all` which
defaults to every architecture each target has. A target only builds the
architectures it has — `windows` is x64 only — so asking for `all` of both
builds the combinations that exist rather than failing on the ones that do not.

A run that builds several packages does not stop at the first one that fails,
because one broken target is not a reason to throw away the four that would have
worked. It finishes the rest and prints what happened to each, so a failure
shows up as one line in a list rather than as a truncated run:

```
==> results
  built    deb amd64    vclock_1.0_amd64.deb  172K
  built    deb arm64    vclock_1.0_arm64.deb  172K
  failed   rpm amd64    see the output above
  skipped  macos arm64  needs Apple hardware; run distro/macos/package.sh on a Mac
```

Only packages produced by that run are listed, so a stale file left in
`distro/out` by an earlier attempt is never mistaken for something just built —
including the case where a target exits successfully but writes nothing, which
counts as a failure. `--distro` exits non-zero if any build failed or if none
ran, which is what a release script should be checking rather than the output.

Each package is built inside a container for the distribution it targets, so
what comes out depends on that distribution rather than on whatever is installed
here — which is what makes it possible to build a Fedora rpm on Ubuntu, and an
aarch64 package on an x86\_64 machine. Docker is the only requirement.

| Target | Architectures | Built by |
| --- | --- | --- |
| `deb` | amd64, arm64 | Ubuntu 24.04 container, CPack |
| `rpm` | amd64, arm64 | Fedora container, CPack |
| `windows` | x64 | MinGW-w64 cross-compile in a Fedora container, NSIS installer |
| `macos` | x86\_64, arm64 | a Mac — see below |

The deb is built on the oldest release vclock supports rather than the newest,
because glibc and Qt symbol versioning are backward compatible but not forward:
a package built on 24.04 installs on 24.04 and everything after it, while one
built on the current release would refuse to install on anything older. Its
dependencies are worked out from what the binary actually links against rather
than from a hand-written list, which would go stale the first time a Qt module
was added.

Building for a foreign architecture runs the container under qemu. That is
several times slower than a native build but produces genuine native binaries.
It needs the qemu handlers registered with the kernel once:

```sh
docker run --privileged --rm tonistiigi/binfmt --install arm64
```

That is a host-wide change which survives reboot; `--uninstall` undoes it.
Without it the container starts and every process in it dies with `exec format
error`, so `build.sh` checks first and says this instead.

The Windows installer is cross-compiled rather than built on Windows. Fedora is
the only mainstream distribution that packages a MinGW-w64 build of Qt6, which
is why that container is a Fedora one. The Qt DLLs are found by walking the
import tables of the binary and then of each DLL it names, so nothing has to be
listed by hand; the plugins Qt loads by directory — `qwindows.dll` above all —
are copied separately, because nothing links against them and the walk cannot
see them. The build fails rather than ships if `qwindows.dll` is missing, since
without it the program starts and immediately aborts.

### macOS

macOS is the one target that cannot be built here. This is not a gap in the
tooling: Apple's SDK licence restricts building to Apple hardware, and since
Catalina an app that is neither signed nor notarised is refused by Gatekeeper
rather than merely warned about. `./build.sh --distro macos` says so rather than
pretending.

The recipe is `distro/macos/package.sh`, which runs on a Mac and produces a
`vclock.app` and a disk image. It uses `macdeployqt` to copy the Qt frameworks
into the bundle and rewrite the binary's load paths to point inside it, so the
result runs on a machine that has no Qt installed. It builds the `.icns` from
`vclock.svg`, since the program otherwise ships no icon file. Set
`CODESIGN_IDENTITY` to sign; without it the build still works but Gatekeeper
will object anywhere but the machine that built it.

`.github/workflows/release.yml` runs all of this on a tag — the container
targets on Linux runners and macOS on GitHub's macOS runners, one job per
architecture, since Qt from Homebrew is single-architecture and a universal
binary is not an option.

`--distro all` builds every combination this host can reach and lists the macOS
ones as skipped, with the reason, rather than quietly producing fewer packages
than were asked for.

## Using it

| Action | Result |
| --- | --- |
| Left drag | move the clock |
| Double click | settings |
| Right click | menu (Always on top, Manage clocks, Settings, Move, Reset defaults, Help, About, Hide, Quit) |
| `Ctrl`/`Cmd`+K | manage clocks |
| `Ctrl`/`Cmd`+S | settings |
| `Ctrl`/`Cmd`+M | move mode (see below) |
| `F1` | help |
| `Ctrl`/`Cmd`+A | about |
| `Ctrl`/`Cmd`+R | reset defaults |
| `Ctrl`/`Cmd`+H | hide this clock |
| `Esc`, `Alt`+F4 (`Cmd`+W on macOS) | hide it as well |
| `Ctrl`/`Cmd`+Q | quit, closing every clock |

Hide takes one clock off screen and leaves the others running; Manage clocks
brings it back. Hiding the last one ends the program, since there is nothing
left to run for.

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
the Settings **Face** box, directly above the two swatches it governs.

**Recolor** — the default. Within the artwork white is treated as the face
colour and black as the wire colour, and both can be set from Settings;
anti-aliased pixels blend between the two, so edges stay smooth. The mapping
reads brightness alone, so a drawing that already had colours comes out in
yours instead, its shading intact. The two ends fade separately -- see
**Opacity** below -- so the body can be dropped away to leave only the line
work over the desktop.

**Original** draws the file exactly as authored. The face and wire colours no
longer mean anything, so they grey out; the hands and the marks still draw on
top as usual.

Which one a given drawing wants is a matter of taste, so vclock does not guess:
the mode is whatever you last set it to and stays there when you change faces.
Recolouring a full-colour picture is a legitimate thing to want — it flattens
the artwork to your own two colours and can look rather good.

Four faces are built in and need no files: the plain default ring, the
gradient dial the app icon is drawn from (stored in the config as
`builtin:icon`), and a silver dial
under a dark rim (`builtin:silver`), which is the same gradient running the other
way, and a honeycomb (`builtin:honeycomb`), whose lit wax walls take the face
colour and whose cells take the wire colour. Whichever is in use, the *Clock face svg* field names it when no file of
your own is loaded; clicking a preset is how you return to a built-in face.

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

### The second hand

The **enabled** box beside the second hand's colour in Settings ▸ *Hands* turns
that hand off, for a quieter clock or one where a sweeping hand is a
distraction. Only the second hand can be dropped this way; an analog clock
without an hour or minute hand has stopped being a clock.

The colour is remembered while the hand is off — the swatch merely greys out —
so ticking it back on returns the hand you had rather than a default one. Every
preset draws a second hand, so clicking a preset turns it back on.

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

The application icon is a clock drawn by the same code that draws the
thumbnails and the clock itself, rather than a picture of its own — so it is
sharp at any size and is always a real vclock. Which clock it is comes from the
config directory: a clock named **icon** (`icon.cfg`) is used when there is one,
so you can dress the icon by editing that clock like any other, and the change
shows the next time About or Help is opened. With no such clock the **Gradient**
preset stands in.

`vclock.svg` in the project root is that same dial written out as a standalone
drawing, for anywhere an icon file is wanted rather than a running program: a
desktop entry, a package, a readme. It is a snapshot rather than a live render,
so it does not follow `icon.cfg` — if you change the icon clock and want the
file to keep up, redraw it. One deliberate difference: the program floors every
mark at one screen pixel so the dial stays legible when it is small, which a
fixed drawing cannot do, so the file takes that floor at the size the About
dialog uses.

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

Every slider in Sizes and Opacity has its value in a box beside it that can be
typed into as well as read, which is the only way to set an exact number on a
slider whose range is wider than the pixels it is drawn in. A value outside the
range is refused as it is typed, and a half-finished one is rounded to the
nearest allowed value when you leave the box.

Opacity fades the four parts of the clock separately, each from solid down to
gone:

| Slider | What it fades |
| --- | --- |
| Face | The body of the artwork -- at 0 the desktop shows through it |
| Wire | The artwork's line work, its outlines and shading |
| Clock hands | The three hands and the pin they turn on |
| Clock marks | The hour and minute indices around the dial |

**sync face/wire** and **sync hands/marks** keep a pair on one value, for when
you want to fade the whole drawing, or the whole dial, in one go. Ticking a box
pulls the second slider onto the first, so the value you were looking at wins.

Any part may fade away entirely, which is the point: a face at 0 leaves a wire
outline over the wallpaper, and hands at 0 leave a dial with nothing on it. The
clock itself never becomes unreachable, because the window goes on taking
clicks wherever the clock is, so right clicking there still opens the menu.

## Configuration

Settings are JSON in `default.cfg` inside the platform config directory above.
The file format is unchanged from the earlier Python implementation, and configs
written under the older `vclock.cfg` name, the older `~/.config/vclock` or
`~/.config/fclock` paths, or the older `ship_color` / `ship_transparent` key
names, are still read once and migrated on the next save.

The same goes for the older `opacity`, which faded the whole window before each
part of the drawing had a fade of its own: it is spread across all four, so a
clock set half faded still looks the way it did. An older `face_transparent` is
read as a face faded to 0, which is what that tick did.

### Command line

    vclock [-c NAME]... [-h]

`-c`, `--config NAME` reads and writes `NAME` instead of `default.cfg`. A bare
name means a file in the config directory, so `-c world` is `world.cfg`;
anything with a `/` in it is a path of your own. A config that does not exist
yet starts from the defaults and is written on the first change.

Repeat the option to run several clocks at once, each with its own config, its
own size, face and position:

    vclock -c desk -c world

They share one process and one tray of settings dialogs; the title bar of each
Settings window names the config it belongs to. Hiding one clock leaves the
others running, and the program ends with the last of them. Naming the same
config twice opens it once, since two clocks writing one file would each save
over the other.

A clock named with `-c` is showing for that run only: it does not change which
clocks come back the next time vclock is started on its own.

## Managing clocks

Right click &#9656; Manage clocks, or `Ctrl`+K, lists every clock you have. Each
row has a Show box, the clock's name, and buttons to open its settings, rename
it, or delete it and the config it keeps its settings in. Double clicking a
name renames it too. `Enter` saves the new name and `Escape` abandons it.

Show puts a clock on screen and takes it off again. Whatever is showing when
vclock stops is what comes back when it starts again, so there is nothing
separate to set for that. If every clock is hidden the default one comes back
rather than the program starting with no windows at all.

The list lives in `vclocks.cfg` in the config directory, alongside the per-clock
configs. Names there are labels only; the config file a clock uses is fixed when
it is made, so renaming never moves any settings.

### Start at login

The box at the bottom of Manage clocks starts vclock with the desktop session.
The box reads the system rather than remembering an answer of its own, so
removing the entry by hand and reopening the dialog shows it unticked, and the
two can never disagree.

Every desktop has this and no two agree on how, so there are three
implementations behind the one box. All are per user and none needs
administrator rights, which is what makes a checkbox an honest interface for
them:

| Platform | What ticking the box writes |
| --- | --- |
| Linux, BSD | `vclock.desktop` in `~/.config/autostart`, the freedesktop convention every "startup applications" list reads |
| macOS | `org.vclock.vclock.plist` in `~/Library/LaunchAgents`, a launchd agent with `RunAtLoad` |
| Windows | a `vclock` value under `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` |

Anywhere else the box is left out of the dialog entirely rather than shown
disabled: a box that cannot be ticked only invites the question of how to make
it tickable, and there would be no answer.

There is one setting for the program, not one per clock, because what comes back
at login is whatever was showing when the session ended — the same rule Show
already follows.

The entry points at the running program's own path, resolved through any symlink
used to start it, so it names the binary that is actually running. Running from
a build directory this means the entry follows that directory, and a build
deleted afterwards will not come back at login; it is worth installing properly
before relying on it.

The Linux entry also needs an icon, which a program that draws its own does not
otherwise have. An installed package puts `vclock.svg` in the icon theme and the
entry simply names it. Running from a build directory there is no such file, so
ticking the box draws one to `~/.local/share/vclock/icon.png` instead. Unticking
leaves it: it costs a few kilobytes and saves redrawing it next time.

The macOS agent sets `KeepAlive` false, or quitting the program would bring it
straight back, and `LimitLoadToSessionType` to `Aqua`, or it would also be
started for ssh and cron sessions where there is no display to draw a clock on.
On Windows the uninstaller clears the Run value, since an entry naming a deleted
program is one Windows goes looking for at every login.

`-h`, `--help` prints the options and exits.

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
| `src/embedded.h` | the built-in SVG faces |
| `src/config.*` | the settings record, its defaults, and JSON load/save |
| `src/face.*` | SVG rasterising, recolouring, and content bounds |
| `src/render.*` | hands, hour/minute indices, the appearance presets, and the app icon |
| `src/colorbutton.*` | a colour swatch button with a live preview |
| `src/colorpicker.*` | the colour picker: wheel, S/B sliders, hex and RGB fields |
| `src/settingsdialog.*` | the Settings window |
| `src/autostart.*` | writing and removing the login startup entry |
| `src/clockwindow.*` | the translucent clock window itself |
| `distro/` | packaging: one recipe per target, and the desktop entry a Linux install ships |
| `distro/packaging.cmake` | the CPack settings the deb and rpm are built from |
| `.github/workflows/` | the release build, and the only place macOS is built |

## Notes on the port

This is a port of `vclock.py` (GTK 3 + PyGObject + librsvg + cairo + NumPy) to
C++ and Qt, keeping the behaviour, settings, config format and look. The only
deliberate omission is the Python version's `quiet_stderr()` filter, which
existed solely to suppress Fontconfig chatter from the Linux GTK stack.
