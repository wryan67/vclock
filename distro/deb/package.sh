#!/bin/sh
# Runs inside the deb container.  /src is the read-only source tree, /out is
# where the finished package is left.
set -e

cmake -S /src -B /tmp/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/build -j"$(nproc)"

cd /tmp/build
cpack -G DEB

for f in /tmp/build/*.deb; do
    # dpkg-deb refuses to describe a file it cannot parse, so this doubles as a
    # check that what we are about to hand over is a package at all.
    dpkg-deb -I "$f" >/dev/null
    cp "$f" /out/
    echo "built $(basename "$f")"
done
