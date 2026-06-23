#!/usr/bin/env bash
# Read metadata from khipro.meta. Source this file: source meta.sh
# Usage:
#   meta_get <key>          - print value for key, return 1 if missing
#   meta_library_version    - print layout_version-library_patch

META_FILE="${META_FILE:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/khipro.meta}"

meta_get() {
  local key="$1"
  [[ -f "$META_FILE" ]] || return 1
  local value
  value=$(grep -E "^${key}=" "$META_FILE" | head -1 | cut -d= -f2-)
  [[ -z "$value" ]] && return 1
  echo "$value"
}

meta_library_version() {
  echo "$(meta_get layout_version)-$(meta_get library_patch)"
}
