#!/usr/bin/env bash
# Khipro build entry point.
#
# Usage:
#   ./scripts/build.sh <target> [flags]
#
# Targets:
#   linux     Linux .so + .a (+ tests)     — native by default; --container for containers
#   macos     macOS .dylib + .a (+ tests)  — host-only, requires a Mac
#   windows   Windows x86_64 .dll          — container by default; --native for host
#   android   Android .so per ABI          — container by default; --native for host
#   wasm      WebAssembly .js + .wasm      — container by default; --native for host
#   all       windows android wasm (container) and linux (native)
#   test      Build + all four conformance suites — always native; --container to use one
#   sync      Pull layouts, regenerate spec tables, build + test — always native
#
# Flags (any order):
#   --native    Build on the host instead of a container
#   --container Build in a container instead of the host
#   --verbose   Force rich output (colors + per-step detail)
#   --quiet     Suppress everything except errors
#   -h, --help  Show this help
#
# Environment:
#   KHIPRO_DIST=<path>   Override dist/ output directory
#   KHIPRO_CONTAINER=cmd Force podman/docker/etc.
#   NO_COLOR=1           Disable ANSI colors

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT/scripts/targets/common.sh"

usage() {
  sed -n '3,24p' "$0"
  exit "${1:-0}"
}

TARGETS=()
USE_NATIVE=false
USE_CONTAINER=false
QUIET=false

while (( $# )); do
  case "$1" in
    --native)    USE_NATIVE=true ;;
    --container) USE_CONTAINER=true ;;
    --verbose)   export KHIPRO_PLAIN="" ;;
    --quiet)     QUIET=true ;;
    -h|--help)   usage 0 ;;
    -*)          echo "Unknown flag: $1" >&2; usage 1 ;;
    *)           TARGETS+=("$1") ;;
  esac
  shift
done

if [[ "$USE_NATIVE" == true && "$USE_CONTAINER" == true ]]; then
  echo "Flags --native and --container are mutually exclusive." >&2
  usage 1
fi

if [[ ${#TARGETS[@]} -eq 0 ]]; then
  usage 0
fi

# Auto-disable color in CI
if [[ -n "${CI:-}" || -n "${GITHUB_ACTIONS:-}" ]]; then
  export KHIPRO_PLAIN=1
fi

run_container() {
  local target="$1"
  local engine
  engine="$(container_engine)"
  if [[ -z "$engine" ]]; then
    die "No podman/docker found. Use --native or install a container engine."
  fi

  if [[ "$QUIET" != "true" ]]; then
    section "khipro" "$target · container"
  fi

  if [[ "$QUIET" != "true" ]]; then step "image"; fi
  local img_rc=0
  "$engine" build -f "$ROOT/container/Dockerfile" --target "$target" \
    -t "khipro-build-$target" "$ROOT" >/dev/null 2>&1 || img_rc=$?
  if [[ "$img_rc" -ne 0 ]]; then
    fail "image build exited ${img_rc}"
    return "$img_rc"
  fi
  if [[ "$QUIET" != "true" ]]; then ok; fi

  if [[ "$QUIET" != "true" ]]; then step "build"; fi
  # set -e is suppressed in callers guarded by `||` (build_one "$t" || FAILED=…),
  # so a failure here must be propagated explicitly or it prints "ok" and
  # returns 0 — the gate goes green on a failed build.
  local rc=0
  if [[ "$QUIET" == "true" ]]; then
    "$engine" run --rm \
      -v "$ROOT:/src:Z" \
      -e KHIPRO_DIST=/src/dist \
      -e KHIPRO_TEST_REQUIRED="${KHIPRO_TEST_REQUIRED:-}" \
      -e KHIPRO_PLAIN=1 \
      "khipro-build-$target" >/dev/null || rc=$?
  else
    "$engine" run --rm \
      -v "$ROOT:/src:Z" \
      -e KHIPRO_DIST=/src/dist \
      -e KHIPRO_TEST_REQUIRED="${KHIPRO_TEST_REQUIRED:-}" \
      -e KHIPRO_PLAIN="${KHIPRO_PLAIN:-}" \
      "khipro-build-$target" || rc=$?
  fi

  if [[ "$rc" -ne 0 ]]; then
    fail "build exited ${rc}"
    return "$rc"
  fi
  if [[ "$QUIET" != "true" ]]; then ok; fi

  if [[ "$QUIET" != "true" ]]; then finish; fi
}

run_native() {
  local script="$ROOT/scripts/targets/$1.sh"
  if [[ ! -x "$script" ]]; then
    chmod +x "$script"
  fi
  # Same `||`-guard caveat as run_container: propagate explicitly.
  local rc=0
  if [[ "$QUIET" == "true" ]]; then
    KHIPRO_DIST="$DIST" "$script" >/dev/null || rc=$?
  else
    KHIPRO_DIST="$DIST" "$script" || rc=$?
  fi
  if [[ "$rc" -ne 0 ]]; then
    fail "$1 build exited ${rc}"
    return "$rc"
  fi
}

build_one() {
  local name="$1"

  # Venue per target: host-first targets (linux/macos/test) run natively and
  # only enter a container on --container; cross targets (windows/android/
  # wasm) build in containers and only run natively on --native. macOS has
  # no container path at all (no macOS containers to cross from Linux).
  local in_container=false
  case "$name" in
    linux|test)
      [[ "$USE_CONTAINER" == "true" ]] && in_container=true
      ;;
    macos)
      in_container=false
      ;;
    windows|android|wasm)
      [[ "$USE_NATIVE" != "true" ]] && in_container=true
      ;;
    *)
      die "Unknown target: $name"
      ;;
  esac

  if [[ "$name" == "test" ]]; then
    # The test target maps onto the linux build with conformance mandatory.
    export KHIPRO_TEST_REQUIRED=1
    name="linux"
  fi

  if [[ "$in_container" == "true" ]]; then
    run_container "$name"
  else
    run_native "$name"
  fi
}

mkdir -p "$DIST"

if [[ ${#TARGETS[@]} -gt 1 ]]; then
  section "khipro" "${TARGETS[*]}"
fi

FAILED=0
for t in "${TARGETS[@]}"; do
  case "$t" in
    sync)
      # Host-side developer flow: fetch layouts, regenerate the spec tables,
      # then build + test so the regenerated tables are proven before commit.
      "$ROOT/scripts/targets/sync.sh" || FAILED=$((FAILED + 1))
      ;;
    linux|macos|windows|android|wasm|test)
      build_one "$t" || FAILED=$((FAILED + 1))
      ;;
    all)
      for sub in linux windows android wasm; do
        build_one "$sub" || FAILED=$((FAILED + 1))
      done
      ;;
    *)
      echo "Unknown target: $t" >&2
      FAILED=$((FAILED + 1))
      ;;
  esac
done

if [[ "$FAILED" -gt 0 ]]; then
  die "$FAILED target(s) failed"
fi
