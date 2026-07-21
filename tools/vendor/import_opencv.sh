#!/usr/bin/env bash
# One-time (re-)import of the pinned OpenCV source into third_party/.
# Host-independent: with no argument it downloads the pinned official tarball
# itself (into a gitignored work dir) and verifies its sha256 before pruning.
# Usage: tools/vendor/import_opencv.sh [path-to-extracted-opencv-5.0.0]
set -euo pipefail
VER="5.0.0"
PIN_SHA="b0528f5a1d379d59d4701cb28c36e22214cc51cf64594e5b56f2d3e6c0233095"
URL_GLOBAL="https://github.com/opencv/opencv/archive/refs/tags/${VER}.tar.gz"
URL_CN="https://gitcode.com/mcpp-res/opencv/releases/download/${VER}/opencv-${VER}.tar.gz"
ROOT="$(git rev-parse --show-toplevel)"
DST="$ROOT/third_party/opencv-${VER}"

SRC="${1:-}"
if [ -z "$SRC" ]; then
    WORK="$ROOT/target/vendor-import"
    mkdir -p "$WORK"
    TAR="$WORK/opencv-${VER}.tar.gz"
    if ! echo "$PIN_SHA  $TAR" | sha256sum -c - 2>/dev/null; then
        URL="$URL_GLOBAL"; [ "${MCPP_MIRROR:-}" = "CN" ] && URL="$URL_CN"
        echo "downloading $URL"
        curl -fL --retry 3 -o "$TAR" "$URL"
        echo "$PIN_SHA  $TAR" | sha256sum -c -
    fi
    rm -rf "$WORK/opencv-${VER}"
    tar -xzf "$TAR" -C "$WORK"
    SRC="$WORK/opencv-${VER}"
else
    # provenance check when the tarball sits next to a user-supplied tree
    TAR="$(dirname "$SRC")/opencv-${VER}.tar.gz"
    if [ -f "$TAR" ]; then
        echo "$PIN_SHA  $TAR" | sha256sum -c - || exit 1
    fi
fi
[ -f "$SRC/CMakeLists.txt" ] || { echo "not an opencv tree: $SRC" >&2; exit 1; }

rm -rf "$DST"; mkdir -p "$DST"
KEEP_MODULES="core imgproc imgcodecs highgui videoio dnn flann geometry"
KEEP_3RDPARTY="libjpeg-turbo libpng zlib protobuf mlas flatbuffers dlpack"
cp "$SRC"/{LICENSE,COPYRIGHT,README.md} "$DST/"
cp -r "$SRC/include" "$DST/include"
mkdir -p "$DST/modules" "$DST/3rdparty" "$DST/modules/python"
for m in $KEEP_MODULES; do cp -r "$SRC/modules/$m" "$DST/modules/$m"; done
cp -r "$SRC/modules/python/src2" "$DST/modules/python/src2"
for t in $KEEP_3RDPARTY; do cp -r "$SRC/3rdparty/$t" "$DST/3rdparty/$t"; done
cp "$SRC/3rdparty/readme.txt" "$DST/3rdparty/"
# Slim the kept modules: test/perf trees and non-C++ binding misc dirs.
for m in $KEEP_MODULES; do
    rm -rf "$DST/modules/$m"/{test,perf}
    for b in java objc python js; do rm -rf "$DST/modules/$m/misc/$b"; done
done
du -sh "$DST"
