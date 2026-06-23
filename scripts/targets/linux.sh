#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"
source "$(dirname "$0")/stage.sh"

ARCH="${KHIPRO_LINUX_ARCH:-$(uname -m)}"
OUT="$DIST/linux/$ARCH"
BUILD="$ROOT/build/linux-$ARCH"
JOBS="$(nproc 2>/dev/null || echo 4)"

section "khipro" "linux · $ARCH"

sync_resources

step "configure"
rm -rf "$BUILD"
cmake -S "$ROOT" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DKHIPRO_BUILD_SHARED=ON \
  -DKHIPRO_BUILD_STATIC=ON \
  -DKHIPRO_BUILD_TESTS=ON >/dev/null 2>&1
ok

step "build"
cmake --build "$BUILD" -j"$JOBS" >/dev/null 2>&1
ok

step "test"
if ctest --test-dir "$BUILD" --output-on-failure >/tmp/khipro-test.log 2>&1; then
  cases="$("$BUILD/tests/khipro_conformance" 2>&1 | awk '/^PASS/ {sum+=$2} END {print sum+0}')"
  ok "${cases:+$cases cases}"
else
  fail "see /tmp/khipro-test.log"
  exit 1
fi

step "stage"
mkdir -p "$OUT/lib"
cp "$BUILD/core/libkhipro.so" "$OUT/lib/"
cp "$BUILD/core/libkhipro.a" "$OUT/lib/"
stage_c_sdk "$OUT"
ok "$OUT"

finish
