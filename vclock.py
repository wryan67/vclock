#!/usr/bin/env python3
"""Analog desktop clock widget that draws its hands over an SVG clock face.

The face is any SVG; its artwork is recoloured (line work -> "wire" colour,
body -> "face" colour) and rasterised at the widget's current pixel size, so
it stays crisp at any scale.  The window is undecorated and uses an RGBA
visual, so everything outside the artwork is genuinely transparent.

Left-drag moves the widget, right-click opens Settings / Quit, Escape quits.
Settings live in a platform-native config directory, including the last
position: $XDG_CONFIG_HOME (or ~/.config) on Linux, ~/Library/Application
Support on macOS, and %APPDATA% on Windows.
"""
import atexit
import json
import math
import os
import signal
import sys
import threading
import time

import cairo
import gi
import numpy as np

gi.require_version("Gtk", "3.0")
gi.require_version("Gdk", "3.0")
gi.require_version("Rsvg", "2.0")
from gi.repository import GLib, GObject, Gdk, GdkPixbuf, Gtk, Rsvg  # noqa: E402

# Where the file chooser starts looking for user-supplied faces.
FACE_DIR = os.path.expanduser("~/Downloads")

# The built-in default face, embedded so the program has no external art
# dependency: it is used on first run, whenever no config exists, and whenever
# the "default" box is ticked.
DEFAULT_FACE_SVG = b"""<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">
  <circle cx="50" cy="50" r="40" stroke="black" stroke-width="3" fill="white" />
</svg>
"""
DEFAULT_FACE_LABEL = "built-in"

# A second built-in face: the app icon's dial with its painted hands, ticks
# and pin removed, so vclock draws those itself from the settings.  It is
# greyscale because recolour() maps brightness onto the wire/face colours;
# a wide brightness range is what makes the gradient survive recolouring.
ICON_FACE_SVG = b"""<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">
  <defs>
    <linearGradient id="clockGradient" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#a8a8a8" />
      <stop offset="100%" stop-color="#101010" />
    </linearGradient>
  </defs>
  <circle cx="50" cy="50" r="40" stroke="#e8e8e8" stroke-width="3" fill="url(#clockGradient)" />
</svg>
"""

# Faces that live inside the program rather than on disk.  They are stored in
# the config as "builtin:<name>", which no real path can collide with.
BUILTIN_FACE_PREFIX = "builtin:"
BUILTIN_FACES = {"icon": (ICON_FACE_SVG, "gradient dial")}

# The application icon, embedded so the program does not depend on a system
# icon theme (which is often absent on Windows and macOS).
APP_ICON_SVG = b"""<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">
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
"""


def app_icon(size):
    """Render the built-in icon to a GdkPixbuf, or None if that fails."""
    try:
        handle = Rsvg.Handle.new_from_data(APP_ICON_SVG)
        ok, art_w, art_h = handle.get_intrinsic_size_in_pixels()
        if not ok or not art_w or not art_h:
            art_w = art_h = float(size)
        surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, size, size)
        cr = cairo.Context(surface)
        cr.scale(size / art_w, size / art_h)
        rect = Rsvg.Rectangle()
        rect.x = rect.y = 0.0
        rect.width, rect.height = art_w, art_h
        handle.render_document(cr, rect)
        surface.flush()
        return Gdk.pixbuf_get_from_surface(surface, 0, 0, size, size)
    except Exception:
        return None


IS_WINDOWS = sys.platform == "win32"
IS_MAC = sys.platform == "darwin"

# macOS users expect Cmd; everyone else expects Ctrl. Ctrl is accepted on all
# platforms regardless, so this only affects what the help text advertises.
CMD_LABEL = "Cmd" if IS_MAC else "Ctrl"
QUIT_KEYS = "Cmd+Q, Cmd+W, Esc" if IS_MAC else "Ctrl+C, Alt+F4, Esc"


def config_dir():
    """Platform-native directory for the settings file."""
    if IS_WINDOWS:
        base = os.environ.get("APPDATA") or os.path.expanduser("~")
    elif IS_MAC:
        base = os.path.expanduser("~/Library/Application Support")
    else:
        base = (os.environ.get("XDG_CONFIG_HOME")
                or os.path.expanduser("~/.config"))
    return os.path.join(base, "vclock")


CONFIG_DIR = config_dir()
CONFIG_PATH = os.path.join(CONFIG_DIR, "vclock.cfg")
# Settings written by earlier releases, read once when the current path has no
# file yet so an upgrade (or a move to a platform-native path) keeps its config.
LEGACY_CONFIG_PATHS = (
    os.path.expanduser("~/.config/vclock/vclock.cfg"),
    os.path.expanduser("~/.config/fclock/fclock.cfg"),
)

SIZE_MIN = 50
SIZE_MAX_FALLBACK = 500  # only used if no monitor can be queried

# Hand geometry, as fractions of the usable face radius.  Expressing widths
# as fractions too keeps the hands proportional at every clock size.
HAND_SPAN = 0.98        # the second hand reaches 98% of the face
HOUR_LEN, MINUTE_LEN, SECOND_LEN = 0.62, 0.86, 1.00
HOUR_WIDTH, MINUTE_WIDTH, SECOND_WIDTH = 0.072, 0.043, 0.029
PIN_RADIUS = 0.047
# Hour indices (the tick marks around the dial).  MARK_OUTER is the outer edge
# at 100% position; the size slider grows the marks inwards from there.
MARK_INNER, MARK_OUTER, MARK_WIDTH = 0.88, 0.98, 0.011

# User-adjustable scaling, as whole percentages of the geometry above.
HAND_SCALE_MIN, HAND_SCALE_MAX = 25, 200
MARK_SCALE_MIN, MARK_SCALE_MAX = 0, 200   # 0 hides the indices entirely

DEFAULTS = {
    "size": 400,                 # widget width in px
    "hand_scale": 100,           # percent of the built-in hand geometry
    "mark_scale": 100,           # percent of the built-in hour-index geometry
    "mark_position": 100,        # percent of the built-in index radius
    "minute_mark_scale": 50,     # percent of the hour-index size
    "quarter_marks_only": False, # draw hour indices at 12/3/6/9 only
    "always_on_top": False,      # keep the widget above other windows
    "face_svg": "",              # path to a user-supplied face ("" = none)
    "face_default": True,        # ignore face_svg and use the built-in face
    "second_color": "#8b0000",   # dark red
    "hour_color": "#363d45",     # charcoal
    "minute_color": "#000000",
    "minute_same_as_hour": False,
    "face_color": "#ffffff",
    "face_transparent": False,
    "wire_color": "#000000",
    "hour_mark_color": "#000000",
    "minute_mark_color": "#000000",
    "center": None,              # [fx, fy] fractions of the canvas, None = auto
    "x": None,
    "y": None,
}

# What an appearance preset is allowed to change: everything except the
# window's physical size, stacking and placement, which stay the user's.
PRESET_KEYS = tuple(
    k for k in DEFAULTS
    if k not in ("size", "always_on_top", "center", "x", "y")
)

# Old key names, kept readable for configs written by earlier versions.
RENAMED = {"ship_color": "face_color", "ship_transparent": "face_transparent"}

ABOUT_TEXT = (
    "A transparent, borderless analog clock for the desktop.\n\n"
    "The clock face can be any SVG. Within that artwork white is treated as "
    "the face color and black as the wire color, and both can be recolored "
    "from Settings."
)


# Chatter from C libraries that says nothing useful about the clock.  Fontconfig
# reports "out of memory" for any failed directive, not just real allocation
# failures, and vclock draws no text on the face itself.
STDERR_NOISE = ("Fontconfig error",)


