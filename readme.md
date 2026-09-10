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

### Starting from nothing

`--clean` removes everything a vclock build has ever left on the machine, and
then the rest of the command line runs exactly as though it had not been given:

```sh
./build.sh --clean                  # clean, then a release build
./build.sh --clean --type Debug     # clean, then a debug build
./build.sh --clean --distro all     # clean, then every package
```

What it removes is what vclock alone put there — the build directories, the
packages in `distro/out`, and the `vclock-build-*` container images that
`--distro` builds in. Packaging then rebuilds those images from their bases
with `--no-cache --pull`, so the compiler and the Qt inside them are whatever
the distribution ships today rather than whatever it shipped the week the layer
was first cached. That is the point of it, and also why it is slow: reckon on
twice the time of an ordinary `--distro all`.

What it leaves alone is anything shared. The `ubuntu` and `fedora` base images
stay, because a machine that builds vclock may well build something else on the
same bases; `--pull` refreshes them in place instead. Docker's build cache and
any already-dangling images stay too, for the same reason — once an image is
dangling there is nothing left to say whose it was. If you want that space
back, `docker system df` will show you what is there and the decision is yours
rather than this script's.

Something installed by `--install` is not removed either. Those files are under
`/usr/local`, put there as root, and the manifest listing them lives in the
build directory `--clean` deletes; guessing at the contents of `/usr/local`
would be worse than leaving it.

### Dependencies by platform

* **Linux** — `qt6-base-dev qt6-svg-dev libxcb1-dev` (Debian/Ubuntu),
  `qt6-qtbase-devel qt6-qtsvg-devel libxcb-devel` (Fedora, RHEL via EPEL),
  `qt6-base qt6-svg libxcb` (Arch), `qt6-base-devel qt6-svg-devel libxcb-devel`
  (openSUSE) or `qt6-qtbase-dev qt6-qtsvg-dev libxcb-dev` (Alpine);
  `./build.sh --check-deps` works this out for you. The xcb headers are for one
  thing only: telling the window manager which clocks belong on top, one clock
  at a time. A compositing window manager is needed for the transparent
  background; without one the window falls back to an opaque rectangle.
* **macOS** — Qt from Homebrew (`brew install qt`) or the Qt installer. The
  build produces a `vclock.app` bundle.
* **Windows** — Qt from the Qt installer with MSVC or MinGW. The executable is
  built with `WIN32`, so it does not open a console window.

## Packaging

`./build.sh --distro` builds installable packages rather than a binary in the
build tree:

```sh
./build.sh --this                        # the package this machine installs
./build.sh --distro deb                  # a .deb for this machine
./build.sh --distro deb --arch arm64     # a .deb for aarch64
./build.sh --distro rpm,windows          # more than one target
./build.sh --distro all                  # everything this host can build
```

`--this` works out both halves for you: it reads `/etc/os-release` and the
machine's architecture and builds the package this system would install — a
`.deb` on Debian and Ubuntu and anything derived from them, an `.rpm` on Fedora,
RHEL and openSUSE. It says which long form it settled on, so
`./build.sh --this` on an Ubuntu x86_64 box reports that it is doing the same as
`--distro deb --arch amd64`. Combining it with either of those is refused rather
than resolved, since the two would be saying different things. It is also the
quick one: see below.

Arch and Alpine have no package target here, so `--this` stops and says so.
Neither is stuck: both can still build a deb or an rpm, because those are built
in containers rather than out of whatever the host happens to be.

Packages land in `distro/out`. `--arch` takes `amd64`, `arm64` or `all`, and
accepts `x86_64`, `x64` and `aarch64` as names for the same two things; it
defaults to this machine's architecture, except with `--distro all` which
defaults to every architecture each target has. A target only builds the
architectures it has — `windows` is x64 only — so asking for `all` of both
builds the combinations that exist rather than failing on the ones that do not.

Because packaging compiles inside a container and not here, the options that
describe a build on this machine — `--type`, `--jobs`, `--qt-dir`,
`--build-dir`, `--run`, `--install`, `--prefix` — have nothing to act on and
are refused. They used to be accepted and quietly dropped, so
`--distro deb --type Debug` produced a Release package without a word about it.
`--clean` is the exception, and is not a contradiction: it describes the state
to start from rather than how to compile, and applies just as well to a
container image as to a build tree.

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

