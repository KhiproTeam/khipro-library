#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"
source "$(dirname "$0")/stage.sh"

BUILD="$ROOT/build/wasm"
JOBS="$(nproc 2>/dev/null || echo 4)"

if ! command -v emcc >/dev/null 2>&1; then
  die "emcc not found. Source emsdk_env.sh or use: ./scripts/build.sh wasm (container)"
fi

section "khipro" "wasm"

sync_resources

step "configure"
rm -rf "$BUILD"
emcmake cmake -S "$ROOT" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DKHIPRO_BUILD_SHARED=OFF \
  -DKHIPRO_BUILD_STATIC=ON \
  -DKHIPRO_BUILD_TESTS=OFF \
  -DKHIPRO_BUILD_WEB=ON >/dev/null 2>&1
ok

step "build"
cmake --build "$BUILD" --target khipro_wasm_module -j"$JOBS" >/dev/null 2>&1
ok

step "typescript"
if command -v npm >/dev/null 2>&1; then
  (cd "$ROOT/bindings/web" && npm install --silent >/dev/null 2>&1 && npm run build >/dev/null 2>&1)
  ok
else
  fail "npm not found"
  exit 1
fi

step "stage"
stage_wasm_package "$BUILD"
ok "$DIST/wasm/"

finish
