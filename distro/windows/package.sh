#!/bin/bash
# Runs inside the Windows cross-build container.  /src is the read-only source
# tree, /out is where the finished installer is left.
set -euo pipefail

PREFIX=/usr/x86_64-w64-mingw32/sys-root/mingw
OBJDUMP=x86_64-w64-mingw32-objdump
STAGE=/tmp/stage

mingw64-cmake -S /src -B /tmp/build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/build -j"$(nproc)"

rm -rf "$STAGE"
mkdir -p "$STAGE"
cp /tmp/build/vclock.exe "$STAGE/"
cp /src/vclock.svg "$STAGE/"

# ------------------------------------------------------------------- the DLLs
#
# Walked out of the binaries themselves rather than listed by hand.  A hand
# written list is right until the day a Qt module is added and then ships an
# installer that dies on startup with a dialog naming a missing DLL.
#
# Anything not found in the MinGW prefix is part of Windows -- kernel32,
# user32, and the rest -- and is deliberately not bundled.

copied=""
collect() {
    local file=$1 dll
    for dll in $("$OBJDUMP" -p "$file" 2>/dev/null |
                 sed -n 's/^\s*DLL Name:\s*//p'); do
        case " $copied " in *" $dll "*) continue ;; esac
        if [ -f "$PREFIX/bin/$dll" ]; then
            copied="$copied $dll"
            cp "$PREFIX/bin/$dll" "$STAGE/"
            collect "$PREFIX/bin/$dll"
        fi
    done
}
collect "$STAGE/vclock.exe"

# --------------------------------------------------------------- the plugins
#
# Qt loads these by directory at runtime, so nothing links against them and the
# walk above cannot see them.  Without qwindows.dll the program starts and then
# aborts with "no Qt platform plugin could be initialized".

plugins=""
for candidate in "$PREFIX/lib/qt6/plugins" "$PREFIX/share/qt6/plugins" \
                 "$PREFIX/lib/qt6/plugin" "$PREFIX/plugins"; do
    [ -d "$candidate/platforms" ] && { plugins=$candidate; break; }
done
if [ -z "$plugins" ]; then
    # Falling back to a search.  Unreadable directories are expected -- this
    # runs as the invoking user, not as root -- so its noise and exit status
    # are both ignored.
    plugins=$(find "$PREFIX" -type d -name platforms 2>/dev/null | head -1 || true)
    plugins=${plugins%/platforms}
fi
[ -n "$plugins" ] || { echo "could not find the Qt plugin directory" >&2; exit 1; }
echo "plugins from $plugins"

for group in platforms styles imageformats iconengines; do
    [ -d "$plugins/$group" ] || continue
    mkdir -p "$STAGE/$group"
    cp "$plugins/$group"/*.dll "$STAGE/$group/" 2>/dev/null || true
    for dll in "$STAGE/$group"/*.dll; do
        [ -f "$dll" ] && collect "$dll"
    done
done

[ -f "$STAGE/platforms/qwindows.dll" ] ||
    { echo "the windows platform plugin is missing; the build would not run" >&2; exit 1; }

x86_64-w64-mingw32-strip "$STAGE"/*.exe "$STAGE"/*.dll "$STAGE"/*/*.dll 2>/dev/null || true

version=$(tr '\n' ' ' < /src/CMakeLists.txt |
    sed -n 's/.*project(vclock[[:space:]][[:space:]]*VERSION[[:space:]][[:space:]]*\([0-9][0-9.]*\).*/\1/p')
# A version that could not be read is not a reason to invent one.  The deb and
# the rpm get theirs from CPack, which reads the real CMake variable, so a
# fallback here would not keep the installer in step with them -- it would
# quietly stamp the old number on it and produce exactly the mismatch it looks
# like it is guarding against.
[ -n "$version" ] ||
    { echo "could not read the project version from CMakeLists.txt" >&2; exit 1; }

echo "bundled $(find "$STAGE" -name '*.dll' | wc -l) DLLs"

makensis -NOCD \
    -DVERSION="$version" \
    -DSTAGE="$STAGE" \
    -DICON="/src/distro/windows/vclock.ico" \
    -DOUTFILE="/tmp/vclock-$version-windows-x64-setup.exe" \
    /src/distro/windows/vclock.nsi >/tmp/nsis.log 2>&1 ||
    { echo "makensis failed"; tail -20 /tmp/nsis.log; exit 1; }

cp "/tmp/vclock-$version-windows-x64-setup.exe" /out/

# makensis leaves the installer 644.  Windows does not have a mode bit and does
# not care, but the file rarely goes straight from here to Windows -- it is
# served, copied to a share, or handed over by scp, and a 644 that arrives on a
# filesystem which does honour the bit is one nobody can run.  Marking it
# executable here costs nothing and saves a chmod at the far end.
chmod 755 "/out/vclock-$version-windows-x64-setup.exe"

echo "built vclock-$version-windows-x64-setup.exe"
