#!/usr/bin/env bash
# Layout resource sync, driven by the per-layout versions in khipro.meta.
#
#   sync.sh          For each layout: if its repo has a newer matching release
#                    tag than khipro.meta records, fetch that tag's spec +
#                    conformance TSVs and bump the recorded version.
#   sync.sh pinned   Fetch exactly the versions khipro.meta records, with no
#                    discovery. This is the release gate: the committed spec
#                    tables must regenerate byte-identically from these inputs.
#
# Tag scheme: classic layouts release as clsc-vX.Y.Z, dark layouts drk-vX.Y.Z.
# Every release pairs the .mim spec with its conformance TSV.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
META="$ROOT/khipro.meta"
RESOURCES="$ROOT/resources"
RAW="https://raw.githubusercontent.com"

MODE="${1:-latest}"
case "$MODE" in
  latest|pinned) ;;
  *) echo "usage: sync.sh [latest|pinned]" >&2; exit 2 ;;
esac

log() { printf 'sync: %s\n' "$*" >&2; }

meta_get() { grep -E "^$1=" "$META" | head -1 | cut -d= -f2-; }
meta_set() { sed -i "s/^$1=.*/$1=$2/" "$META"; }

# name repo tag-prefix mim-src mim-dest tsv-src tsv-dest meta-key
LAYOUTS=(
  "classic|rank-coder/khipro-m17n|clsc|bn-khipro-classic.mim|bn-khipro-classic.mim|khipro-testcases.tsv|khipro-testcases.tsv|khipro_classic_version"
  "classic_touch|KhiproTeam/khipro-mim-touchscreen|clsc|bn-khipro-classic.mim|bn-khipro-classic-touchscreen.mim|khipro-testcases.tsv|khipro-testcases-touch.tsv|khipro_touchscreen_classic_version"
  "dark|KhiproTeam/khipro-dark-m17n|drk|bn-khipro-dark.mim|bn-khipro-dark.mim|khipro-testcases.tsv|khipro-testcases-dark.tsv|khipro_dark_version"
  "dark_touch|KhiproTeam/khipro-dark-mim-touchscreen|drk|bn-khipro-dark.mim|bn-khipro-dark-touchscreen.mim|khipro-testcases.tsv|khipro-testcases-dark-touch.tsv|khipro_touchscreen_dark_version"
)

fetch() { # <owner/repo> <tag> <src-path> <dest-path>
  curl -fsSL --retry 3 --retry-delay 2 "$RAW/$1/$2/$3" -o "$RESOURCES/$4" || return 1
  [[ -s "$RESOURCES/$4" ]] || return 1
}

# Newest release tag of a repo under <prefix>-vX.Y.Z.
newest_tag() {
  git ls-remote --tags --quiet "https://github.com/$1.git" \
    | awk '{print $2}' | sed 's|refs/tags/||' | grep -v '\^{}$' \
    | grep -E "^${2}-v[0-9]+\.[0-9]+\.[0-9]+$" | sort -V | tail -1
}

sync_one() {
  local mode="$1" name="$2" repo="$3" prefix="$4" mim_src="$5" mim_dest="$6" \
        tsv_src="$7" tsv_dest="$8" key="$9"
  local current tag remote_version

  current="$(meta_get "$key")"
  if [[ -z "$current" ]]; then
    log "$name: khipro.meta missing $key"
    return 1
  fi

  if [[ "$mode" == "pinned" ]]; then
    tag="${prefix}-v${current}"
  else
    tag="$(newest_tag "$repo" "$prefix")"
    if [[ -z "$tag" ]]; then
      log "$name: no ${prefix}-vX.Y.Z release found"
      return 0
    fi
    remote_version="${tag#${prefix}-v}"
    if [[ "$remote_version" == "$current" ]]; then
      log "$name: up to date ($current)"
      return 0
    fi
    if [[ "$(printf '%s\n%s\n' "$current" "$remote_version" | sort -V | tail -1)" != "$remote_version" ]]; then
      log "$name: remote $remote_version is older than recorded $current — skipping"
      return 0
    fi
  fi

  if ! fetch "$repo" "$tag" "$mim_src" "$mim_dest"; then
    log "$name: download failed ($repo@$tag/$mim_src)"
    return 1
  fi
  if ! fetch "$repo" "$tag" "$tsv_src" "$tsv_dest"; then
    log "$name: download failed ($repo@$tag/$tsv_src)"
    return 1
  fi

  if [[ "$mode" == "latest" ]]; then
    meta_set "$key" "${tag#${prefix}-v}"
    log "$name: $current -> ${tag#${prefix}-v} ($tag)"
  else
    log "$name: pinned $tag"
  fi
}

if [[ "$MODE" == "latest" && "${KHIPRO_NO_SYNC:-}" == "1" ]]; then
  log "skipped (KHIPRO_NO_SYNC=1)"
  exit 0
fi

mkdir -p "$RESOURCES"

failed=0
for layout in "${LAYOUTS[@]}"; do
  IFS='|' read -r name repo prefix mim_src mim_dest tsv_src tsv_dest key <<<"$layout"
  sync_one "$MODE" "$name" "$repo" "$prefix" "$mim_src" "$mim_dest" "$tsv_src" "$tsv_dest" "$key" || failed=1
done

if [[ "$failed" -ne 0 ]]; then
  log "some layouts failed to sync"
  exit 1
fi
