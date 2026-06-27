#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"
source "$(dirname "$0")/stage.sh"

# arm64 (Apple Silicon) by default. Set KHIPRO_MACOS_ARCH=x86_64 for Intel,
# or KHIPRO_MACOS_ARCH=universal for a fat arm64+x86_64 dylib/.a.
ARCH="${KHIPRO_MACOS_ARCH:-$(uname -m)}"
OUT="$DIST/macos/$ARCH"
BUILD="$ROOT/build/macos-$ARCH"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

case "$ARCH" in
  universal) OSX_ARCHS="arm64;x86_64" ;;
  *)         OSX_ARCHS="$ARCH" ;;
esac

section "khipro" "macos · $ARCH"

sync_resources

step "configure"
rm -rf "$BUILD"
cmake -S "$ROOT" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="$OSX_ARCHS" \
  -DKHIPRO_BUILD_SHARED=ON \
  -DKHIPRO_BUILD_STATIC=ON \
  -DKHIPRO_BUILD_TESTS=ON >/dev/null 2>&1
ok

step "build"
cmake --build "$BUILD" -j"$JOBS" >/dev/null 2>&1
ok

# A universal build can't run its tests on a single-arch host slice cleanly;
# tests run for native single-arch builds only.
if [[ "$ARCH" != "universal" ]]; then
  step "test"
  if ctest --test-dir "$BUILD" --output-on-failure >/tmp/khipro-test.log 2>&1; then
    cases="$("$BUILD/tests/khipro_conformance" 2>&1 | awk '/^PASS/ {sum+=$2} END {print sum+0}')"
    ok "${cases:+$cases cases}"
  else
    fail "see /tmp/khipro-test.log"
    exit 1
  fi
fi

step "stage"
mkdir -p "$OUT/lib"
cp "$BUILD/core/libkhipro.dylib" "$OUT/lib/"
cp "$BUILD/core/libkhipro.a" "$OUT/lib/"
# Make the dylib relocatable so consumers can link via @rpath.
install_name_tool -id @rpath/libkhipro.dylib "$OUT/lib/libkhipro.dylib"
stage_c_sdk "$OUT"
ok "$OUT"

finish
