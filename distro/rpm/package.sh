#!/bin/sh
# Runs inside the rpm container.  /src is the read-only source tree, /out is
# where the finished package is left.
set -e

cmake -S /src -B /tmp/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/build -j"$(nproc)"

cd /tmp/build
cpack -G RPM

for f in /tmp/build/*.rpm; do
    rpm -qip "$f" >/dev/null
    cp "$f" /out/
    echo "built $(basename "$f")"
done