def quiet_stderr(patterns=STDERR_NOISE):
    """Drop matching lines from stderr, passing everything else through.

    The messages come from C code writing straight to file descriptor 2, so
    they can only be intercepted at the descriptor level.  Set VCLOCK_VERBOSE
    to see them again.  The noise is specific to the Linux font stack, so on
    other platforms the descriptor juggling is skipped entirely.
    """
    if os.environ.get("VCLOCK_VERBOSE") or not sys.platform.startswith("linux"):
        return
    try:
        real_fd = os.dup(2)   # kept so the original stderr can be restored
        sink_fd = os.dup(2)   # where the pump writes the lines it keeps
        read_fd, write_fd = os.pipe()
    except OSError:
        return
    os.dup2(write_fd, 2)
    os.close(write_fd)

    def pump():
        with os.fdopen(read_fd, "rb", 0) as src, os.fdopen(sink_fd, "wb", 0) as dst:
            for line in src:
                if not any(p.encode() in line for p in patterns):
                    dst.write(line)
                    dst.flush()

    thread = threading.Thread(target=pump, daemon=True)
    thread.start()

    def drain():
        # Restoring fd 2 closes the pipe's last write end, so the pump sees EOF
        # and can flush what is left -- otherwise a final traceback is lost.
        try:
            sys.stderr.flush()
        except (OSError, ValueError):
            pass
        os.dup2(real_fd, 2)
        thread.join(timeout=1.0)

    atexit.register(drain)


def load_config():
    cfg = dict(DEFAULTS)
    # Settings written under an older name or path are still honoured; the next
    # save writes them to the current location.
    path = CONFIG_PATH
    if not os.path.exists(path):
        path = next((p for p in LEGACY_CONFIG_PATHS if os.path.exists(p)), path)
    try:
        with open(path) as fh:
            stored = json.load(fh)
        if isinstance(stored, dict):
            for old, new in RENAMED.items():
                if old in stored and new not in stored:
                    stored[new] = stored.pop(old)
            # The marks used to be painted in the wire colour.  A config from
            # before they had their own colours keeps its old look.
            for key in ("hour_mark_color", "minute_mark_color"):
                if key not in stored and "wire_color" in stored:
                    stored[key] = stored["wire_color"]
            cfg.update({k: v for k, v in stored.items() if k in DEFAULTS})
    except (OSError, ValueError):
        pass

    try:
        cfg["size"] = int(cfg["size"])
    except (TypeError, ValueError):
        cfg["size"] = DEFAULTS["size"]
    cfg["size"] = max(SIZE_MIN, cfg["size"])  # upper bound depends on the monitor

    cfg["hand_scale"] = clamp_percent(
        cfg["hand_scale"], HAND_SCALE_MIN, HAND_SCALE_MAX, DEFAULTS["hand_scale"])
    cfg["mark_scale"] = clamp_percent(
        cfg["mark_scale"], MARK_SCALE_MIN, MARK_SCALE_MAX, DEFAULTS["mark_scale"])
    cfg["mark_position"] = clamp_percent(
        cfg["mark_position"], MARK_SCALE_MIN, MARK_SCALE_MAX,
        DEFAULTS["mark_position"])
    cfg["minute_mark_scale"] = clamp_percent(
        cfg["minute_mark_scale"], MARK_SCALE_MIN, MARK_SCALE_MAX,
        DEFAULTS["minute_mark_scale"])
    cfg["quarter_marks_only"] = bool(cfg["quarter_marks_only"])
    cfg["always_on_top"] = bool(cfg["always_on_top"])

    cfg["center"] = sanitize_center(cfg["center"])
    if not isinstance(cfg["face_svg"], str):
        cfg["face_svg"] = ""
    if not cfg["face_svg"]:
        cfg["face_default"] = True
    return cfg


def clamp_percent(value, low, high, fallback):
    """Read a percentage from the config, tolerating junk written by hand."""
    try:
        return max(low, min(high, int(round(float(value)))))
    except (TypeError, ValueError):
        return fallback


def sanitize_center(value):
    """Accept only a pair of fractions inside the canvas; else fall back to auto."""
    try:
        fx, fy = float(value[0]), float(value[1])
    except (TypeError, ValueError, IndexError):
        return None
    if not (0.0 <= fx <= 1.0 and 0.0 <= fy <= 1.0):
        return None
    return [fx, fy]


def save_config(cfg):
    try:
        os.makedirs(CONFIG_DIR, exist_ok=True)
        tmp = CONFIG_PATH + ".tmp"
        with open(tmp, "w") as fh:
            json.dump(cfg, fh, indent=2, sort_keys=True)
        os.replace(tmp, CONFIG_PATH)
    except OSError as exc:
        print(f"WARNING: could not save {CONFIG_PATH}: {exc}")


def monitor_max_size(window=None):
    """Largest allowed clock size: the height of the monitor it sits on."""
    display = Gdk.Display.get_default()
    if display is None:
        return SIZE_MAX_FALLBACK

    monitor = None
    gdk_window = window.get_window() if window is not None else None
    if gdk_window is not None:
        monitor = display.get_monitor_at_window(gdk_window)
    if monitor is None:
        monitor = display.get_primary_monitor() or display.get_monitor(0)
    if monitor is None:
        return SIZE_MAX_FALLBACK
    return max(SIZE_MIN, monitor.get_geometry().height)


def rgba_of(hex_color):
    c = Gdk.RGBA()
    c.parse(hex_color)
    return c


def hex_of(rgba):
    return "#%02x%02x%02x" % (
        round(rgba.red * 255), round(rgba.green * 255), round(rgba.blue * 255)
    )


def rgb_tuple(hex_color):
    c = rgba_of(hex_color)
    return (c.red, c.green, c.blue, 1.0)


