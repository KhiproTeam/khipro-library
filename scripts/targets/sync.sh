#!/usr/bin/env bash
# Sync orchestration: pull layouts (walk modes commit as they go), regenerate
# the spec tables, then build and run every conformance suite natively.
#
#   fetch    Build helper for dev trees: newest-stable pull, no commits.
#            Tables regenerate locally; nothing is committed.
#   develop  Develop-branch walk (meta-pointer commits, one per upstream
#            release). Tables regenerate locally for the build; develop is
#            the union line, nothing releases from it.
#   stable   Main-branch walk (finals only). The walk commits resources +
#            meta + tables per taken release plus the library bump, so the
#            tree must be clean afterwards — then the suites gate the push.
#            Releases cut from main are full/stable.
#   beta     Beta-branch walk (betas only). Same contract; bump carries a
#            -beta suffix. Releases cut from beta are prereleases.
#   pinned   Release gate: recorded inputs must regenerate the committed
#            tables byte-identically, then everything builds and passes.
set -euo pipefail

source "$(dirname "$0")/common.sh"

MODE="${1:-fetch}"  # fetch | develop | stable | beta | pinned

section "khipro" "sync · $MODE"

step "sync"
if "$ROOT/scripts/resources/sync.sh" "$MODE" >/tmp/khipro-sync.log 2>&1; then
  ok
else
  fail "see /tmp/khipro-sync.log"
  exit 1
fi

step "codegen"
generate_spec_tables

changed="$(git -C "$ROOT" status --porcelain -- core/src/spec_generated.cpp core/src/spec_generated.h khipro.meta resources)"
case "$MODE" in
  pinned)
    if [[ -n "$changed" ]]; then
      fail "regenerated spec tables differ from the committed ones — tables and khipro.meta have drifted"
      exit 1
    fi
    ok "spec tables match the recorded inputs"
    ;;
  stable|beta)
    if [[ -n "$changed" ]]; then
      fail "walk left the tree dirty — per-release commits should have covered everything"
      exit 1
    fi
    ok "branch walked, tables committed"
    ;;
  develop)
    if [[ -n "$changed" ]]; then
      ok "tables regenerated locally (dev flavor) — committing them on develop is your call"
    else
      ok "spec tables match the recorded inputs"
    fi
    ;;
  *)
    if [[ -n "$changed" ]]; then
      printf 'sync: spec tables changed — prove them with the tests below, then commit core/src/spec_generated.{h,cpp}\n' >&2
    else
      ok "spec tables match the recorded inputs"
    fi
    ;;
esac

# Build and test exactly what was just generated, natively. KHIPRO_NO_SYNC
# keeps the linux target from re-fetching; sync.sh already refreshed everything.
KHIPRO_NO_SYNC=1 KHIPRO_TEST_REQUIRED=1 "$ROOT/scripts/targets/linux.sh"