### Version numbers

Every package takes its version from one place, the `project()` line in
`CMakeLists.txt`. Bump that and all of them follow; there is nothing else to
edit.

The names they come out with are not all spelled the same way, which makes them
look inconsistent when they are not:

```
vclock_1.0_amd64.deb                   version 1.0
vclock-1.0-1.fc41.x86_64.rpm           version 1.0, release 1.fc41
vclock-1.0-windows-x64-setup.exe       version 1.0
```

The rpm's second number is not part of the version. RPM names a file
`name-version-release.arch`, where the release counts rebuilds of the same
source — `1.0-2` would be this same vclock packaged a second time, after
correcting something about the packaging rather than the program. Debian has
the same field and CPack leaves it off when it is unset, which is the only
reason the `.deb` looks shorter: vclock is a native package there, where
upstream and packaging are the same project and a separate revision means
nothing.

The release cannot be moved somewhere less confusing — `name-version-release`
is what every rpm tool parses — but it can be spelled the way the rest of the
system spells it. `fc41` is the dist tag, and packages carry one as a matter of
course: `libgcc-14.3.1-4.fc41.x86_64.rpm`. Beside those, a bare `1` was the
unusual spelling, and the one that reads like a second version. It also keeps
two builds apart, since the same source packaged on Fedora 41 and on Fedora 42
would otherwise produce the same file name.

The deb and the rpm get the version from CPack, which reads the CMake variable
directly. The Windows installer and the macOS bundle are built outside CMake and
read the same line out of `CMakeLists.txt` themselves; if they cannot, they stop
rather than carry on with a guess, because a wrong version in a shipped artefact
is worse than a build that fails.

One thing that is not a version and must not be bumped with the others:
`Version=1.0` in `distro/vclock.desktop` is the Desktop Entry Specification the
file conforms to, not the version of vclock.

Each package is built inside a container for the distribution it targets, so
what comes out depends on that distribution rather than on whatever is installed
here — which is what makes it possible to build a Fedora rpm on Ubuntu, and an
aarch64 package on an x86\_64 machine. Docker is the only requirement, except on
the `--this` path described below.

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

`--this` is the one exception: when the package is for the architecture it is
running on, it builds here instead of in a container, which takes about ten
seconds rather than several minutes. That trades away the compatibility
guarantee above — built on a newer Ubuntu, the deb picks up that release's
glibc and Qt and will not install on 24.04 — which is a fair trade for a package
going straight back onto the machine that built it, and the wrong one for a
package being handed to somebody else. So it applies to `--this` alone;
`--distro` and `all` always use the container. The native path also stands down
whenever it cannot do the job properly and says why, rather than producing
something subtly different: a foreign architecture, a target other than deb or
rpm, or a missing `cmake`, `rpmbuild` or `dpkg-deb` all fall back to the
container.

Building for a foreign architecture runs the container under qemu. That is
several times slower than a native build but produces genuine native binaries.
Running foreign binaries at all needs a handler registered with the kernel, and
`build.sh` registers one itself when it reaches the first package that needs it
— asking for every package on an amd64 machine does not disturb the kernel until
it gets to the arm64 ones. It takes the handler away again when the run ends,
whether that run finished, failed, or was interrupted, so the machine is left
able to run exactly what it could run beforehand.

A handler that was already registered before the build started is left alone,
both while the build runs and afterwards. If you would rather have one
permanently — it saves a few seconds a run, and is what a machine that builds
arm64 packages all day wants — register it yourself and `build.sh` will simply
use it:

```sh
docker run --privileged --rm tonistiigi/binfmt --install arm64
```

That is a host-wide change, and not one that survives a reboot: it lives in
`binfmt_misc`, which is kernel state, so making it permanent means a line in
`/etc/binfmt.d` or a command that runs at boot. Registering it per-run is why
`build.sh` does not ask you to care about any of that.

