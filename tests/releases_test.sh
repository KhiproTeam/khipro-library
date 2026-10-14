#!/usr/bin/env bash
# Test matrix for the release-ordering and walk-slice logic (releases.sh).
# Run: bash tests/releases_test.sh
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=../scripts/resources/releases.sh
source "$HERE/../scripts/resources/releases.sh"

failures=0
check() { # <label> <got> <want>
  if [[ "$2" != "$3" ]]; then
    echo "FAIL $1"
    echo "  got:  $(printf '%q' "$2")"
    echo "  want: $(printf '%q' "$3")"
    failures=$((failures + 1))
  else
    echo "ok   $1"
  fi
}

mklist() { # raw tag names (with repo prefix)... -> ordered canonical list file
  local f; f="$(mktemp)"
  printf '%s\n' "$@" | sed -E 's/^[a-z]+-v//' | ordered_from_tags /dev/stdin >"$f"
  echo "$f"
}

# --- ordering: base ascending, beta before final within a base -------------
list="$(mklist clsc-v37.0.2 clsc-v37.0.1 clsc-v37.0.2-beta clsc-v37.0.3-beta)"
check "order: beta precedes final of same base, bases ascend" \
  "$(ordered_versions "$list")" \
  "$(printf '37.0.1\n37.0.2-beta\n37.0.2\n37.0.3-beta')"

# --- slicing: everything strictly after the pointer ------------------------
list="$(mklist clsc-v37.0.1 clsc-v37.0.2-beta clsc-v37.0.2 clsc-v37.0.3-beta)"
check "slice: after a beta pointer yields the final and later" \
  "$(slice_after "$list" "37.0.2-beta" | cut -f1 | tr '\n' ' ')" \
  "37.0.2 37.0.3-beta "
check "slice: after the final of a base yields later bases only" \
  "$(slice_after "$list" "37.0.2" | cut -f1 | tr '\n' ' ')" \
  "37.0.3-beta "
check "slice: empty pointer yields everything" \
  "$(slice_after "$list" "" | cut -f1 | wc -l)" \
  "4"
check "slice: pointer at tip yields nothing" \
  "$(slice_after "$list" "37.0.3-beta" | wc -l)" \
  "0"

# --- slicing fallback: pointer tag vanished upstream ------------------------
list="$(mklist clsc-v37.0.1 clsc-v37.0.3-beta)"
check "slice: deleted pointer falls back to strictly-newer bases" \
  "$(slice_after "$list" "37.0.2-beta" | cut -f1 | tr '\n' ' ')" \
  "37.0.3-beta "
check "slice: deleted pointer keeps same-base final reachable" \
  "$(slice_after "$list" "37.0.1-beta" | cut -f1 | tr '\n' ' ')" \
  "37.0.1 37.0.3-beta "

# --- newest-of-kind ---------------------------------------------------------
list="$(mklist clsc-v37.0.1 clsc-v37.0.2-beta clsc-v37.0.2 clsc-v37.0.3-beta)"
check "newest final" "$(newest_of_kind "$list" final)" "37.0.2"
check "newest beta"  "$(newest_of_kind "$list" beta)"  "37.0.3-beta"
check "newest any"   "$(newest_of_kind "$list" any)"   "37.0.3-beta"
beta_only="$(mklist clsc-v37.0.2-beta)"
check "newest final: none" "$(newest_of_kind "$beta_only" final)" ""

# --- needs matrix -----------------------------------------------------------
# pointers: m = main ledger, s = stable branch, b = beta branch
finals_of() { awk -F'\t' '$2 == "final"' "$1"; }
needs() { # <list> <m> <s> <b> -> "main stable beta" subset
  local r=""
  [[ -n "$(slice_after "$1" "$2")" ]] && r="main"
  local finals; finals="$(mktemp)"; finals_of "$1" >"$finals"
  [[ -n "$(slice_after "$finals" "$3")" ]] && r="$r stable"
  [[ -n "$(slice_after "$1" "$4")" ]] && r="$r beta"
  echo "$r" | xargs
}

list="$(mklist clsc-v37.0.1 clsc-v37.0.2-beta clsc-v37.0.2)"
check "needs: fresh clone, everything behind" \
  "$(needs "$list" "" "" "")" "main stable beta"
check "needs: ledger current, branches behind" \
  "$(needs "$list" "37.0.2" "" "")" "stable beta"
check "needs: nothing to do" \
  "$(needs "$list" "37.0.2" "37.0.2" "37.0.2")" ""
check "needs: only a beta arrived" \
  "$(needs "$(mklist clsc-v37.0.1 clsc-v37.0.2-beta)" "37.0.1" "37.0.1" "37.0.1")" \
  "main beta"
check "needs: only a final arrived (classic beta scenario inverse)" \
  "$(needs "$(mklist clsc-v37.0.1 clsc-v37.0.2)" "37.0.1" "37.0.1" "37.0.1")" \
  "main stable beta"

if [[ $failures -ne 0 ]]; then
  echo "$failures test(s) failed"
  exit 1
fi
echo "all release-logic tests passed"
