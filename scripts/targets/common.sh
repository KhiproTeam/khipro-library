#!/usr/bin/env bash
# Shared primitives for build target scripts: paths, container detection,
# resource sync wrapper, and TTY-aware logging.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DIST="${KHIPRO_DIST:-$ROOT/dist}"
_BUILD_START="${_BUILD_START:-$(date +%s.%N)}"

if [[ -t 1 && -z "${NO_COLOR:-}" && "${KHIPRO_PLAIN:-}" != "1" ]]; then
  C_RESET=$'\033[0m'
  C_DIM=$'\033[2m'
  C_GREEN=$'\033[32m'
  C_RED=$'\033[31m'
  C_YELLOW=$'\033[33m'
  C_BLUE=$'\033[34m'
else
  C_RESET="" C_DIM="" C_GREEN="" C_RED="" C_YELLOW="" C_BLUE=""
fi

container_engine() {
  if [[ -n "${KHIPRO_CONTAINER:-}" ]]; then
    echo "$KHIPRO_CONTAINER"
    return
  fi
  if command -v podman >/dev/null 2>&1; then
    echo podman
    return
  fi
  if command -v docker >/dev/null 2>&1; then
    echo docker
    return
  fi
  echo ""
}

sync_resources() {
  if [[ "${KHIPRO_NO_SYNC:-}" == "1" ]]; then
    step "sync"
    skip "pinned (KHIPRO_NO_SYNC=1)"
    return 0
  fi
  step "sync"
  if "$ROOT/scripts/resources/sync.sh" >/tmp/khipro-sync.log 2>&1; then
    ok
  else
    fail "see /tmp/khipro-sync.log"
    return 1
  fi
}

section() {
  if [[ -n "$C_BLUE" ]]; then
    printf '\n  %s· %s · %s%s\n' "$C_BLUE" "$1" "${2:-}" "$C_RESET"
  else
    printf '\n[%s %s]\n' "$1" "${2:-}"
  fi
}

step() {
  if [[ -n "$C_BLUE" ]]; then
    printf '  %s→%-14s%s ' "$C_DIM" "$1" "$C_RESET"
  else
    printf '%s: ' "$1"
  fi
}

ok() {
  if [[ -n "$C_GREEN" ]]; then
    printf '%s✓%s %s\n' "$C_GREEN" "$C_RESET" "${1:-}"
  else
    printf 'ok %s\n' "${1:-}"
  fi
}

fail() {
  if [[ -n "$C_RED" ]]; then
    printf '%s✗%s %s\n' "$C_RED" "$C_RESET" "${1:-}" >&2
  else
    printf 'fail %s\n' "${1:-}" >&2
  fi
}

skip() {
  if [[ -n "$C_YELLOW" ]]; then
    printf '%sskipped%s %s\n' "$C_YELLOW" "$C_RESET" "${1:-}"
  else
    printf 'skipped %s\n' "${1:-}"
  fi
}

die() {
  fail "$*"
  exit 1
}

elapsed() {
  awk -v start="$_BUILD_START" -v now="$(date +%s.%N)" \
    'BEGIN { printf "%.1f", now - start }'
}

finish() {
  if [[ -n "$C_DIM" ]]; then
    printf '\n  %sdone in %ss%s\n' "$C_DIM" "$(elapsed)" "$C_RESET"
  else
    printf '\ndone in %ss\n' "$(elapsed)"
  fi
}
