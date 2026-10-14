#!/usr/bin/env bash
# Layout sync, driven by the per-layout versions in khipro.meta.
#
#   sync.sh fetch    Build helper: pull each layout's newest STABLE release
#                    into the working tree. No commits, no pointer walking —
#                    this is what every build runs before compiling.
#   sync.sh develop  Develop-branch walk: one commit per upstream release
#                    newer than the recorded pointer (any kind, oldest
#                    first). Develop is the union line; nothing releases
#                    from it.
#   sync.sh stable   Main-branch walk: one commit per newer STABLE release,
#                    then a library_version patch bump. Releases cut from
#                    main are full/stable and never contain beta layouts.
#   sync.sh beta     Beta-branch walk: one commit per newer BETA release,
#                    then a library_version bump with a -beta suffix.
#                    Releases cut from this branch are prereleases and
#                    never contain final layouts.
#   sync.sh pinned   Release gate: fetch exactly the versions khipro.meta
#                    records. The committed spec tables must regenerate
#                    byte-identically or the release aborts.
#
# Tag scheme: classic layouts release as clsc-vX.Y.Z[-sfx], dark layouts as
# drk-vX.Y.Z[-sfx]. Every release pairs the .mim spec with its conformance
# TSV. The stable/beta walks must run on their own branch (enforced below).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
META="$ROOT/khipro.meta"
RESOURCES="$ROOT/resources"
RAW="https://raw.githubusercontent.com"

MODE="${1:-fetch}"
case "$MODE" in
  fetch|develop|stable|beta|pinned) ;;
  *) echo "usage: sync.sh [fetch|develop|stable|beta|pinned]" >&2; exit 2 ;;
esac

log() { printf 'sync: %s\n' "$*" >&2; }
meta_get() { grep -E "^$1=" "$META" | head -1 | cut -d= -f2-; }
meta_set() { sed -i "s/^$1=.*/$1=$2/" "$META"; }

# shellcheck source=releases.sh
. "$ROOT/scripts/resources/releases.sh"

fetch() { # <owner/repo> <tag> <src-path> <dest-path>
  curl -fsSL --retry 3 --retry-delay 2 "$RAW/$1/$2/$3" -o "$RESOURCES/$4" || return 1
  [[ -s "$RESOURCES/$4" ]] || return 1
}

# name repo tag-prefix mim-src mim-dest tsv-src tsv-dest meta-key
LAYOUTS=(
  "classic|rank-coder/khipro-m17n|clsc|bn-khipro-classic.mim|bn-khipro-classic.mim|khipro-testcases.tsv|khipro-testcases.tsv|khipro_classic_version"
  "classic_touch|KhiproTeam/khipro-mim-touchscreen|clsc|bn-khipro-classic.mim|bn-khipro-classic-touchscreen.mim|khipro-testcases.tsv|khipro-testcases-touch.tsv|khipro_touchscreen_classic_version"
  "dark|KhiproTeam/khipro-dark-m17n|drk|bn-khipro-dark.mim|bn-khipro-dark.mim|khipro-testcases.tsv|khipro-testcases-dark.tsv|khipro_dark_version"
  "dark_touch|KhiproTeam/khipro-dark-mim-touchscreen|drk|bn-khipro-dark.mim|bn-khipro-dark-touchscreen.mim|khipro-testcases.tsv|khipro-testcases-dark-touch.tsv|khipro_touchscreen_dark_version"
)

# Walk one repo: every release after the recorded pointer, one fetch + meta
# update per release. kind_filter: "any" (develop), "final" (stable) or
# "beta" (beta). staging: "meta" (develop ledger — one meta-pointer commit
# per release) or "none" (release lines — the caller commits meta + tables
# after the conformance gate passes; failure commits nothing). Increments
# WALKED.
walk_repo() { # <staging> <kind_filter> <name> <repo> <prefix> <mim_src> <mim_dest> <tsv_src> <tsv_dest> <key>
  local staging="$1" filter="$2" name="$3" repo="$4" prefix="$5"
  local mim_src="$6" mim_dest="$7" tsv_src="$8" tsv_dest="$9" key="${10}"
  local pointer list tmp tip version kind tag walked_here=0

  pointer="$(meta_get "$key")"
  list="$(fetch_ordered_releases "$repo" "$prefix")" || {
    log "$name: release listing failed ($repo)"
    return 1
  }
  tmp="$(mktemp)"
  if [[ "$filter" == "final" ]]; then
    awk -F'\t' '$2 == "final"' "$list" >"$tmp"
  elif [[ "$filter" == "beta" ]]; then
    awk -F'\t' '$2 == "beta"' "$list" >"$tmp"
  else
    cp "$list" "$tmp"
  fi

  local slice; slice="$(slice_after "$tmp" "$pointer")"
  if [[ -n "$slice" ]]; then
    while IFS=$'\t' read -r version kind; do
      [[ -z "$version" ]] && continue
      tag="${prefix}-v${version}"
      if ! fetch "$repo" "$tag" "$mim_src" "$mim_dest"; then
        log "$name: download failed ($repo@$tag/$mim_src)"
        return 1
      fi
      if ! fetch "$repo" "$tag" "$tsv_src" "$tsv_dest"; then
        log "$name: no paired TSV at $tag — keeping current testcases"
      fi
      meta_set "$key" "$version"
      walked_here=$((walked_here + 1))
      log "$name: took $tag"
      if [[ "$staging" == meta ]]; then
        git -C "$ROOT" add khipro.meta
        git -C "$ROOT" commit -q -m "layouts: ${name} ${tag}"
      fi
    done <<<"$slice"
  elif [[ -n "$pointer" ]]; then
    # Content-drift check: upstream may re-release under the same tag (a
    # fixed beta). Re-fetch the tip; the working tree decides whether
    # anything changed. A change is committed as a drift commit.
    tip="$(newest_of_kind "$tmp" "$filter")"
    if [[ -n "$tip" ]] && fetch "$repo" "${prefix}-v$tip" "$mim_src" "$mim_dest" \
       && fetch "$repo" "${prefix}-v$tip" "$tsv_src" "$tsv_dest"        && [[ -n "$(git -C "$ROOT" status --porcelain -- resources)" ]]; then
      walked_here=$((walked_here + 1))
      log "$name: $tip re-released with new content"
    fi
  fi
  rm -f "$list" "$tmp"
  WALKED=$((WALKED + walked_here))
}

