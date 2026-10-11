#!/usr/bin/env bash
# Developer flow: pull the four layouts from upstream and regenerate the spec
# tables. Tests are deliberately not run here — run ./scripts/build.sh test
# yourself and only commit the regenerated tables once every suite passes.
set -euo pipefail

source "$(dirname "$0")/common.sh"

section "khipro" "sync"

step "sync"
if "$ROOT/scripts/resources/sync.sh" >/tmp/khipro-sync.log 2>&1; then
  ok
else
  fail "see /tmp/khipro-sync.log"
  exit 1
fi

generate_spec_tables

if [[ -n "$(git -C "$ROOT" status --porcelain -- core/src/spec_generated.cpp core/src/spec_generated.h)" ]]; then
  printf 'sync: spec tables changed — run ./scripts/build.sh test, then commit core/src/spec_generated.{h,cpp}\n' >&2
else
  ok "spec tables unchanged"
fi