class Face:
    """An SVG clock face: its handle, aspect ratio and rasterising."""

    def __init__(self, path=None):
        """Load a face from a file, a "builtin:" name, or the embedded default."""
        self.path = path or None
        self.builtin = None
        if self.path and self.path.startswith(BUILTIN_FACE_PREFIX):
            name = self.path[len(BUILTIN_FACE_PREFIX):]
            if name in BUILTIN_FACES:
                self.builtin = name
            else:
                print(f"WARNING: unknown built-in face {name!r}; using the default")
                self.path = None

        if self.builtin is not None:
            self.handle = Rsvg.Handle.new_from_data(BUILTIN_FACES[self.builtin][0])
        elif self.path:
            self.handle = Rsvg.Handle.new_from_file(self.path)
        else:
            self.handle = Rsvg.Handle.new_from_data(DEFAULT_FACE_SVG)
        self.aspect = self._aspect()

    @property
    def is_default(self):
        return self.path is None

    @property
    def label(self):
        if self.path is None:
            return DEFAULT_FACE_LABEL
        if self.builtin is not None:
            return BUILTIN_FACES[self.builtin][1]
        return os.path.basename(self.path)

    def _aspect(self):
        """Height / width of the artwork, from its viewBox or intrinsic size."""
        try:
            dims = self.handle.get_intrinsic_dimensions()
            if dims.out_has_viewbox and dims.out_viewbox.width > 0:
                return dims.out_viewbox.height / dims.out_viewbox.width
            if dims.out_width.length > 0:
                return dims.out_height.length / dims.out_width.length
        except AttributeError:
            pass
        return 1.0

    def render(self, width, height):
        """Rasterise at a pixel size, returning an (h, w, 4) straight-RGBA array."""
        surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, width, height)
        cr = cairo.Context(surface)
        viewport = Rsvg.Rectangle()
        viewport.x = 0.0
        viewport.y = 0.0
        viewport.width = float(width)
        viewport.height = float(height)
        self.handle.render_document(cr, viewport)
        surface.flush()

        buf = np.ndarray(
            shape=(height, surface.get_stride() // 4, 4),
            dtype=np.uint8,
            buffer=surface.get_data(),
        )[:, :width]

        # Cairo hands back premultiplied BGRA on little-endian; unpremultiply.
        alpha = buf[:, :, 3]
        out = np.empty((height, width, 4), np.uint8)
        for dst, src in ((0, 2), (1, 1), (2, 0)):
            out[:, :, dst] = UNPREMUL[alpha, buf[:, :, src]]
        out[:, :, 3] = alpha
        return out


def _unpremul_table():
    """UNPREMUL[alpha][value] -> straight-alpha value, so un-premultiplying is
    a table lookup instead of a per-pixel float divide."""
    a = np.arange(256, dtype=np.float32)[:, None]
    v = np.arange(256, dtype=np.float32)[None, :]
    return np.clip(v * 255.0 / np.where(a == 0, 1.0, a), 0, 255).astype(np.uint8)


UNPREMUL = _unpremul_table()


def open_face(path):
    """Load a face from a path, falling back to the embedded default."""
    if path:
        try:
            return Face(path)
        except GLib.Error as exc:
            print(f"WARNING: could not load face {path}: {exc.message}")
    return Face(None)


def recolor(art, wire_hex, face_hex, face_transparent):
    """Map the artwork's dark line work to wire_hex and its light body to face_hex.

    Anti-aliased pixels are blended between the two, so edges stay smooth.
    Every output pixel depends only on the source pixel's brightness (0..255)
    and its alpha, so the blend collapses into a 256-entry lookup table.
    """
    # t = 0 for pure black (wire), 255 for pure white (face body)
    t = art[:, :, :3].max(axis=2)
    alpha = art[:, :, 3]
    wire = np.array(rgb_tuple(wire_hex)[:3], np.float32) * 255.0

    out = np.empty(art.shape, np.uint8)
    if face_transparent:
        out[:, :, 0], out[:, :, 1], out[:, :, 2] = (
            int(round(c)) for c in np.clip(wire, 0, 255))
        # Fade out the light body, keeping only the wire: alpha * (1 - t/255)
        out[:, :, 3] = (
            (alpha.astype(np.uint16) * (255 - t).astype(np.uint16) + 127) // 255
        ).astype(np.uint8)
    else:
        face = np.array(rgb_tuple(face_hex)[:3], np.float32) * 255.0
        ramp = np.arange(256, dtype=np.float32) / 255.0
        for c in range(3):
            lut = np.clip(wire[c] * (1.0 - ramp) + face[c] * ramp,
                          0, 255).astype(np.uint8)
            out[:, :, c] = lut[t]
        out[:, :, 3] = alpha
    return out


def content_bounds(art):
    """Bounding box (x0, y0, x1, y1) of the artwork's non-transparent pixels."""
    opaque = art[:, :, 3] > 32
    cols = np.nonzero(opaque.any(axis=0))[0]
    rows = np.nonzero(opaque.any(axis=1))[0]
    if not len(cols) or not len(rows):
        return 0, 0, art.shape[1] - 1, art.shape[0] - 1
    return cols.min(), rows.min(), cols.max(), rows.max()


def array_to_pixbuf(arr):
    h, w, _ = arr.shape
    data = GLib.Bytes.new(arr.tobytes())
    return GdkPixbuf.Pixbuf.new_from_bytes(
        data, GdkPixbuf.Colorspace.RGB, True, 8, w, h, w * 4
    )


def draw_marks(cr, cfg, cx, cy, radius, w, h):
    """Draw the hour indices and the finer minute track around the dial."""
    size = cfg["mark_scale"] / 100.0
    position = cfg["mark_position"] / 100.0
    minute_size = cfg["minute_mark_scale"] / 100.0
    if size <= 0 or position <= 0:
        return

    # The indices hang off a common outer edge and grow inwards.  The edge
    # is clamped to the canvas so a high position cannot slice them off.
    hour_len = (MARK_OUTER - MARK_INNER) * size * radius
    hour_width = MARK_WIDTH * size * radius
    limit = max(1.0, min(cx, cy, w - cx, h - cy) - hour_width / 2)
    outer = min(MARK_OUTER * position * radius, limit)

    # Quarter mode promotes only 12/3/6/9 to full indices; the hours it
    # drops fall back to the minute track rather than vanishing.
    step = 3 if cfg["quarter_marks_only"] else 1
    hours = {i * 5 for i in range(0, 12, step)}

    hour_rgb = rgb_tuple(cfg["hour_mark_color"])
    minute_rgb = rgb_tuple(cfg["minute_mark_color"])
    for minute in range(60):
        if minute in hours:
            length, width, color = hour_len, hour_width, hour_rgb
        elif minute_size > 0:
            length = hour_len * minute_size
            width = hour_width * minute_size
            color = minute_rgb
        else:
            continue
        inner = max(0.0, outer - length)
        angle = minute * math.pi / 30
        sin, cos = math.sin(angle), math.cos(angle)
        cr.set_source_rgba(*color)
        cr.set_line_width(max(1.0, width))
        cr.move_to(cx + inner * sin, cy - inner * cos)
        cr.line_to(cx + outer * sin, cy - outer * cos)
        cr.stroke()

def draw_hand(cr, cx, cy, angle, length, width, color):
    cr.set_source_rgba(*color)
    cr.set_line_width(max(1.0, width))
    cr.move_to(cx, cy)
    cr.line_to(cx + length * math.sin(angle), cy - length * math.cos(angle))
    cr.stroke()


def minute_color_of(cfg):
    return cfg["hour_color"] if cfg["minute_same_as_hour"] else cfg["minute_color"]


def draw_hands(cr, cfg, cx, cy, radius, w, h, when):
    """Draw the three hands and the centre pin for a (hour, minute, second)."""
    hours, minutes, seconds = when
    hand_scale = cfg["hand_scale"] / 100.0
    hour_color = rgb_tuple(cfg["hour_color"])

    # Hands longer than the canvas would be sliced off by the window edge, so
    # cap each one at the room actually available around the pivot.
    margin = HOUR_WIDTH * radius * hand_scale / 2
    reach = max(1.0, min(cx, cy, w - cx, h - cy) - margin)

    def length(fraction):
        return min(fraction * radius * hand_scale, reach)

    draw_hand(cr, cx, cy, (hours + minutes / 60) * math.pi / 6, length(HOUR_LEN),
              HOUR_WIDTH * radius * hand_scale, hour_color)
    draw_hand(cr, cx, cy, minutes * math.pi / 30, length(MINUTE_LEN),
              MINUTE_WIDTH * radius * hand_scale, rgb_tuple(minute_color_of(cfg)))
    draw_hand(cr, cx, cy, seconds * math.pi / 30, length(SECOND_LEN),
              SECOND_WIDTH * radius * hand_scale, rgb_tuple(cfg["second_color"]))

    cr.set_source_rgba(*hour_color)
    cr.arc(cx, cy, max(1.0, PIN_RADIUS * radius * hand_scale), 0, 2 * math.pi)
    cr.fill()



# Appearance presets shown as clickable thumbnails at the top of Settings.
# Each is a sparse overlay on DEFAULTS; clicking one snaps every appearance
# setting to it.  Window size and position are deliberately left alone.
PRESET_TIME = (10, 9, 30)   # a flattering time for the preview thumbnails
PRESET_THUMB = 58           # thumbnail size in px

PRESETS = (
    {
        "name": "Classic",
        "tip": "The built-in face with the default hands and colours",
        "values": {},
    },
    {
        "name": "Gradient",
        "tip": "The app icon's dial, with its own line colours",
        "values": {
            "face_svg": BUILTIN_FACE_PREFIX + "icon",
            "face_default": False,
            "face_transparent": False,
            "face_color": "#b0c4de",       # the icon's rim
            "wire_color": "#00008b",       # the dark end of its gradient
            "hour_color": "#f0f0f0",       # the icon's hands and ticks
            "minute_color": "#f0f0f0",
            "minute_same_as_hour": True,
            "second_color": "#ffd166",
            "hour_mark_color": "#f0f0f0",
            "minute_mark_color": "#f0f0f0",
        },
    },
    {
        "name": "Ember",
        "tip": "The gradient dial with its blues swapped to reds",
        "values": {
            "face_svg": BUILTIN_FACE_PREFIX + "icon",
            "face_default": False,
            "face_transparent": False,
            "face_color": "#dec4b0",       # #b0c4de with red and blue swapped
            "wire_color": "#8b0000",       # #00008b likewise
            "hour_color": "#f0f0f0",
            "minute_color": "#f0f0f0",
            "minute_same_as_hour": True,
            "second_color": "#66d1ff",
            "hour_mark_color": "#f0f0f0",
            "minute_mark_color": "#f0f0f0",
        },
    },
    {
        "name": "Emerald",
        "tip": "The gradient dial with its blues swapped to greens",
        "values": {
            "face_svg": BUILTIN_FACE_PREFIX + "icon",
            "face_default": False,
            "face_transparent": False,
            "face_color": "#b0dec4",       # #b0c4de with green and blue swapped
            "wire_color": "#006400",       # darkgreen; green reads brighter than
                                           # navy, so it needs to go darker to
                                           # match the other dials' depth
            "hour_color": "#f0f0f0",
            "minute_color": "#f0f0f0",
            "minute_same_as_hour": True,
            "second_color": "#d166ff",
            "hour_mark_color": "#f0f0f0",
            "minute_mark_color": "#f0f0f0",
        },
    },
)


def _make_labels_unselectable(widget):
    """Clear "selectable" on every label under a widget, caret and all."""
    if isinstance(widget, Gtk.Label):
        widget.set_selectable(False)
    if isinstance(widget, Gtk.Container):
        for child in widget.get_children():
            _make_labels_unselectable(child)


def preset_values(preset):
    """A complete settings dict for a preset (DEFAULTS plus its overrides)."""
    values = {k: v for k, v in DEFAULTS.items() if k in PRESET_KEYS}
    values.update(preset["values"])
    return values


def preset_thumbnail(values, size):
    """Render a preset the way the clock would draw it, as a GdkPixbuf."""
    path = None if values["face_default"] else (values["face_svg"] or None)
    art = open_face(path).render(size, size)
    art = recolor(art, values["wire_color"], values["face_color"],
                  values["face_transparent"])

    surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, size, size)
    cr = cairo.Context(surface)
    Gdk.cairo_set_source_pixbuf(cr, array_to_pixbuf(art), 0, 0)
    cr.paint()
    cr.set_line_cap(1)  # ROUND

    # Same radius rule the clock uses: the hands span the artwork's content.
    x0, y0, x1, y1 = content_bounds(art)
    cx, cy = size / 2.0, size / 2.0
    reach = min(cx - x0, x1 - cx, cy - y0, y1 - cy)
    if reach <= 0:
        reach = cx
    radius = max(1.0, reach * HAND_SPAN)

    draw_marks(cr, values, cx, cy, radius, size, size)
    draw_hands(cr, values, cx, cy, radius, size, size, PRESET_TIME)
    surface.flush()
    return Gdk.pixbuf_get_from_surface(surface, 0, 0, size, size)


