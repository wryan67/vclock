#!/bin/bash
#
# make-icons.sh -- rebuild the Windows and macOS icon files from vclock.svg.
#
#     distro/make-icons.sh
#
# Everywhere else the program draws its own icon at runtime, so there is
# nothing to keep in step.  Windows and macOS are the exceptions: both want an
# icon *in the file* rather than one the program produces once it is running,
# because both show it before the program has started -- in Explorer, in the
# Start menu, in Finder and in the Dock.  Neither format can be produced from
# the SVG at build time on the machine that usually builds them, so the two
# files are committed and this script is how they are refreshed.
#
# Run it when vclock.svg changes.  It needs, on a Linux box:
#
#     rsvg-convert   librsvg2-bin      the SVG rasteriser
#     convert        imagemagick       assembles the .ico
#     png2icns       icnsutils         assembles the .icns
#
# On a Mac, iconutil replaces png2icns and is already installed; see
# distro/macos/package.sh, which prefers it when it is available.

set -euo pipefail

HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ROOT=$(cd -- "$HERE/.." && pwd)
SVG=$ROOT/vclock.svg

missing=
for tool in rsvg-convert convert png2icns; do
    command -v "$tool" >/dev/null 2>&1 || missing="$missing $tool"
done
[ -z "$missing" ] || {
    echo "error: not installed here:$missing" >&2
    echo "       apt install librsvg2-bin imagemagick icnsutils" >&2
    exit 1
}

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# 16 through 256 are what Windows asks for, and picks between by context: 16 in
# a title bar, 256 in a large Explorer view.  A file offering only one size gets
# a scaled version of it everywhere else, which is what a blurry icon usually
# is.  512 and 1024 are for macOS, where the Dock and Finder go that large.
for size in 16 20 24 32 40 48 64 128 256 512 1024; do
    rsvg-convert -w "$size" -h "$size" "$SVG" -o "$work/i$size.png"
done

# 256 is stored as PNG inside the .ico rather than a bitmap; anything from
# Vista on expects that, and it keeps the file a tenth of the size.
convert "$work"/i16.png "$work"/i20.png "$work"/i24.png "$work"/i32.png \
        "$work"/i40.png "$work"/i48.png "$work"/i64.png "$work"/i128.png \
        "$work"/i256.png "$HERE/windows/vclock.ico"

png2icns "$HERE/macos/vclock.icns" \
         "$work"/i16.png "$work"/i32.png "$work"/i48.png "$work"/i128.png \
         "$work"/i256.png "$work"/i512.png "$work"/i1024.png >/dev/null

echo "wrote $(du -h "$HERE/windows/vclock.ico" | cut -f1) distro/windows/vclock.ico"
echo "wrote $(du -h "$HERE/macos/vclock.icns" | cut -f1) distro/macos/vclock.icns"
