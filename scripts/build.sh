#!/usr/bin/env bash
# Khipro build entry point.
#
# Usage:
#   ./scripts/build.sh <target> [flags]
#
# Targets:
#   linux     Linux .so + .a (+ tests)
#   windows   Windows x86_64 .dll
#   android   Android .so per ABI
#   wasm      WebAssembly .js + .wasm
#   all       All targets available on this host
#   test      Linux build + conformance tests (alias for linux)
#
# Flags (any order):
#   --native    Build on host (skip container); requires local toolchain
#   --verbose   Force rich output (colors + per-step detail)
#   --quiet     Suppress everything except errors
#   -h, --help  Show this help
#
# Environment:
#   KHIPRO_NO_SYNC=1     Skip GitHub layout-version check
#   KHIPRO_DIST=<path>   Override dist/ output directory
#   KHIPRO_CONTAINER=cmd Force podman/docker/etc.
#   NO_COLOR=1           Disable ANSI colors

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT/scripts/targets/common.sh"

usage() {
  sed -n '3,21p' "$0"
  exit "${1:-0}"
}

TARGETS=()
USE_NATIVE=false
QUIET=false

while (( $# )); do
  case "$1" in
    --native)  USE_NATIVE=true ;;
    --verbose) export KHIPRO_PLAIN="" ;;
    --quiet)   QUIET=true ;;
    -h|--help) usage 0 ;;
    -*)        echo "Unknown flag: $1" >&2; usage 1 ;;
    *)         TARGETS+=("$1") ;;
  esac
  shift
done

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
  "$engine" build -f "$ROOT/container/Dockerfile" --target "$target" \
    -t "khipro-build-$target" "$ROOT" >/dev/null 2>&1
  if [[ "$QUIET" != "true" ]]; then ok; fi

  if [[ "$QUIET" != "true" ]]; then step "build"; fi
  if [[ "$QUIET" == "true" ]]; then
    "$engine" run --rm \
      -v "$ROOT:/src:Z" \
      -e KHIPRO_DIST=/src/dist \
      -e KHIPRO_NO_SYNC="${KHIPRO_NO_SYNC:-}" \
      -e KHIPRO_PLAIN=1 \
      "khipro-build-$target" >/dev/null
  else
    "$engine" run --rm \
      -v "$ROOT:/src:Z" \
      -e KHIPRO_DIST=/src/dist \
      -e KHIPRO_NO_SYNC="${KHIPRO_NO_SYNC:-}" \
      -e KHIPRO_PLAIN="${KHIPRO_PLAIN:-}" \
      "khipro-build-$target"
  fi
  if [[ "$QUIET" != "true" ]]; then ok; fi

  if [[ "$QUIET" != "true" ]]; then finish; fi
}

run_native() {
  local script="$ROOT/scripts/targets/$1.sh"
  if [[ ! -x "$script" ]]; then
    chmod +x "$script"
  fi
  if [[ "$QUIET" == "true" ]]; then
    KHIPRO_DIST="$DIST" "$script" >/dev/null
  else
    KHIPRO_DIST="$DIST" "$script"
  fi
}

build_one() {
  local name="$1"

  if [[ "$USE_NATIVE" == "true" ]]; then
    case "$name" in
      test) run_native "linux" ;;
      linux|windows|android|wasm) run_native "$name" ;;
      *) die "Unknown target: $name" ;;
    esac
    return
  fi

  case "$name" in
    linux|windows|android|wasm)
      run_container "$name"
      ;;
    test)
      run_container "linux"
      ;;
    *)
      die "Unknown target: $name"
      ;;
  esac
}

mkdir -p "$DIST"

if [[ ${#TARGETS[@]} -gt 1 ]]; then
  section "khipro" "${TARGETS[*]}"
fi

FAILED=0
for t in "${TARGETS[@]}"; do
  case "$t" in
    linux|windows|android|wasm|test)
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