class Clock(Gtk.Window):
    def __init__(self):
        super().__init__(type=Gtk.WindowType.TOPLEVEL)
        self.cfg = load_config()
        self.face = open_face(self.face_path())
        self.raster = None          # the recoloured, rasterised face
        self.bounds = (0, 0, 1, 1)  # content bbox of the current raster
        self.settings_dialog = None
        self.picking = False
        self.dragging_center = False
        self.center_before_pick = None
        self._save_pending = None
        self._rebuild_pending = None
        self.raster_stale = False

        self.set_title("vclock")
        icon = app_icon(64)
        if icon is not None:
            self.set_icon(icon)
        self.set_decorated(False)
        self.set_keep_above(self.cfg["always_on_top"])
        self.set_skip_taskbar_hint(True)
        self.set_skip_pager_hint(True)
        self.set_resizable(False)
        self.set_app_paintable(True)

        visual = self.get_screen().get_rgba_visual()
        if visual is None:
            print("WARNING: no RGBA visual / compositor; background will not be transparent")
        else:
            self.set_visual(visual)

        self.cfg["size"] = min(self.cfg["size"], self.max_size())
        self.apply_size()
        self.rebuild_pixbuf()

        self.add_events(Gdk.EventMask.BUTTON_PRESS_MASK
                        | Gdk.EventMask.BUTTON_RELEASE_MASK
                        | Gdk.EventMask.BUTTON1_MOTION_MASK)
        self.connect("draw", self.on_draw)
        self.connect("destroy", self.on_destroy)
        self.connect("button-press-event", self.on_button_press)
        self.connect("button-release-event", self.on_button_release)
        self.connect("motion-notify-event", self.on_motion_notify)
        self.connect("key-press-event", self.on_key_press)
        self.connect("configure-event", self.on_configure)

        self.menu = Gtk.Menu()
        settings_item = Gtk.MenuItem(label="Settings")
        settings_item.connect("activate", lambda _w: self.open_settings())
        self.menu.append(settings_item)
        self.on_top_item = Gtk.CheckMenuItem(label="Always on top")
        self.on_top_item.set_active(self.cfg["always_on_top"])
        self.on_top_item.connect("toggled", self.on_always_on_top)
        self.menu.append(self.on_top_item)
        reset_item = Gtk.MenuItem(label="Reset defaults")
        reset_item.connect("activate", lambda _w: self.confirm_reset())
        self.menu.append(reset_item)
        help_item = Gtk.MenuItem(label="Help")
        help_item.connect("activate", lambda _w: self.show_help())
        self.menu.append(help_item)
        about_item = Gtk.MenuItem(label="About")
        about_item.connect("activate", lambda _w: self.show_about())
        self.menu.append(about_item)
        self.menu.append(Gtk.SeparatorMenuItem())
        quit_item = Gtk.MenuItem(label="Quit")
        quit_item.connect("activate", lambda _w: self.destroy())
        self.menu.append(quit_item)
        self.menu.show_all()

        GLib.timeout_add(1000, self.tick)

    # ---------------------------------------------------------------- config

    def face_path(self):
        """The face file to load, or None for the embedded default."""
        if self.cfg["face_default"]:
            return None
        return self.cfg["face_svg"] or None

    def max_size(self):
        return monitor_max_size(self)

    def pixel_size(self):
        w = max(1, int(round(self.cfg["size"])))
        h = max(1, int(round(w * self.face.aspect)))
        return w, h

    def apply_size(self):
        w, h = self.pixel_size()
        self.set_size_request(w, h)
        self.resize(w, h)

    def rebuild_pixbuf(self):
        """Re-rasterise the face at the current widget size and apply colours."""
        w, h = self.pixel_size()
        art = self.face.render(w, h)
        x0, y0, x1, y1 = content_bounds(art)
        # Stored as fractions so the hands stay correct even while the raster is
        # briefly stale during a resize drag.
        self.bounds = (x0 / w, y0 / h, x1 / w, y1 / h)
        self.raster = array_to_pixbuf(
            recolor(
                art,
                self.cfg["wire_color"],
                self.cfg["face_color"],
                self.cfg["face_transparent"],
            )
        )
        self.raster_stale = False

    def center_fraction(self):
        """Where the hands pivot, as fractions of the canvas (auto = its centre)."""
        if self.cfg["center"] is not None:
            return tuple(self.cfg["center"])
        return (0.5, 0.5)

    def center_pixels(self):
        w, h = self.pixel_size()
        fx, fy = self.center_fraction()
        return fx * w, fy * h

    def hand_radius(self):
        """Largest radius that keeps the hands inside the artwork's content."""
        cx, cy = self.center_pixels()
        w, h = self.pixel_size()
        fx0, fy0, fx1, fy1 = self.bounds
        x0, y0, x1, y1 = fx0 * w, fy0 * h, fx1 * w, fy1 * h
        reach = min(cx - x0, x1 - cx, cy - y0, y1 - cy)
        if reach <= 0:  # centre sits outside the artwork; use what room there is
            reach = min(cx, w - cx, cy, h - cy)
        return max(1.0, reach * HAND_SPAN)

    def minute_color(self):
        return minute_color_of(self.cfg)

    def schedule_rebuild(self):
        """Re-raster after the user pauses, coalescing a burst of slider events.

        Dragging the size slider fires value-changed on every pointer motion; a
        full re-raster each time cannot keep up at large sizes, so until things
        settle we scale the existing raster instead (see on_draw).
        """
        self.raster_stale = True
        if self._rebuild_pending is not None:
            GLib.source_remove(self._rebuild_pending)
        self._rebuild_pending = GLib.timeout_add(60, self._do_rebuild)

    def _do_rebuild(self):
        self._rebuild_pending = None
        self.rebuild_pixbuf()
        self.queue_draw()
        return False

    def queue_save(self):
        if self._save_pending is not None:
            GLib.source_remove(self._save_pending)
        self._save_pending = GLib.timeout_add(500, self._do_save)

    def _do_save(self):
        self._save_pending = None
        save_config(self.cfg)
        return False

    # ---------------------------------------------------------------- events

    def tick(self):
        self.queue_draw()
        return True

    def on_configure(self, _widget, _event):
        x, y = self.get_position()
        if (x, y) != (self.cfg["x"], self.cfg["y"]):
            self.cfg["x"], self.cfg["y"] = x, y
            self.queue_save()
        return False

    def on_destroy(self, _widget):
        if self._rebuild_pending is not None:
            GLib.source_remove(self._rebuild_pending)
            self._rebuild_pending = None
        if self._save_pending is not None:
            GLib.source_remove(self._save_pending)
            self._save_pending = None
        save_config(self.cfg)
        Gtk.main_quit()

    def on_button_press(self, _widget, event):
        if self.picking and event.button == 1:
            # Click and drag: preview follows the pointer, nothing is committed
            # until the button comes back up.
            self.dragging_center = True
            self.preview_center(event.x, event.y)
            return True
        if event.button == 1:
            self.begin_move_drag(event.button, int(event.x_root), int(event.y_root), event.time)
        elif event.button == 3:
            self.menu.popup_at_pointer(event)
        return True

    def on_motion_notify(self, _widget, event):
        if self.dragging_center:
            self.preview_center(event.x, event.y)
        return True

    def on_button_release(self, _widget, event):
        if self.dragging_center and event.button == 1:
            self.dragging_center = False
            self.preview_center(event.x, event.y)
            self.center_before_pick = None
            self.queue_save()
            self.stop_picking()
            return True
        return False

    def on_key_press(self, _widget, event):
        name = Gdk.keyval_name(event.keyval) or ""
        state = event.state
        alt = state & Gdk.ModifierType.MOD1_MASK
        # The "command" modifier: Ctrl everywhere, plus Cmd on macOS, where GDK
        # reports the Command key as META. Meta is deliberately not accepted on
        # X11/Windows, because window managers there frequently alias it onto
        # Alt, which would fire these actions by accident.
        command = bool(state & Gdk.ModifierType.CONTROL_MASK)
        if IS_MAC:
            command = command or bool(state & Gdk.ModifierType.META_MASK)

        if name == "Escape":
            if self.picking:
                self.cancel_picking()
            else:
                self.destroy()
        elif alt and name == "F4" and not IS_MAC:
            self.destroy()
        elif command:
            actions = {
                "c": self.destroy,
                "s": self.open_settings,
                "h": self.show_help,
                "a": self.show_about,
                "r": self.confirm_reset,
            }
            if IS_MAC:
                actions["q"] = self.destroy
                actions["w"] = self.destroy
            action = actions.get(name.lower())
            if action is not None:
                action()
        return True

    # ------------------------------------------------------------ centre pick

    def start_picking(self):
        """Let the user drag on the face to place the hands' pivot."""
        self.picking = True
        self.dragging_center = False
        self.center_before_pick = self.cfg["center"]
        gdk_window = self.get_window()
        if gdk_window is not None:
            gdk_window.set_cursor(
                Gdk.Cursor.new_from_name(self.get_display(), "crosshair")
            )
        self.queue_draw()

    def stop_picking(self):
        self.picking = False
        self.dragging_center = False
        gdk_window = self.get_window()
        if gdk_window is not None:
            gdk_window.set_cursor(None)
        self.queue_draw()

    def cancel_picking(self):
        """Abandon a pick, putting the pivot back where it started."""
        if self.center_before_pick is not None or self.picking:
            self.set_center(self.center_before_pick, save=False)
        self.center_before_pick = None
        self.stop_picking()

    def preview_center(self, x, y):
        """Move the pivot to a pointer position, without writing the config."""
        w, h = self.pixel_size()
        fx = min(1.0, max(0.0, x / w)) if w else 0.5
        fy = min(1.0, max(0.0, y / h)) if h else 0.5
        self.set_center([fx, fy], save=False)

    def set_center(self, center, save=True):
        self.cfg["center"] = sanitize_center(center)
        self.queue_draw()
        if save:
            self.queue_save()
        if self.settings_dialog is not None:
            self.settings_dialog.refresh_center()

    # ---------------------------------------------------------------- drawing

    def on_draw(self, _widget, cr):
        cr.set_operator(1)  # SOURCE: clear to fully transparent pixels
        cr.set_source_rgba(0, 0, 0, 0)
        cr.paint()
        cr.set_operator(2)  # OVER

        # The face is rasterised at the widget's pixel size, so paint it 1:1 and
        # draw the hands in the same pixel coordinates.  Mid-resize the raster
        # may still be the previous size, so scale it to fit until it catches up.
        w, h_canvas = self.get_size()
        rw, rh = self.raster.get_width(), self.raster.get_height()
        if (rw, rh) != (w, h_canvas):
            cr.save()
            cr.scale(w / rw, h_canvas / rh)
            Gdk.cairo_set_source_pixbuf(cr, self.raster, 0, 0)
            cr.paint()
            cr.restore()
        else:
            Gdk.cairo_set_source_pixbuf(cr, self.raster, 0, 0)
            cr.paint()

        cx, cy = self.center_pixels()
        radius = self.hand_radius()
        cr.set_line_cap(1)  # ROUND

        draw_marks(cr, self.cfg, cx, cy, radius, w, h_canvas)

        t = time.localtime()
        when = (t.tm_hour % 12, t.tm_min, t.tm_sec)
        draw_hands(cr, self.cfg, cx, cy, radius, w, h_canvas, when)

        if self.picking:
            self.draw_pick_hint(cr, cx, cy, radius)

    def draw_pick_hint(self, cr, cx, cy, radius):
        """Crosshair over the current centre while the user is picking."""
        cr.set_source_rgba(0.1, 0.6, 1.0, 0.9)
        cr.set_line_width(max(1.0, 0.008 * radius))
        span = radius * 0.25
        cr.move_to(cx - span, cy)
        cr.line_to(cx + span, cy)
        cr.move_to(cx, cy - span)
        cr.line_to(cx, cy + span)
        cr.stroke()
        cr.arc(cx, cy, span * 0.45, 0, 2 * math.pi)
        cr.stroke()

    # --------------------------------------------------------------- settings

    def open_settings(self):
        if self.settings_dialog is not None:
            self.settings_dialog.present()
            return

        # Snapshot so Cancel can restore the exact previous look.
        snapshot = dict(self.cfg)
        dlg = SettingsDialog(self)
        self.settings_dialog = dlg

        def on_response(dialog, response):
            self.stop_picking()
            if response == Gtk.ResponseType.OK:
                self.apply_settings(dialog.values())
                save_config(self.cfg)
            else:
                self.apply_settings(snapshot)
                self.queue_save()
            self.settings_dialog = None
            dialog.destroy()

        dlg.connect("response", on_response)
        dlg.connect("delete-event", lambda d, e: (d.emit("response", Gtk.ResponseType.CANCEL), True)[1])
        dlg.show_all()
        self.place_dialog(dlg)

    def place_dialog(self, dlg):
        """Put the dialog beside the clock so it never covers it."""
        cx, cy = self.get_position()
        cw, ch = self.get_size()
        dw, dh = dlg.get_size()
        screen = self.get_screen()
        mon = screen.get_display().get_monitor_at_window(self.get_window())
        area = mon.get_workarea()

        x = cx + cw + 12
        if x + dw > area.x + area.width:
            x = cx - dw - 12          # try the left side
        if x < area.x:
            x = min(max(cx + cw + 12, area.x), area.x + area.width - dw)
        y = min(max(cy, area.y), area.y + max(0, area.height - dh))
        dlg.move(x, y)

    def on_always_on_top(self, item):
        self.cfg["always_on_top"] = item.get_active()
        self.set_keep_above(self.cfg["always_on_top"])
        self.queue_save()

    def sync_always_on_top(self):
        """Keep the check mark in step when the setting changes elsewhere."""
        self.set_keep_above(self.cfg["always_on_top"])
        if self.on_top_item.get_active() != self.cfg["always_on_top"]:
            self.on_top_item.handler_block_by_func(self.on_always_on_top)
            self.on_top_item.set_active(self.cfg["always_on_top"])
            self.on_top_item.handler_unblock_by_func(self.on_always_on_top)

    def close_settings(self):
        """Drop the settings dialog without running its Cancel restore."""
        dlg, self.settings_dialog = self.settings_dialog, None
        if dlg is not None:
            self.stop_picking()
            dlg.destroy()

    def confirm_reset(self):
        dlg = Gtk.MessageDialog(
            transient_for=self, modal=True,
            message_type=Gtk.MessageType.QUESTION,
            buttons=Gtk.ButtonsType.OK_CANCEL,
            text="Are you sure?",
        )
        dlg.format_secondary_text(
            "This restores every clock setting to its default, including the "
            "built-in clock face."
        )
        response = dlg.run()
        dlg.destroy()
        if response == Gtk.ResponseType.OK:
            self.reset_defaults()

    def reset_defaults(self):
        """Restore every setting except the on-screen position."""
        self.close_settings()
        values = {k: v for k, v in DEFAULTS.items() if k not in ("x", "y")}
        self.apply_settings(values)
        save_config(self.cfg)

    def show_help(self):
        dlg = Gtk.MessageDialog(
            transient_for=self, modal=True,
            message_type=Gtk.MessageType.INFO,
            buttons=Gtk.ButtonsType.CLOSE,
            text="vclock help",
        )
        dlg.format_secondary_markup(
            "<b>Mouse</b>\n"
            "Left drag — move the clock\n"
            "Right click — menu\n"
            "\n<b>Keyboard</b>\n"
            f"{CMD_LABEL}+S — settings\n"
            f"{CMD_LABEL}+H — this help\n"
            f"{CMD_LABEL}+A — about\n"
            f"{CMD_LABEL}+R — reset defaults\n"
            f"{QUIT_KEYS} — quit\n"
            "\n<b>Clock face</b>\n"
            "Any SVG can be used. Within the artwork white is treated as the "
            "face color and black as the wire color, and both can be recolored "
            "from Settings.\n"
            "\n<b>Hand center</b>\n"
            "Settings ▸ Pick on clock, then drag on the face. The hands and "
            "marks follow the pointer and settle where you release the button. "
            "Esc cancels."
        )
        dlg.connect("response", lambda d, _r: d.destroy())
        dlg.show_all()

    def show_about(self):
        dlg = Gtk.AboutDialog(transient_for=self, modal=True)
        dlg.set_program_name("vclock")
        logo = app_icon(96)
        if logo is not None:
            dlg.set_logo(logo)
        dlg.set_comments(ABOUT_TEXT)
        dlg.set_copyright("Written by Wade Ryan\nSeptember, 2026")
        dlg.connect("response", lambda d, _r: d.destroy())

        ok = dlg.add_button("OK", Gtk.ResponseType.OK)
        ok.set_can_default(True)
        dlg.set_default_response(Gtk.ResponseType.OK)
        dlg.show_all()
        # AboutDialog builds its labels selectable, so the first one takes focus
        # and parks a text caret beside the program name.  Give focus to the
        # button and take the selectability away so it cannot come back.
        _make_labels_unselectable(dlg)
        ok.grab_focus()

    def apply_settings(self, values):
        """Apply a settings dict to the live widget (used for preview too)."""
        face_keys = ("face_svg", "face_default")
        color_keys = ("wire_color", "face_color", "face_transparent")
        new_face = any(values.get(k, self.cfg[k]) != self.cfg[k] for k in face_keys)
        changed_color = any(values.get(k, self.cfg[k]) != self.cfg[k] for k in color_keys)
        changed_size = values.get("size", self.cfg["size"]) != self.cfg["size"]

        self.cfg.update(values)
        if "always_on_top" in values:
            self.sync_always_on_top()
        if new_face:
            self.face = open_face(self.face_path())
        if new_face or changed_color:
            if self._rebuild_pending is not None:
                GLib.source_remove(self._rebuild_pending)
                self._rebuild_pending = None
            self.apply_size()
            self.rebuild_pixbuf()
        elif changed_size:
            # Resizing alone is the hot path while dragging the size slider:
            # move the window now and re-raster once the drag settles.
            self.apply_size()
            self.schedule_rebuild()
        self.queue_draw()