bump_library() { # <stable|beta> -> prints new version; writes meta
  local version base patch
  version="$(meta_get library_version)"
  if ! [[ "$version" =~ ^([0-9]+\.[0-9]+)(\.([0-9]+))?(-beta)?$ ]]; then
    log "library_version ${version} is not auto-bumpable — bump manually"
    return 1
  fi
  base="${BASH_REMATCH[1]}"
  patch="${BASH_REMATCH[3]:-0}"
  if [[ "$1" == beta ]]; then
    meta_set library_version "${base}.$((patch + 1))-beta"
  else
    meta_set library_version "${base}.$((patch + 1))"
  fi
  meta_get library_version
}

if [[ "$MODE" == fetch || "$MODE" == pinned ]]; then
  # Build helper / release gate: stateless sync of the newest-stable (fetch)
  # or recorded (pinned) releases. No commits, no pointer walking.
  failed=0
  for layout in "${LAYOUTS[@]}"; do
    IFS='|' read -r name repo prefix mim_src mim_dest tsv_src tsv_dest key <<<"$layout"
    current="$(meta_get "$key")"
    if [[ "$MODE" == "pinned" ]]; then
      tag="${prefix}-v${current}"
    else
      list="$(fetch_ordered_releases "$repo" "$prefix")" || { log "$name: release listing failed"; failed=1; continue; }
      tag="${prefix}-v$(newest_of_kind "$list" final)"
      rm -f "$list"
      tag="${tag:-${prefix}-v${current}}"
    fi
    if [[ -z "$current" && "$MODE" == "pinned" ]]; then
      log "$name: khipro.meta missing $key"
      failed=1
      continue
    fi
    if fetch "$repo" "$tag" "$mim_src" "$mim_dest" && fetch "$repo" "$tag" "$tsv_src" "$tsv_dest"; then
      if [[ "$MODE" == "pinned" ]]; then log "$name: pinned $tag"; else log "$name: $tag"; fi
    else
      log "$name: download failed ($repo@$tag)"
      failed=1
    fi
  done
  [[ $failed -ne 0 ]] && { log "some layouts failed to sync"; exit 1; }
  exit 0
fi

# Branch walks. develop walks on develop; the stable walk lands on main
# (main is the stable release line); the beta walk on beta.
declare -A MODE_BRANCH=( [develop]=develop [stable]=main [beta]=beta )
if [[ "$MODE" != develop ]]; then
  expected="${MODE_BRANCH[$MODE]}"
  branch="$(git -C "$ROOT" branch --show-current)"
  if [[ "$branch" != "$expected" && "${KHIPRO_SYNC_NO_BRANCH_GUARD:-}" != 1 ]]; then
    log "$MODE walk must run on the $expected branch (on: ${branch:-detached})"
    exit 1
  fi
fi

mkdir -p "$RESOURCES"

WALKED=0
failed=0
for layout in "${LAYOUTS[@]}"; do
  IFS='|' read -r name repo prefix mim_src mim_dest tsv_src tsv_dest key <<<"$layout"
  if [[ "$MODE" == stable ]]; then
    walk_repo none final "$name" "$repo" "$prefix" "$mim_src" "$mim_dest" "$tsv_src" "$tsv_dest" "$key" || failed=1
  elif [[ "$MODE" == beta ]]; then
    walk_repo none beta "$name" "$repo" "$prefix" "$mim_src" "$mim_dest" "$tsv_src" "$tsv_dest" "$key" || failed=1
  else
    walk_repo meta any "$name" "$repo" "$prefix" "$mim_src" "$mim_dest" "$tsv_src" "$tsv_dest" "$key" || failed=1
  fi
done

if [[ $failed -ne 0 ]]; then
  log "some layouts failed to walk"
  exit 1
fi

# Version policy: the walk itself never touches library_version; one bump
# per sync that took content.
if [[ "$MODE" != develop && $WALKED -gt 0 ]]; then
  new_version="$(bump_library "$MODE")" || exit 1
  log "library_version bumped to ${new_version} (${WALKED} layout release(s) taken) — committed by the caller after the gate"
fi
