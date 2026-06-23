#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"
source "$(dirname "$0")/stage.sh"

OUT="$DIST/windows/x86_64"
BUILD="$ROOT/build/windows-x86_64"
JOBS="$(nproc 2>/dev/null || echo 4)"

section "khipro" "windows · x86_64"

sync_resources

step "configure"
rm -rf "$BUILD"
cmake -S "$ROOT" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchains/mingw-w64-64.cmake" \
  -DKHIPRO_BUILD_SHARED=ON \
  -DKHIPRO_BUILD_STATIC=ON \
  -DKHIPRO_BUILD_TESTS=OFF >/dev/null 2>&1
ok

step "build"
cmake --build "$BUILD" -j"$JOBS" >/dev/null 2>&1
ok

step "stage"
mkdir -p "$OUT/bin" "$OUT/lib"
shopt -s nullglob
dll=( "$BUILD/core"/*.dll )
if [[ ${#dll[@]} -eq 0 ]]; then
  fail "no .dll produced"
  exit 1
fi
cp "${dll[0]}" "$OUT/bin/khipro.dll"
a=( "$BUILD/core"/*.dll.a "$BUILD/core"/*.a )
for f in "${a[@]}"; do
  [[ -f "$f" ]] && cp "$f" "$OUT/lib/"
done
stage_c_sdk "$OUT"
ok "$OUT"

finish
