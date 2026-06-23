#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LOCK_FILE="$ROOT/resources/.lock"
META_FILE="$ROOT/khipro.meta"
RESOURCES="$ROOT/resources"

# shellcheck source=meta.sh
. "$(dirname "${BASH_SOURCE[0]}")/meta.sh"

MIM_DEFAULT_REPO="rank-coder/khipro-m17n"
MIM_TOUCHSCREEN_REPO="KhiproTeam/khipro-mim-touchscreen"
MIM_FILE="bn-khipro.mim"
DEFAULT_DEST="$RESOURCES/bn-khipro.mim"
TOUCH_DEST="$RESOURCES/bn-khipro-touchscreen.mim"

log() { printf 'sync: %s\n' "$*" >&2; }

read_lock() {
  local key="$1"
  if [[ -f "$LOCK_FILE" ]]; then
    grep "^${key}=" "$LOCK_FILE" 2>/dev/null | cut -d= -f2- || true
  fi
}

write_lock() {
  local default_tag="$1"
  local touch_tag="$2"
  local default_sha="$3"
  local touch_sha="$4"
  local fetched_at
  fetched_at="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

  cat >"$LOCK_FILE" <<EOF
MIM_DEFAULT_TAG=${default_tag}
MIM_TOUCHSCREEN_TAG=${touch_tag}
MIM_DEFAULT_REPO=${MIM_DEFAULT_REPO}
MIM_TOUCHSCREEN_REPO=${MIM_TOUCHSCREEN_REPO}
MIM_DEFAULT_SHA256=${default_sha}
MIM_TOUCHSCREEN_SHA256=${touch_sha}
TESTCASES_DEFAULT_REPO=KhiproTeam/khipro-testcases
TESTCASES_DEFAULT_REF=main
TESTCASES_TOUCHSCREEN_REPO=KhiproTeam/khipro-testcases-touch
TESTCASES_TOUCHSCREEN_REF=main
FETCHED_AT=${fetched_at}
EOF
}

bump_meta_version() {
  local new_version="$1"
  python3 -c "
import sys
sys.path.insert(0, '$ROOT/scripts/resources')
from meta import set_fields
from pathlib import Path
set_fields(Path('$META_FILE'), {'layout_version': '$new_version', 'library_patch': '0'})
"
}

sha256_file() {
  sha256sum "$1" | awk '{print $1}'
}

tag_to_version() {
  echo "${1#v}"
}

is_stable_tag() {
  local tag="${1#refs/tags/}"
  tag="${tag#v}"
  [[ "$tag" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]
}

fetch_remote_stable_tags() {
  local repo="$1"
  git ls-remote --tags --sort=-v:refname "https://github.com/${repo}.git" 2>/dev/null \
    | awk '{print $2}' \
    | sed 's|refs/tags/||' \
    | grep -v '\^{}$' \
    | while read -r tag; do
        if is_stable_tag "$tag"; then
          echo "$tag"
        fi
      done
}

version_gt() {
  [[ "$(printf '%s\n%s\n' "$2" "$1" | sort -V | tail -1)" == "$1" && "$1" != "$2" ]]
}

network_reachable() {
  curl -fsSI --max-time 5 -o /dev/null "https://api.github.com" 2>/dev/null
}

download_mim() {
  local repo="$1"
  local tag="$2"
  local dest="$3"
  local url="https://raw.githubusercontent.com/${repo}/${tag}/${MIM_FILE}"
  mkdir -p "$(dirname "$dest")"
  curl -fsSL --max-time 60 "$url" -o "$dest"
}

find_newest_common_tag() {
  local default_tags touch_tags
  default_tags="$(fetch_remote_stable_tags "$MIM_DEFAULT_REPO")"
  touch_tags="$(fetch_remote_stable_tags "$MIM_TOUCHSCREEN_REPO")"

  if [[ -z "$default_tags" || -z "$touch_tags" ]]; then
    return 1
  fi

  comm -12 \
    <(printf '%s\n' "$default_tags" | sort -V) \
    <(printf '%s\n' "$touch_tags" | sort -V) \
    | tail -1
}

CURRENT_VERSION="$(meta_get layout_version)"
CURRENT_VERSION="${CURRENT_VERSION:-0.0.0}"

if [[ "${KHIPRO_NO_SYNC:-}" == "1" ]]; then
  log "skipped (KHIPRO_NO_SYNC=1); using pinned ${CURRENT_VERSION}"
  exit 0
fi

if ! network_reachable; then
  log "offline; using pinned version ${CURRENT_VERSION}"
  exit 0
fi

NEWEST_TAG="$(find_newest_common_tag || true)"
if [[ -z "$NEWEST_TAG" ]]; then
  log "could not resolve common stable tag; using pinned version ${CURRENT_VERSION}"
  exit 0
fi

NEW_VERSION="$(tag_to_version "$NEWEST_TAG")"

if ! version_gt "$NEW_VERSION" "$CURRENT_VERSION"; then
  log "up to date (${CURRENT_VERSION}); remote latest common stable is ${NEW_VERSION}"
  exit 0
fi

log "updating ${CURRENT_VERSION} -> ${NEW_VERSION} (tag ${NEWEST_TAG})"

if ! download_mim "$MIM_DEFAULT_REPO" "$NEWEST_TAG" "$DEFAULT_DEST"; then
  log "failed to download default .mim; keeping pinned version ${CURRENT_VERSION}"
  exit 0
fi

if ! download_mim "$MIM_TOUCHSCREEN_REPO" "$NEWEST_TAG" "$TOUCH_DEST"; then
  log "failed to download touchscreen .mim; keeping pinned version ${CURRENT_VERSION}"
  exit 0
fi

DEFAULT_SHA="$(sha256_file "$DEFAULT_DEST")"
TOUCH_SHA="$(sha256_file "$TOUCH_DEST")"

write_lock "$NEWEST_TAG" "$NEWEST_TAG" "$DEFAULT_SHA" "$TOUCH_SHA"
bump_meta_version "$NEW_VERSION"
log "updated to ${NEW_VERSION}; library_patch reset to 0"
