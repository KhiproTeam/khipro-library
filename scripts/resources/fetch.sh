#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LOCK_FILE="$ROOT/resources/.lock"
RESOURCES="$ROOT/resources"

# shellcheck source=meta.sh
. "$(dirname "${BASH_SOURCE[0]}")/meta.sh"

read_lock() {
  local key="$1"
  grep "^${key}=" "$LOCK_FILE" | cut -d= -f2-
}

download_testcases() {
  local repo="$1"
  local ref="$2"
  local dest="$3"
  local url="https://raw.githubusercontent.com/${repo}/${ref}/khipro-testcases.tsv"
  curl -fsSL --max-time 60 "$url" -o "$dest"
}

unset KHIPRO_NO_SYNC
"$ROOT/scripts/resources/sync.sh"

TC_DEFAULT_REPO="$(read_lock TESTCASES_DEFAULT_REPO)"
TC_DEFAULT_REF="$(read_lock TESTCASES_DEFAULT_REF)"
TC_TOUCH_REPO="$(read_lock TESTCASES_TOUCHSCREEN_REPO)"
TC_TOUCH_REF="$(read_lock TESTCASES_TOUCHSCREEN_REF)"

download_testcases "$TC_DEFAULT_REPO" "$TC_DEFAULT_REF" "$RESOURCES/khipro-testcases.tsv"
download_testcases "$TC_TOUCH_REPO" "$TC_TOUCH_REF" "$RESOURCES/khipro-testcases-touch.tsv"

python3 "$ROOT/scripts/resources/embed.py" \
  --default "$RESOURCES/bn-khipro.mim" \
  --touchscreen "$RESOURCES/bn-khipro-touchscreen.mim" \
  --out "$ROOT/core/src/embedded_specs.h"

echo "Resources refreshed (library version $(meta_library_version))."