The Windows installer is cross-compiled rather than built on Windows. Fedora is
the only mainstream distribution that packages a MinGW-w64 build of Qt6, which
is why that container is a Fedora one. The Qt DLLs are found by walking the
import tables of the binary and then of each DLL it names, so nothing has to be
listed by hand; the plugins Qt loads by directory — `qwindows.dll` above all —
are copied separately, because nothing links against them and the walk cannot
see them. The build fails rather than ships if `qwindows.dll` is missing, since
without it the program starts and immediately aborts.

Windows locks the file of a running program, so installing over a copy that is
still on screen used to fail partway through with *Error opening file for
writing* — after some of the DLLs had already been replaced. The installer and
the uninstaller now ask a running vclock to close first, with `taskkill`, which
is part of Windows and so needs no NSIS plugin beyond the `nsExec` that ships
with NSIS. The polite form is used, which posts `WM_CLOSE` and lets the program
write its settings out; it is retried for ten seconds before a copy that has
stopped answering is killed outright. That is only safe because an outside
close no longer reads as hiding a clock — see [Using it](#using-it) — or every
install would silently take the user's clocks off screen.

The last page offers to run vclock, ticked, as installers usually do. It is
worth a little more than usual here: an upgrade closes the copy that was
running, so the clocks were on screen a moment ago and leaving the user with
nothing is the surprising outcome.

Writing to Program Files needs an elevated installer, but the program must not
inherit that. Everything vclock remembers is per user — the configs under
`%APPDATA%` and the *Start at login* value under `HKCU` — so a first run under
the wrong token writes them into another profile, and the settings then appear
to vanish the next time it is started normally. `Exec` would hand the program
the installer's token, so the launch goes through `explorer.exe` instead:
Explorer runs as the logged-in user, and what it starts inherits its token
rather than the installer's. That needs no plugin, which matters because the
NSIS in the build container ships neither `UAC` nor `ShellExecAsUser`.

#### Installing over an existing copy

Run the installer on a machine that already has vclock and the first page it
shows is not the directory page but a choice of three:

| | |
| --- | --- |
| Upgrade or repair, keeping my settings | the default, and what an upgrade is |
| Reset all settings, then reinstall | for a configuration that has been arranged into a corner |
| Reset all settings only, without reinstalling | the same, without replacing files that are already correct |

The page appears only when there is something installed. On a fresh machine the
other two options would be offered against nothing, so it is skipped entirely
and the first thing seen is the directory page, as before. An existing install
also fixes where this one goes, so the directory page is skipped in turn — the
location was decided the first time and changing it silently would leave two
copies.

Uninstalling is where the settings question belongs, since that is the path
Windows itself offers through Settings → Apps. The uninstaller asks once, on a
page that replaces the stock *are you sure*: **Also remove my settings**,
unticked, because settings are cheap to keep and expensive to lose and an
uninstall is often just the first half of an upgrade. *Start at login* is
cleared either way — an entry naming a program that is gone is one Windows goes
looking for at every login.

Nothing is ever deleted. A reset, and an uninstall that was told to remove the
settings, **rename** `%APPDATA%\vclock` to `%APPDATA%\vclock.bak`. One backup is
kept, so a second reset replaces the first rather than leaving a collection of
folders nobody will look at again. A misread prompt therefore costs a rename
rather than every clock the user had arranged.

The same choices are available to anyone scripting an install, since a silent
one cannot click a radio button:

    vclock-setup.exe /S               # install or upgrade, keeping settings
    vclock-setup.exe /S /RESET        # put settings aside, then install
    vclock-setup.exe /S /RESETONLY    # put settings aside and stop
    Uninstall.exe /S                  # uninstall, keeping settings
    Uninstall.exe /S /PURGE           # uninstall and put settings aside

One caveat, and it is the elevation problem again from the other end. The
settings are per user and the installer is elevated. If the person at the
keyboard is an administrator who merely clicked through the UAC prompt, the
elevated process keeps their profile and `%APPDATA%` is theirs. If a standard
user instead typed *somebody else's* administrator credentials, the elevated
process belongs to that other account and `%APPDATA%` points at its profile,
where there is unlikely to be anything to reset. Resolving the invoking user's
profile from an elevated process means going after the shell's token, which is a
good deal of Win32 for something that would go wrong silently. So the path being
reset is printed on the page instead: the one case where this does the wrong
thing is the one where the wrong path is on screen to see.

### Icons on Windows and macOS

Everywhere else the program draws its own icon once it is running, and that is
enough: Linux takes the window icon from the running process and the installed
`vclock.svg` for the menu. Windows and macOS both want an icon *in the file*,
because both show one before the program has started — in Explorer, on the
Start menu, in Finder and in the Dock. A build without one is not broken, but
the first thing a new user sees is a blank page glyph.

So two icon files are committed, both built from `vclock.svg` by
`distro/make-icons.sh`:

| File | Used by |
| --- | --- |
| `distro/windows/vclock.ico` | `distro/windows/vclock.rc`, compiled into the `.exe`; also the installer and uninstaller icons |
| `distro/macos/vclock.icns` | copied into `vclock.app/Contents/Resources` |

They are committed rather than generated during the build because neither
platform can produce them where it is built: a Windows machine has no SVG
rasteriser, and macOS has no command-line one either unless somebody has
installed `librsvg`. Generating them would mean the icon quietly disappearing on
the machines least able to notice. `distro/macos/package.sh` still rebuilds the
`.icns` with `iconutil` — Apple's own tool, always present — when a rasteriser
is there to feed it, so an edit to the artwork reaches the bundle even if the
committed file has not been refreshed; the committed one is the floor, not the
ceiling. Run `distro/make-icons.sh` after changing `vclock.svg`; it needs
`librsvg2-bin`, `imagemagick` and `icnsutils`.

The `.ico` holds nine sizes from 16 to 256 rather than one. Windows picks
between them by context, and a file offering a single size gets a scaled copy
of it everywhere else — which is what a blurry icon in a title bar usually is.

### macOS

macOS is the one target that cannot be built here. This is not a gap in the
tooling: Apple's SDK licence restricts building to Apple hardware, and since
Catalina an app that is neither signed nor notarised is refused by Gatekeeper
rather than merely warned about. `./build.sh --distro macos` says so rather than
pretending. Run on a Mac, `--this` names that target and then points at the
script below, rather than repeating that Apple hardware is required to somebody
who is sitting at some.

The recipe is `distro/macos/package.sh`, which runs on a Mac and produces a
`vclock.app` and a disk image. It uses `macdeployqt` to copy the Qt frameworks
into the bundle and rewrite the binary's load paths to point inside it, so the
result runs on a machine that has no Qt installed. Set `CODESIGN_IDENTITY` to
sign; without it the build still works but Gatekeeper will object anywhere but
the machine that built it.

#### Uninstalling on macOS

The disk image also carries `uninstall.sh`, which is there for one reason. An
app installed by dragging it out of a disk image is removed by dragging it to
the Trash, and for most programs that is genuinely all there is to it —
everything they own lives inside the bundle. vclock is not quite one of those
programs: ticking *Start at login* writes a launchd agent to
`~/Library/LaunchAgents`, which is outside the bundle and so survives the Trash.
What is left behind is an agent naming an application that no longer exists,
which launchd tries to start at every login for as long as the account lasts.
Nothing can hook drag-to-Trash to prevent that, so the answer is a script:

    ./uninstall.sh              # app and login agent, settings kept
    ./uninstall.sh --purge      # settings too
    ./uninstall.sh --purge --yes

It prints what it is about to do and asks before doing any of it, quits a
running copy first — deleting the bundle out from under it would also let it
write its settings back on the way out — and unloads the agent with `launchctl`
before removing the plist, since launchd holds a job in memory once it has read
it. Settings are renamed to `vclock.bak` rather than deleted, matching Windows.
It needs no administrator rights: `/Applications` is writable by admin users,
who are who installed the app, and everything else is in the user's own home.

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
| Hover | the date and time, spelled out |
| Left drag | move the clock |
| Double click | settings |
| Wheel | resize, while Settings is open (`Shift` for finer steps) |
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

Hiding is the only thing that marks a clock as not showing. A close that comes
from outside the program -- a session logging out, or the Windows installer
clearing the way for a new copy -- means the program is stopping, not that the
user put a clock away, so it is treated as Quit: the settings are written out
and every clock stays marked as showing, so they all come back next time. The
two arrive identically as a window close, so Hide sets a flag on its way in and
anything without that flag is taken as the outside kind. Read as a hide
instead, one logout would quietly leave all but one of the clocks off.

Each menu entry shows its shortcut in a right-hand column.

A drag only begins once the pointer has actually travelled a few pixels, so a
double click that stays put opens Settings instead of being swallowed by the
start of a move. The clock therefore trails the pointer by that small threshold
for the rest of a drag, which is the ordinary feel of dragging anything.

The wheel over the clock resizes it, but only while its Settings dialog is
open. The rest of the time the clock is a thing sitting on the desktop, and a
wheel over it belongs to whatever is underneath; with Settings open you are
plainly adjusting this clock, and the wheel sizes it against what is behind it
-- which the size slider cannot show you, because the dialog is in the way.
Each notch is a tenth of the current size rather than a fixed number of pixels,
so it is the same visible change on a large clock as on a small one, and
`Shift` cuts the step to two percent. The plain wheel is the coarse one on
purpose: you spin a wheel to cross a distance and then creep the last little
way, and creeping is the part worth holding a key for. It moves the slider
rather than the clock directly, so the number on screen keeps up and Cancel
still puts back the size you started with.

The clock refuses to be minimised, maximised, or made full screen, whether the
request comes from the window manager, a "show desktop" key, or a tiling
shortcut. Because it keeps out of the taskbar and the window switcher, being
iconified would leave no way to get it back.

### The date on hover

Resting the pointer on a clock brings up a small bubble beside it:

```
     Thursday
September 3rd, 2026
   11:17:52 PM
```

An analog face gives the time to the nearest minute and says nothing at all
about the date, so the bubble fills in both, down to the second. It waits for
the pointer to settle before appearing, so crossing a clock on the way
somewhere else does not summon it, and it goes as soon as the pointer leaves or
a button goes down. The seconds keep running while it is up.

*Date on hover after*, at the foot of the manage dialog, sets how long that
wait is; it starts at three seconds. Winding it down past zero reads *never*
and stops the bubble appearing at all -- no time at all is not a wait anyone
would ask for on purpose, which leaves the bottom of the range free to mean the
one other thing you might want from the setting. Like *Start at login* it is
one answer for the program rather than a property of any one clock, which is
why it lives there and not in a clock's own settings. Clocks read it as they
are hovered, so a change takes hold on the next hover rather than the next run.

The bubble is white on black rather than the desktop's own tooltip colours: a
clock usually sits over a photograph or a bright window, and the pale yellow
most desktops use for tooltips gets lost against that. Its text is drawn with
greyscale antialiasing rather than the subpixel kind, which works by lying
about colour along the edge of every stroke -- unnoticeable on a pale
background, and an orange-and-blue fringe around white letters on black.

### Always on top

The clock is kept above other windows by default; *Always on top* in the menu
turns that off. Existing configs keep whatever they already had.

It is a setting per clock, and it now behaves as one. On X11 every window a
program opens belongs to a single window group, and window managers stack a
group as a unit -- whichever member is highest, the rest are raised to meet it.
That is right for a document and its dialogs and quite wrong for a set of
clocks, whose only relationship is having been started together: turning
*Always on top* on for one of them turned it on for all of them. Each clock is
now taken out of that group, so it stacks on its own.

The switch also asks the window manager directly rather than going through Qt's
window flag. Changing that flag makes Qt destroy the native window and build
another, which loses the clock's position and unmaps it for a moment; asking
the window manager leaves the window alone.

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

Six faces are built in and need no files: the plain default ring; the gradient
dial the app icon is drawn from (`builtin:icon`); a silver dial under a dark rim
(`builtin:silver`), which is the same gradient running the other way; a
honeycomb (`builtin:honeycomb`), whose lit wax walls take the face colour and
whose cells take the wire colour; and a spiral (`builtin:spiral`), one arm
winding six turns out of the hub and broadening into the rim, which takes those two colours the
other way round -- the dial is the face colour and the arm the wire. Whichever is in use, the
*Clock face svg* field names it when no file of your own is loaded; clicking a
preset is how you return to a built-in face.

The sixth is the kaleidoscope (`builtin:kaleidoscope:<seed>`), and it is the
only one that is not drawn in advance but worked out when it is asked for. Its
name carries the digits it was generated from, so a face is stored as a seed
rather than as a picture: two words in the config that redraw the same dial
after a save, a reload, or a copy to another clock. It is also the only
built-in in full colour, so it wants *Original* colouring; recolouring would
flatten it to two tones, which is the one thing this face is not. Clicking the
**Kaleidoscope** preset rolls a new seed each time, and the preset's own
thumbnail is redrawn to show what you just got.

Every shape is outlined, which is what stops a dozen wedges of flat colour from
running together into a smear. The lines widen with the radius they are drawn
at, so the pattern is pencilled in at the hub and inked at the rim; without
that the middle, where every wedge's shapes crowd into the same small space,
fills in solid.

Being generated, it is also the one face that reads its two colours as an
instruction about how to draw rather than as a way of repainting what was
drawn, so the **random** tick beside *Face color* means a little more here than
it does elsewhere. Ticked -- which is how the preset leaves it -- the face is a
scheme of several hues built around the colour in the box. Unticked, it is that
one colour and its shades. Either way the seed decides the shape and the
colours only decide the paint, so you can settle on a pattern and then recolour
it, or click the preset again for another arrangement in the colours you have.

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

The thumbnails at the top of Settings are whole default clocks, not just
colour schemes. Clicking one restores every appearance setting, and also puts
the hand centre back to its default — so a preset gives you exactly the clock in
the thumbnail, at the size you already had. How big the clock is and where it
sits on screen are left alone, since those are placement choices rather than a
look. As with any other change, the preset is only a preview until Save; Cancel
puts the previous appearance and centre back.

Three of the ten are see-through: **Smoked glass** is the Onyx dial faded back
to about a quarter, **Clear glass** has no dial at all, only a pale rim, heavy
hour marks and hands hanging over the wallpaper, and **Spiral** is Smoked glass
on a dial of its own -- a single silver arm leaving the centre as a hairline,
winding six turns and thickening as it goes until it runs out through the edge,
its last stretch merged into the rim. Nothing else is drawn on it: no hour
marks, no minute track and no second hand, because the spiral is already a
drawing that fills the face, and marks set around the rim only fence it in. The
time is read off two hands against the turns of the arm.

Every thumbnail is the clock and nothing else -- no backdrop, no checkerboard,
nothing to say "this one is transparent". A see-through preset simply lets the
dialog show through, exactly as it will let your wallpaper show through. What
you see is what you get, and a thumbnail that needed decoration to explain
itself would not be one.

A preset changes how the clock looks and nothing else, so smooth sweep, reverse
time, always on top and the clock's size all survive it. If you want the glass
clocks with a gliding second hand, tick *Smooth sweep hands* on the Hands tab
after clicking the preset.

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

The controls sit on four tabs -- **Face**, **Marks**, **Hands**, **Opacity**.
The first three are the parts of the clock, in the order they are drawn: the
face behind, its marks on top of that, the hands over both. Each carries
everything about its own part, sizes included: someone adjusting the hour marks
wants their colour, their size and their position within reach, not the colour
on one page and the size on another. **Opacity** is the one page left that cuts
across all three.

Clock size heads the Face tab. It is the only setting that is about the clock
rather than about any part of it, and it is the one reached most often, so it
goes where the dialog opens rather than on a page of its own.

Beside the hour and minute mark colours is an **enabled** box, which is a
second view of that mark's size slider: unticking it takes the size to zero and
remembers what it was, so ticking it back on returns the marks you had rather
than a made-up default. Zeroing the slider by hand unticks the box for the same
reason -- there is one setting there, not two.

The presets stay above the tabs rather than living on one of them. A preset
writes to every tab at once, so on a tab it would silently change pages you
cannot see, which is the one thing tabs are bad at. Above them it plainly
belongs to the whole dialog. They wrap at six to a row, so a new preset makes
the box taller rather than the dialog wider.

Opacity keeps all four sliders together for the same reason: *sync face/wire*
and *sync hands/marks* each tie a pair, and splitting hands and marks across
their own tabs would leave a checkbox governing a slider on another page.

The dialog is sized to its tallest tab and does not resize when you switch, so
the buttons stay under your mouse. The price is some empty space below the
shorter tabs, which is the better half of that trade.

If the screen is not tall or wide enough for the whole dialog, the controls
scroll and the Save and Cancel buttons stay pinned below them, so they are
always reachable. Scrollbars appear only when they are actually needed.

**Manage clocks…** sits apart from Save and Cancel, at the far left, because it
commits nothing. Settings governs one clock, and while it has focus the Ctrl+K
that would otherwise reach Manage clocks goes to the dialog rather than to the
clock behind it — so without that button the per-clock view is a dead end. Both
windows then stay open together: the hub can reach any clock's settings, and now
a clock's settings can reach the hub.

**Reset** is centred between them, and it asks first, since it throws away a
whole clock's worth of choices at once. It offers two of them: *Undo my changes*
puts the controls back to how they were when Settings opened, which is what you
want when a few minutes of sliders have gone wrong, and *Restore the defaults*
goes all the way back to a factory clock. *Reset defaults* in the clock's own
menu only does the second — there is no sitting there to undo — and it saves
straight away, while the button only moves the controls and previews the clock,
so Cancel puts everything back and nothing is written until Save. That follows
the rule the rest of the dialog already keeps — while Settings is open, Save
and Cancel decide.

Because it acts on the dialog's own controls, the button resets only what the
dialog shows. Always on top has no control here, so it is left as it is; the
menu's *Reset defaults* covers that one too.

Every slider has its value in a box beside it that can be
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
clock itself never becomes unreachable, because what takes a click is worked out
from the artwork as drawn at full strength rather than from what you can see, so
right clicking a clock faded to nothing still opens the menu.

That reach stops at the drawing. A clock's window is square, but the artwork in
it is not, and the corners left over are not the clock's to take: a click there
goes to whatever is behind, the window or the desktop, as though the clock were
not in the way. So a clock may sit over a window you are working in without
stealing the clicks that land beside it. The shape follows the hands and the
indices too, which a large **Hour mark position** can carry out past the dial,
so anything you can see is something you can click.

The blue on the sliders, on the selected row in Manage clocks and on selected
text is the program's own, not the desktop's. Qt draws those in whatever colour
the system nominates: the theme's highlight on Linux, and on Windows the system
accent colour, which the Windows 11 style paints slider grooves with directly —
so on a machine whose owner had picked red for the taskbar, every slider in the
dialog came out red. A clock is a piece of decoration, and looking the same
everywhere is worth more here than following a setting that was chosen for
something else, so it fixes the colour itself. Only the surrounding controls are
affected; the clock face, the hands and the colour picker all work out their own
colours and never consult the palette.

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
row starts with a grip, then has a Show box and the clock's name, and then four
narrow columns: Set opens that clock's settings, Top keeps it above other
windows, Name renames it (or, on the Default clock, copies it), and Del deletes
it and the config it keeps its settings in. Double clicking a name renames it,
and so does `F2` on the selected row, as in a file manager. `Enter` saves the
new name and `Escape` abandons it.

Drag a row by its grip to put the list in whatever order you want; a line shows
where the row will land, and the order is kept between runs. The Default clock
stays at the top and cannot be dragged, and nothing can be dropped above it: it
is the clock the program falls back on, so it is always in the same place.

**New clock** adds a row and waits for a name. Once you have named it the clock
comes up on screen with its settings already open, since making a clock is the
point at which you have something in mind for it, and it saves going back to the
list to ask. Leaving the name blank, or pressing `Escape`, drops the new row.

The Default clock cannot be renamed, so where its Name button would be there is
a clone button instead, which makes a clock that starts out as a copy of it.
Naming the copy works exactly as **New clock** does, and the settings are copied
once there is a name to copy them into. The copy does not come up with its
settings open: it already looks how you wanted it to.

A copy carries the original's place on screen along with its looks, so it comes
up exactly where the clock it was copied from was, and that clock goes down as
it does. What is left on screen is the one clock, in the one place, and it is
the copy -- the one you are now free to change.

Top is the same setting as Always on top in a clock's own menu, gathered here so
the whole set can be seen and changed in one place. It applies to that one clock
only. On a clock that is not showing there is no window to raise, so the setting
is written to its config and takes effect when it next comes on screen.

Show puts a clock on screen and takes it off again. Whatever is showing when
vclock stops is what comes back when it starts again, so there is nothing
separate to set for that. If every clock is hidden the default one comes back
rather than the program starting with no windows at all.

The list lives in `vclocks.cfg` in the config directory, alongside the per-clock
configs. The few settings that belong to the program rather than to a clock --
the hover wait, for one -- are kept in that file too, beside the list, rather
than repeated in every config.

### Names and files

A clock's name is its file name: **Kitchen** keeps its settings in
`Kitchen.cfg`. Renaming the clock renames the file, so the two never drift
apart, and you can find a clock's settings on disk by reading its name off the
list. The `.cfg` is the program's to add -- typing it yourself is harmless, it
is simply taken off again.

That makes a name subject to the rules a file name is, and vclock applies the
strictest set across the platforms it runs on rather than whichever the machine
in front of you happens to use, so a config directory can be copied from one to
another and still work. A name is refused if it is empty, contains any of
`\ / : * ? " < > |`, starts or ends with a dot, is one of the device names
Windows reserves (`CON`, `PRN`, `AUX`, `NUL`, `COM1`-`COM9`, `LPT1`-`LPT9`), or
comes to more than 255 bytes with the `.cfg` on the end. Names vclock keeps for
itself -- `default`, `vclock`, `vclocks` and `clocks` -- are refused too, and so
is a name another clock already has, compared without regard to case, since two
clocks sharing a name would share a file. A name that would land on a `.cfg`
already sitting in the config directory is refused as well, rather than
swallowing settings that belong to something else.

A refused name is not quietly changed into an acceptable one. The dialog says
what is wrong and waits for OK, and then puts you back in the editor with what
you typed still there to correct. `Escape` from there abandons the rename, as it
always does.

The **Default** clock is the exception: its file is `default.cfg`, which is what
a clock started with no `-c` writes and what vclock falls back to, so its name
is not yours to change and its Name button is greyed out. `default` is refused
for every other clock for the same reason.

Clocks made by older versions of vclock keep the file names they were given.
Renaming such a clock moves it onto the new footing.

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
program is one Windows goes looking for at every login. macOS has the same
problem and no uninstaller to solve it — dragging the app to the Trash leaves
the agent behind — so the disk image carries `uninstall.sh`, which removes it.

`-h`, `--help` prints the options and exits.

### Monitors

A clock that has never been sized -- a new one, or one whose settings have just
been reset -- opens at a fifteenth of its screen's height, in the top left
corner of the working area. A fixed pixel count
cannot suit every panel: what sits neatly in the corner of a 1080 screen is a
stamp on a 4K one. Any size you set yourself is kept as it is, per monitor, and
never second-guessed. The top left corner is used because it is the one place
that is free of the taskbar on every platform, and because a new clock landing
in the middle of the screen covers whatever you were looking at.

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
| `src/timetip.*` | the date and time bubble shown on hover |
| `src/windowgroup.*` | keeping each clock's stacking its own, on X11 |
| `distro/` | packaging: one recipe per target, and the desktop entry a Linux install ships |
| `distro/packaging.cmake` | the CPack settings the deb and rpm are built from |
| `distro/make-icons.sh` | rebuilds the Windows and macOS icon files from `vclock.svg` |
| `.github/workflows/` | the release build, and the only place macOS is built |

## Notes on the port

This is a port of `vclock.py` (GTK 3 + PyGObject + librsvg + cairo + NumPy) to
C++ and Qt, keeping the behaviour, settings, config format and look. The only
deliberate omission is the Python version's `quiet_stderr()` filter, which
existed solely to suppress Fontconfig chatter from the Linux GTK stack.
