#!/usr/bin/env bash
# Developer flow: pull the four layouts from upstream, regenerate the spec
# tables, then build and run every conformance suite. KHIPRO_TEST_REQUIRED
# makes missing TSVs a hard failure — sync just pulled them. Runs natively.
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
  printf 'sync: spec tables changed — prove them with the tests below, then commit core/src/spec_generated.{h,cpp}\n' >&2
else
  ok "spec tables unchanged"
fi

# Build and test exactly what was just generated, natively. KHIPRO_NO_SYNC
# keeps the linux target from re-fetching; sync.sh already refreshed everything.
KHIPRO_NO_SYNC=1 KHIPRO_TEST_REQUIRED=1 "$ROOT/scripts/targets/linux.sh"
