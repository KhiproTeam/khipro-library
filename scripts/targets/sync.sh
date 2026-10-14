#!/usr/bin/env bash
# Sync orchestration: pull layouts (walk modes commit as they go), regenerate
# the spec tables, then build and run every conformance suite natively.
#
#   fetch    Build helper for dev trees: newest-stable pull, no commits.
#            Tables regenerate locally; nothing is committed.
#   develop  Develop-branch walk (meta-pointer commits, one per upstream
#            release). Tables regenerate locally for the build; develop is
#            the union line, nothing releases from it.
#   stable   Main-branch walk (finals only). Nothing commits until the
#            suites pass — then meta + regenerated tables + the library
#            bump land as one commit. Releases cut from main are
#            full/stable. A failing run commits nothing: fix the cause and
#            rerun.
#   beta     Beta-branch walk (betas only). Same contract; bump carries a
#            -beta suffix. Releases cut from beta are prereleases.
#   pinned   Release gate: recorded inputs must regenerate the committed
#            tables byte-identically, then everything builds and passes.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=../resources/meta.sh
source "$ROOT/scripts/resources/meta.sh"
source "$ROOT/scripts/targets/common.sh"

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

changed="$(git -C "$ROOT" status --porcelain -- core/src/spec_generated.h core/src/spec_classic.cpp core/src/spec_classic_touch.cpp core/src/spec_dark.cpp core/src/spec_dark_touch.cpp khipro.meta resources)"
case "$MODE" in
  pinned)
    if [[ -n "$changed" ]]; then
      fail "regenerated spec tables differ from the committed ones — tables and khipro.meta have drifted"
      echo "--- drift diff (first 60 lines) ---" >&2
      git -C "$ROOT" diff --stat -- core/src/spec_generated.h core/src/spec_classic.cpp \
        core/src/spec_classic_touch.cpp core/src/spec_dark.cpp \
        core/src/spec_dark_touch.cpp 2>/dev/null | head -10 >&2
      git -C "$ROOT" diff -- core/src/spec_generated.h core/src/spec_classic.cpp \
        core/src/spec_classic_touch.cpp core/src/spec_dark.cpp core/src/spec_dark_touch.cpp 2>/dev/null | head -60 >&2
      exit 1
    fi
    ok "spec tables match the recorded inputs"
    ;;
  stable|beta)
    ok "walk took content — commit lands after the suites below"
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

# Gate passed — now (and only now) commit the generated tables, the pointer
# moves, and the library bump. A failed run above left the tree dirty on
# purpose: fix the cause, rerun, and this commit happens then.
if [[ "$MODE" == stable || "$MODE" == beta ]]; then
  step "commit"
  pending="$(git -C "$ROOT" status --porcelain -- khipro.meta core/src/spec_generated.h \
    core/src/spec_classic.cpp core/src/spec_classic_touch.cpp \
    core/src/spec_dark.cpp core/src/spec_dark_touch.cpp)"
  if [[ -z "$pending" ]]; then
    skip "nothing to commit (walk took no content)"
  else
    git -C "$ROOT" add khipro.meta core/src/spec_generated.h \
      core/src/spec_classic.cpp core/src/spec_classic_touch.cpp \
      core/src/spec_dark.cpp core/src/spec_dark_touch.cpp
    git -C "$ROOT" commit -q -m "tables: library $(meta_get library_version)"
    ok "tables: library $(meta_get library_version)"
  fi
fi
