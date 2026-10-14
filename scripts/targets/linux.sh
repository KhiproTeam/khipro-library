#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"
source "$(dirname "$0")/stage.sh"

ARCH="${KHIPRO_LINUX_ARCH:-$(uname -m)}"
OUT="$DIST/linux/$ARCH"
BUILD="$ROOT/build/linux-$ARCH"
JOBS="$(nproc 2>/dev/null || echo 4)"

section "khipro" "linux · $ARCH"

# Conformance needs the upstream TSVs, which only `make sync` pulls. The test
# target demands them; plain builds skip the tests instead.
TESTS_ENABLED=true
if [[ ! -f "$ROOT/resources/khipro-testcases.tsv" ]]; then
  if [[ "${KHIPRO_TEST_REQUIRED:-0}" == "1" ]]; then
    fail "conformance TSVs missing — run ./scripts/build.sh sync first"
    exit 1
  fi
  TESTS_ENABLED=false
fi

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
if [[ "$TESTS_ENABLED" != "true" ]]; then
  skip "no TSVs — run ./scripts/build.sh sync"
elif ctest --test-dir "$BUILD" --output-on-failure >/tmp/khipro-test.log 2>&1; then
  detail="$("$BUILD/tests/khipro_conformance" 2>&1 | awk '/^PASS/ {sub(/:/, "", $2); printf "%s%s %s", sep, $2, $3; sep=" · "}')"
  ok "$detail"
else
  fail "see /tmp/khipro-test.log"
  exit 1
fi

step "stage"
mkdir -p "$OUT/lib"
cp "$BUILD/core/libkhipro.so" "$OUT/lib/"
cp "$BUILD/core/libkhipro.a" "$OUT/lib/"
stage_c_sdk "$OUT" "$BUILD"
ok "$OUT"

finish
