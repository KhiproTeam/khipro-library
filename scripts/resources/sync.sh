#!/usr/bin/env bash
# Dev-mode resource sync: pull the four layouts (spec + conformance TSVs)
# from each layout repository's main branch.
#
# Tag/release channel tracking is deferred until the upstream repos cut
# releases that pair a spec with its tests; until then this sync refreshes
# from main. Runs before every build via scripts/targets/common.sh.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
RESOURCES="$ROOT/resources"
RAW="https://raw.githubusercontent.com"

log() { printf 'sync: %s\n' "$*" >&2; }

fetch() { # <owner/repo> <ref> <src-name> <dest-name>
  local repo="$1" ref="$2" src="$3" dest="$4"
  if ! curl -fsSL --retry 3 --retry-delay 2 "$RAW/$repo/$ref/$src" -o "$RESOURCES/$dest"; then
    log "download failed: $repo/$ref/$src"
    return 1
  fi
  if [[ ! -s "$RESOURCES/$dest" ]]; then
    log "empty download: $repo/$ref/$src"
    return 1
  fi
  log "$dest <- $repo/$ref/$src"
}

if [[ "${KHIPRO_NO_SYNC:-}" == "1" ]]; then
  log "skipped (KHIPRO_NO_SYNC=1)"
  exit 0
fi

mkdir -p "$RESOURCES"

failed=0
fetch rank-coder/khipro-m17n main bn-khipro-classic.mim bn-khipro-classic.mim || failed=1
fetch KhiproTeam/khipro-mim-touchscreen main bn-khipro-classic.mim bn-khipro-classic-touchscreen.mim || failed=1
fetch KhiproTeam/khipro-dark-m17n main bn-khipro-dark.mim bn-khipro-dark.mim || failed=1
fetch KhiproTeam/khipro-dark-mim-touchscreen main bn-khipro-dark.mim bn-khipro-dark-touchscreen.mim || failed=1

fetch rank-coder/khipro-m17n main khipro-testcases.tsv khipro-testcases.tsv || failed=1
fetch KhiproTeam/khipro-mim-touchscreen main khipro-testcases.tsv khipro-testcases-touch.tsv || failed=1
fetch KhiproTeam/khipro-dark-m17n main khipro-testcases.tsv khipro-testcases-dark.tsv || failed=1
fetch KhiproTeam/khipro-dark-mim-touchscreen main khipro-testcases.tsv khipro-testcases-dark-touch.tsv || failed=1

if [[ "$failed" -ne 0 ]]; then
  log "some downloads failed; keeping the existing files where present"
fi
exit 0
