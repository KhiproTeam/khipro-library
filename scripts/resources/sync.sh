#!/usr/bin/env bash
# Layout resource sync, driven by the per-layout versions in khipro.meta.
#
#   sync.sh          Stable track: pull each layout's newest STABLE release
#                    (clsc-vX.Y.Z / drk-vX.Y.Z) and record its version.
#   sync.sh beta     Beta track: pull each layout's newest release of ANY
#                    kind — a -beta suffix wins when it is the newest.
#   sync.sh pinned   Fetch exactly the versions khipro.meta records, with no
#                    discovery. This is the release gate: the committed spec
#                    tables must regenerate byte-identically from these inputs.
#
# Files are ALWAYS fetched (they are tiny); the spec-table drift check after
# regeneration decides whether anything actually changed. This is what makes
# same-version re-releases (fixed betas re-tagged upstream) detectable.
#
# Tag scheme: classic layouts release as clsc-vX.Y.Z[-sfx], dark layouts as
# drk-vX.Y.Z[-sfx]. Every release pairs the .mim spec with its conformance TSV.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
META="$ROOT/khipro.meta"
RESOURCES="$ROOT/resources"
RAW="https://raw.githubusercontent.com"

MODE="${1:-latest}"
case "$MODE" in
  latest|beta|pinned) ;;
  *) echo "usage: sync.sh [latest|beta|pinned]" >&2; exit 2 ;;
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

# Newest release tag of a repo under <prefix>-vX.Y.Z. The beta mode also
# accepts prerelease suffixes (clsc-v37.0.2-beta), so a beta newer than the
# latest stable is picked up.
newest_tag() {
  local suffix=""
  [[ "$MODE" == "beta" ]] && suffix="(-[a-z0-9.]+)?"
  git ls-remote --tags --quiet "https://github.com/$1.git" \
    | awk '{print $2}' | sed 's|refs/tags/||' | grep -v '\^{}$' \
    | grep -E "^${2}-v[0-9]+\.[0-9]+\.[0-9]+${suffix}$" | sort -V | tail -1
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
    # Files are always re-fetched even when the version matches: upstream can
    # re-release a fixed beta under the same tag, and the post-regeneration
    # drift check is what decides whether anything actually changed.
    # A prerelease of the recorded base (37.0.1-beta vs 37.0.1) is newer work,
    # not a downgrade — compare base versions only.
    local version="${tag#${prefix}-v}"
    if [[ "$(printf '%s\n%s\n' "${current%%-*}" "${version%%-*}" | sort -V | tail -1)" != "${version%%-*}" ]]; then
      log "$name: remote $version is older than recorded $current — skipping"
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

  local version="${tag#${prefix}-v}"
  if [[ "$mode" == "pinned" ]]; then
    log "$name: pinned $tag"
  elif [[ "$version" == "$current" ]]; then
    log "$name: $version (re-fetched; content drift, if any, shows in the table check)"
    meta_set "$key" "$version"
  else
    meta_set "$key" "$version"
    log "$name: $current -> $version ($tag)"
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