class ColorButton(Gtk.Button):
    """A colour swatch button that previews changes live.

    Gtk.ColorButton only emits "color-set" once its internal dialog is accepted,
    and GTK3 offers no hook into that dialog, so the clock could not follow the
    colour while it was being chosen.  This drives its own Gtk.ColorChooserWidget
    instead and re-emits "color-set" on every "notify::rgba", giving a live
    preview; Cancel restores the colour that was in effect when the dialog
    opened.  The API mirrors Gtk.ColorButton closely enough to be a drop-in.
    """

    __gsignals__ = {"color-set": (GObject.SignalFlags.RUN_FIRST, None, ())}

    SWATCH_W = 48
    SWATCH_H = 22

    def __init__(self, rgba=None, use_alpha=False, title="Choose a color"):
        super().__init__()
        self._rgba = rgba if rgba is not None else rgba_of("#000000")
        self._use_alpha = use_alpha
        self._title = title
        self._dialog = None

        area = Gtk.DrawingArea()
        area.set_size_request(self.SWATCH_W, self.SWATCH_H)
        area.connect("draw", self._draw_swatch)
        self._area = area
        self.add(area)
        self.connect("clicked", self._on_clicked)

    # -------------------------------------------------------------- Gtk API

    def get_rgba(self):
        return self._rgba.copy()

    def set_rgba(self, rgba):
        """Set the colour without emitting "color-set" (matches Gtk.ColorButton,
        and keeps sync_swatches() from recursing)."""
        self._rgba = rgba.copy()
        self._area.queue_draw()

    def get_use_alpha(self):
        return self._use_alpha

    def set_use_alpha(self, value):
        self._use_alpha = bool(value)
        self._area.queue_draw()

    # --------------------------------------------------------------- render

    def _draw_swatch(self, widget, cr):
        w = widget.get_allocated_width()
        h = widget.get_allocated_height()
        alpha = self._rgba.alpha if self._use_alpha else 1.0

        if alpha < 1.0:
            # Standard light/dark checkerboard behind partially clear colours.
            step = 6
            for iy in range(0, h, step):
                for ix in range(0, w, step):
                    shade = 0.6 if ((ix // step) + (iy // step)) % 2 else 1.0
                    cr.set_source_rgb(shade, shade, shade)
                    cr.rectangle(ix, iy, step, step)
                    cr.fill()

        cr.set_source_rgba(self._rgba.red, self._rgba.green, self._rgba.blue, alpha)
        cr.rectangle(0, 0, w, h)
        cr.fill()

        cr.set_source_rgba(0, 0, 0, 0.45)
        cr.set_line_width(1)
        cr.rectangle(0.5, 0.5, w - 1, h - 1)
        cr.stroke()
        return True

    # --------------------------------------------------------------- picker

    def _on_clicked(self, _button):
        if self._dialog is not None:      # already open; just raise it
            self._dialog.present()
            return

        before = self._rgba.copy()
        dlg = Gtk.Dialog(title=self._title,
                         transient_for=self.get_toplevel(), modal=True)
        dlg.add_button("Cancel", Gtk.ResponseType.CANCEL)
        dlg.add_button("Select", Gtk.ResponseType.OK)
        dlg.set_default_response(Gtk.ResponseType.OK)

        chooser = Gtk.ColorChooserWidget()
        Gtk.ColorChooser.set_use_alpha(chooser, self._use_alpha)
        Gtk.ColorChooser.set_rgba(chooser, before)
        chooser.set_margin_top(6)
        chooser.set_margin_bottom(6)
        chooser.set_margin_start(6)
        chooser.set_margin_end(6)
        dlg.get_content_area().pack_start(chooser, True, True, 0)

        def on_notify(widget, _param):
            self._rgba = Gtk.ColorChooser.get_rgba(widget)
            self._area.queue_draw()
            self.emit("color-set")

        chooser.connect("notify::rgba", on_notify)

        def on_response(d, response):
            # Gdk.RGBA compares by identity in PyGObject, so compare by value.
            if response != Gtk.ResponseType.OK and not self._rgba.equal(before):
                self._rgba = before          # Cancel: put the old colour back
                self._area.queue_draw()
                self.emit("color-set")
            self._dialog = None
            d.destroy()

        dlg.connect("response", on_response)
        self._dialog = dlg
        dlg.show_all()


class SettingsDialog(Gtk.Dialog):
    def __init__(self, parent):
        # Not modal, so the clock stays interactive and the dialog can be moved
        # around freely while previewing changes.
        super().__init__(title="Clock Settings", transient_for=parent, modal=False)
        self.clock = parent
        self.live = False
        cfg = parent.cfg
        self.add_button("Cancel", Gtk.ResponseType.CANCEL)
        self.add_button("Save", Gtk.ResponseType.OK)
        self.set_default_response(Gtk.ResponseType.OK)

        grid = Gtk.Grid(row_spacing=8, column_spacing=10, margin=12)
        self.get_content_area().add(grid)
        row = 0

        self.face_override = None

        grid.attach(self._label("Presets"), 0, row, 1, 1)
        preset_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=6)
        for preset in PRESETS:
            btn = Gtk.Button()
            thumb = preset_thumbnail(preset_values(preset), PRESET_THUMB)
            btn.set_image(Gtk.Image.new_from_pixbuf(thumb))
            btn.set_always_show_image(True)
            btn.set_tooltip_text(f"{preset['name']} \u2014 {preset['tip']}")
            btn.connect("clicked", self.on_preset_clicked, preset)
            preset_box.pack_start(btn, False, False, 0)
        grid.attach(preset_box, 1, row, 2, 1)
        row += 1

        grid.attach(self._label("Clock face svg"), 0, row, 1, 1)
        self.face_chooser = Gtk.FileChooserButton(
            title="Choose a clock face SVG", action=Gtk.FileChooserAction.OPEN
        )
        svg_filter = Gtk.FileFilter()
        svg_filter.set_name("SVG images")
        svg_filter.add_pattern("*.svg")
        self.face_chooser.add_filter(svg_filter)
        self.face_chooser.set_current_folder(FACE_DIR)
        if cfg["face_svg"] and os.path.exists(cfg["face_svg"]):
            self.face_chooser.set_filename(cfg["face_svg"])
        self.face_chooser.set_hexpand(True)
        grid.attach(self.face_chooser, 1, row, 1, 1)
        self.face_is_default = Gtk.CheckButton(label="default")
        self.face_is_default.set_active(cfg["face_default"])
        grid.attach(self.face_is_default, 2, row, 1, 1)
        row += 1

        grid.attach(self._label("Clock size (px)"), 0, row, 1, 1)
        max_size = parent.max_size()
        self.size = Gtk.Scale.new_with_range(
            Gtk.Orientation.HORIZONTAL, SIZE_MIN, max_size, 1
        )
        # Marks make the available range obvious next to the value readout.
        self.size.add_mark(SIZE_MIN, Gtk.PositionType.BOTTOM, str(SIZE_MIN))
        self.size.add_mark(max_size, Gtk.PositionType.BOTTOM, str(max_size))
        self.size.set_value(cfg["size"])
        self.size.set_hexpand(True)
        self.size.set_size_request(240, -1)
        self.size.set_draw_value(False)  # shown in our own label instead

        # Own label to the left of the slider, reserved wide enough for the
        # largest possible value so the slider never shifts as digits change.
        self.size_value = Gtk.Label()
        self.size_value.set_width_chars(len(str(int(max_size))))
        self.size_value.set_max_width_chars(len(str(int(max_size))))
        self.size_value.set_xalign(1.0)
        self.size.connect("value-changed", self.on_size_value)
        self.on_size_value(self.size)

        size_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        size_box.pack_start(self.size_value, False, False, 0)
        size_box.pack_start(self.size, True, True, 0)
        size_box.set_hexpand(True)
        grid.attach(size_box, 1, row, 2, 1)
        row += 1

        self.hand_scale = self._percent_slider(
            grid, row, "Hand size (%)", cfg["hand_scale"],
            HAND_SCALE_MIN, HAND_SCALE_MAX)
        row += 1

        self.mark_scale = self._percent_slider(
            grid, row, "Hour mark size (%)", cfg["mark_scale"],
            MARK_SCALE_MIN, MARK_SCALE_MAX)
        row += 1

        self.mark_position = self._percent_slider(
            grid, row, "Hour mark position (%)", cfg["mark_position"],
            MARK_SCALE_MIN, MARK_SCALE_MAX)
        row += 1

        self.minute_mark_scale = self._percent_slider(
            grid, row, "Minute mark size (%)", cfg["minute_mark_scale"],
            MARK_SCALE_MIN, MARK_SCALE_MAX)
        self.minute_mark_scale.set_tooltip_text(
            "Percentage of the hour mark size. 0 hides the minute marks.")
        row += 1

        self.quarter_marks = Gtk.CheckButton(label="quarter marks only")
        self.quarter_marks.set_active(cfg["quarter_marks_only"])
        self.quarter_marks.set_tooltip_text(
            "Draw full indices at 12, 3, 6 and 9 only; the other hours drop to "
            "the minute track.")
        grid.attach(self.quarter_marks, 1, row, 2, 1)
        row += 1

        grid.attach(self._label("Second hand color"), 0, row, 1, 1)
        self.second = ColorButton(rgba_of(cfg["second_color"]),
                                  title="Second hand color")
        grid.attach(self.second, 1, row, 1, 1)
        row += 1

        grid.attach(self._label("Hour hand color"), 0, row, 1, 1)
        self.hour = ColorButton(rgba_of(cfg["hour_color"]),
                                title="Hour hand color")
        grid.attach(self.hour, 1, row, 1, 1)
        row += 1

        grid.attach(self._label("Minute hand color"), 0, row, 1, 1)
        self.minute = ColorButton(rgba_of(cfg["minute_color"]),
                                  title="Minute hand color")
        grid.attach(self.minute, 1, row, 1, 1)
        self.minute_same = Gtk.CheckButton(label="same as hour")
        self.minute_same.set_active(cfg["minute_same_as_hour"])
        grid.attach(self.minute_same, 2, row, 1, 1)
        # The user's own minute colour is remembered while "same as hour" is on,
        # so unticking the box brings it back.
        self.minute_own = cfg["minute_color"]
        row += 1

        grid.attach(self._label("Face color"), 0, row, 1, 1)
        # use_alpha lets the swatch show the checkerboard when transparent.
        self.face = ColorButton(rgba_of(cfg["face_color"]), use_alpha=True,
                                title="Face color")
        grid.attach(self.face, 1, row, 1, 1)
        self.face_transparent = Gtk.CheckButton(label="transparent")
        self.face_transparent.set_active(cfg["face_transparent"])
        grid.attach(self.face_transparent, 2, row, 1, 1)
        self.face_own = cfg["face_color"]
        row += 1

        grid.attach(self._label("Wire color"), 0, row, 1, 1)
        self.wire = ColorButton(rgba_of(cfg["wire_color"]),
                                title="Wire color")
        grid.attach(self.wire, 1, row, 1, 1)
        row += 1

        grid.attach(self._label("Hour mark color"), 0, row, 1, 1)
        self.hour_mark = ColorButton(rgba_of(cfg["hour_mark_color"]),
                                     title="Hour mark color")
        grid.attach(self.hour_mark, 1, row, 1, 1)
        row += 1

        grid.attach(self._label("Minute mark color"), 0, row, 1, 1)
        self.minute_mark = ColorButton(rgba_of(cfg["minute_mark_color"]),
                                       title="Minute mark color")
        grid.attach(self.minute_mark, 1, row, 1, 1)
        row += 1

        grid.attach(self._label("Hand center"), 0, row, 1, 1)
        self.pick = Gtk.Button(label="Pick on clock…")
        self.pick.connect("clicked", self.on_pick)
        grid.attach(self.pick, 1, row, 1, 1)
        self.center_auto = Gtk.CheckButton(label="auto (canvas center)")
        self.center_auto.set_active(cfg["center"] is None)
        self.center_auto.connect("toggled", self.on_center_auto)
        grid.attach(self.center_auto, 2, row, 1, 1)
        row += 1

        self.center_label = self._label("")
        self.center_label.set_margin_start(4)
        grid.attach(self.center_label, 1, row, 2, 1)

        self.size.connect("value-changed", self.on_changed)
        for scale in (self.hand_scale, self.mark_scale,
                      self.mark_position, self.minute_mark_scale):
            scale.connect("value-changed", self.on_changed)
        for btn in (self.second, self.hour, self.minute, self.face, self.wire,
                    self.hour_mark, self.minute_mark):
            btn.connect("color-set", self.on_changed)
        for chk in (self.minute_same, self.face_transparent,
                    self.face_is_default, self.quarter_marks):
            chk.connect("toggled", self.on_changed)
        self.face_chooser.connect("file-set", self.on_face_chosen)

        self.sync_swatches()
        self.refresh_center()
        self.show_all()
        self.live = True

    @staticmethod
    def _label(text):
        lbl = Gtk.Label(label=text)
        lbl.set_xalign(0.0)
        return lbl

    def _percent_slider(self, grid, row, caption, value, low, high):
        """A slider laid out like the size row: value label left, fixed width."""
        grid.attach(self._label(caption), 0, row, 1, 1)
        scale = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, low, high, 1)
        scale.add_mark(low, Gtk.PositionType.BOTTOM, str(low))
        scale.add_mark(100, Gtk.PositionType.BOTTOM, "100")
        scale.add_mark(high, Gtk.PositionType.BOTTOM, str(high))
        scale.set_value(value)
        scale.set_hexpand(True)
        scale.set_size_request(240, -1)
        scale.set_draw_value(False)

        readout = Gtk.Label()
        width = len(str(int(high)))
        readout.set_width_chars(width)
        readout.set_max_width_chars(width)
        readout.set_xalign(1.0)
        scale.connect(
            "value-changed",
            lambda sc: readout.set_text(str(int(round(sc.get_value())))))
        readout.set_text(str(int(round(scale.get_value()))))

        box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        box.pack_start(readout, False, False, 0)
        box.pack_start(scale, True, True, 0)
        box.set_hexpand(True)
        grid.attach(box, 1, row, 2, 1)
        return scale

    def on_size_value(self, scale):
        self.size_value.set_text(str(int(round(scale.get_value()))))

    def on_pick(self, _button):
        self.clock.start_picking()

    def on_center_auto(self, check):
        if check.get_active():
            self.clock.stop_picking()
            self.clock.set_center(None)
        self.refresh_center()

    def refresh_center(self):
        """Show the pivot in canvas pixels, and keep the auto box in sync."""
        auto = self.clock.cfg["center"] is None
        if self.center_auto.get_active() != auto:
            self.center_auto.handler_block_by_func(self.on_center_auto)
            self.center_auto.set_active(auto)
            self.center_auto.handler_unblock_by_func(self.on_center_auto)
        self.pick.set_sensitive(True)
        cx, cy = self.clock.center_pixels()
        kind = "auto" if auto else "manual"
        self.center_label.set_markup(
            f"<small>face: {GLib.markup_escape_text(self.clock.face.label)}  |  "
            f"center {kind}: {cx:.0f}, {cy:.0f} px "
            f"(radius {self.clock.hand_radius():.0f})</small>"
        )

    def on_preset_clicked(self, _button, preset):
        self.apply_preset(preset_values(preset))

    def apply_preset(self, values):
        """Snap every appearance control to a preset, then preview it once."""
        was_live = self.live
        self.live = False       # move the widgets without a preview per widget
        try:
            self.face_override = values["face_svg"]
            # Presets only ever use the built-in faces, so whatever file the
            # chooser was holding must not win over the preset's own face.
            self.face_chooser.unselect_all()
            self.face_is_default.set_active(values["face_default"])

            for scale, key in ((self.hand_scale, "hand_scale"),
                               (self.mark_scale, "mark_scale"),
                               (self.mark_position, "mark_position"),
                               (self.minute_mark_scale, "minute_mark_scale")):
                scale.set_value(values[key])
            self.quarter_marks.set_active(values["quarter_marks_only"])
            self.minute_same.set_active(values["minute_same_as_hour"])
            self.face_transparent.set_active(values["face_transparent"])

            self.face_own = values["face_color"]
            self.minute_own = values["minute_color"]
            for button, key in ((self.second, "second_color"),
                                (self.hour, "hour_color"),
                                (self.wire, "wire_color"),
                                (self.hour_mark, "hour_mark_color"),
                                (self.minute_mark, "minute_mark_color")):
                button.set_rgba(rgba_of(values[key]))
        finally:
            self.live = was_live
        self.on_changed(None)

    def on_face_chosen(self, widget):
        self.face_override = None   # an explicit file beats a preset's face
        self.on_changed(widget)

    def on_changed(self, widget):
        if widget is self.minute and not self.minute_same.get_active():
            self.minute_own = hex_of(self.minute.get_rgba())
        if widget is self.face and not self.face_transparent.get_active():
            self.face_own = hex_of(self.face.get_rgba())
        self.sync_swatches()
        if self.live:
            self.clock.apply_settings(self.values())
            self.refresh_center()

    def sync_swatches(self):
        """Mirror the hour colour into the minute swatch and show the alpha
        checkerboard on the face swatch, matching the checkbox states.

        Setting a colour programmatically does not emit "color-set", so this
        cannot recurse.
        """
        same = self.minute_same.get_active()
        self.minute.set_sensitive(not same)
        self.minute.set_rgba(rgba_of(self.hour_color() if same else self.minute_own))

        clear = self.face_transparent.get_active()
        self.face.set_sensitive(not clear)
        color = rgba_of(self.face_own)
        color.alpha = 0.0 if clear else 1.0
        self.face.set_rgba(color)

        self.face_chooser.set_sensitive(not self.face_is_default.get_active())
        self.face_is_default.set_tooltip_text(
            f"Use the {DEFAULT_FACE_LABEL} clock face"
        )

    def hour_color(self):
        return hex_of(self.hour.get_rgba())

    def face_svg(self):
        chosen = self.face_chooser.get_filename()
        if chosen:
            return chosen
        if self.face_override is not None:
            return self.face_override
        return self.clock.cfg["face_svg"]

    def face_default(self):
        """Fall back to the built-in face when no file has been chosen."""
        return self.face_is_default.get_active() or not self.face_svg()

    def values(self):
        return {
            "size": int(round(self.size.get_value())),
            "hand_scale": int(round(self.hand_scale.get_value())),
            "mark_scale": int(round(self.mark_scale.get_value())),
            "mark_position": int(round(self.mark_position.get_value())),
            "minute_mark_scale": int(round(self.minute_mark_scale.get_value())),
            "quarter_marks_only": self.quarter_marks.get_active(),
            "face_svg": self.face_svg(),
            "face_default": self.face_default(),
            "second_color": hex_of(self.second.get_rgba()),
            "hour_color": hex_of(self.hour.get_rgba()),
            "minute_color": self.minute_own,
            "minute_same_as_hour": self.minute_same.get_active(),
            "face_color": self.face_own,
            "face_transparent": self.face_transparent.get_active(),
            "wire_color": hex_of(self.wire.get_rgba()),
            "hour_mark_color": hex_of(self.hour_mark.get_rgba()),
            "minute_mark_color": hex_of(self.minute_mark.get_rgba()),
        }


if __name__ == "__main__":
    quiet_stderr()
    win = Clock()
    win.show_all()
    if win.cfg["x"] is not None and win.cfg["y"] is not None:
        win.move(win.cfg["x"], win.cfg["y"])
    # Ctrl+C in the launching terminal shuts down the same way the menu does,
    # so the config still gets flushed.  glib-unix does not exist on Windows,
    # where Python's own KeyboardInterrupt handling applies instead.
    if hasattr(GLib, "unix_signal_add"):
        GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, signal.SIGINT,
                             lambda *_a: (win.destroy(), GLib.SOURCE_REMOVE)[1])
    Gtk.main()
