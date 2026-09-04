#!/bin/bash
#
# package.sh -- build vclock.app and a disk image.  Runs on macOS.
#
#     distro/macos/package.sh [--out DIR]
#
# This is the one target that cannot be cross-built from Linux.  Apple's SDK
# licence restricts its use to Apple hardware, and since Catalina an app that is
# neither signed nor notarised is refused by Gatekeeper rather than merely
# warned about.  So this script runs on a Mac -- yours, or one of GitHub's
# runners by way of .github/workflows/release.yml.
#
# Signing is done only if the environment provides an identity, so it runs
# unsigned on a developer machine without complaint:
#
#     CODESIGN_IDENTITY   e.g. "Developer ID Application: Name (TEAMID)"
#
# An unsigned build is fine for your own use.  Getting it past Gatekeeper on
# someone else's machine needs signing and notarisation both.

set -euo pipefail

HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ROOT=$(cd -- "$HERE/../.." && pwd)
OUT=${VCLOCK_OUT:-$ROOT/distro/out}
BUILD=${VCLOCK_BUILD:-$ROOT/build-macos}

while [ $# -gt 0 ]; do
    case $1 in
        --out) OUT=$2; shift 2 ;;
        -h|--help) sed -n '2,22p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "unknown option $1" >&2; exit 1 ;;
    esac
done

[ "$(uname -s)" = "Darwin" ] || {
    echo "error: this builds a macOS app and has to run on macOS" >&2
    exit 1
}

mkdir -p "$OUT"

# Homebrew is not on PATH in a non-interactive shell on Apple Silicon.
for brew_prefix in /opt/homebrew /usr/local; do
    [ -x "$brew_prefix/bin/brew" ] && eval "$("$brew_prefix/bin/brew" shellenv)" && break
done

QT_PREFIX=${QT_PREFIX:-$(brew --prefix qt 2>/dev/null || true)}
[ -n "$QT_PREFIX" ] || {
    echo "error: Qt not found.  brew install qt, or set QT_PREFIX." >&2
    exit 1
}

arch=$(uname -m)
version=$(sed -n 's/^project(vclock VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")
[ -n "$version" ] || version=1.0

echo "==> building vclock $version for macOS $arch"

# ------------------------------------------------------------------- the icon
#
# The program draws its own icon, so there is no .icns in the tree to point the
# bundle at.  vclock.svg is the same artwork as a file, and iconutil turns a
# directory of PNGs into the .icns that Finder and the Dock want.

iconset=$BUILD/vclock.iconset
icns=$BUILD/vclock.icns
rm -rf "$iconset"; mkdir -p "$iconset"

if command -v rsvg-convert >/dev/null 2>&1; then
    for size in 16 32 64 128 256 512 1024; do
        rsvg-convert -w $size -h $size "$ROOT/vclock.svg" -o "$iconset/icon_${size}x${size}.png"
    done
    # Retina variants are the same image at twice the pixels, named for the
    # size they stand in for.
    for size in 16 32 128 256 512; do
        cp "$iconset/icon_$((size * 2))x$((size * 2)).png" \
           "$iconset/icon_${size}x${size}@2x.png" 2>/dev/null || true
    done
    rm -f "$iconset/icon_1024x1024.png"
    iconutil -c icns "$iconset" -o "$icns" 2>/dev/null || icns=""
else
    echo "    (no rsvg-convert; building without a bundle icon)"
    icns=""
fi

# ------------------------------------------------------------------ the build

cmake_args=(-S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_PREFIX_PATH="$QT_PREFIX"
            -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0)
[ -n "$icns" ] && cmake_args+=(-DVCLOCK_MACOS_ICON="$icns")

cmake "${cmake_args[@]}"
cmake --build "$BUILD" -j"$(sysctl -n hw.ncpu)"

app=$BUILD/vclock.app
[ -d "$app" ] || { echo "error: no vclock.app was produced" >&2; exit 1; }

# ---------------------------------------------------------------- the bundle
#
# macdeployqt copies the Qt frameworks and plugins the binary needs into the
# bundle and rewrites its load paths to point inside it.  Without this the app
# runs only on a machine that has Qt installed in the same place.

"$QT_PREFIX/bin/macdeployqt" "$app" -always-overwrite

if [ -n "${CODESIGN_IDENTITY:-}" ]; then
    echo "==> signing"
    # Inside out: a signature covers what it contains, so re-signing a nested
    # framework after the bundle would invalidate the bundle's own signature.
    codesign --force --deep --options runtime --timestamp \
             --sign "$CODESIGN_IDENTITY" "$app"
    codesign --verify --deep --strict "$app"
else
    echo "==> not signing (no CODESIGN_IDENTITY); Gatekeeper will object on"
    echo "    machines other than the one that built it"
fi

# ------------------------------------------------------------------- the dmg

dmg=$OUT/vclock-$version-macos-$arch.dmg
rm -f "$dmg"

staging=$BUILD/dmg
rm -rf "$staging"; mkdir -p "$staging"
cp -R "$app" "$staging/"
ln -s /Applications "$staging/Applications"

hdiutil create -volname "vclock $version" -srcfolder "$staging" \
        -ov -format UDZO "$dmg" >/dev/null

[ -n "${CODESIGN_IDENTITY:-}" ] &&
    codesign --force --sign "$CODESIGN_IDENTITY" "$dmg"

echo "built $(basename "$dmg")"
