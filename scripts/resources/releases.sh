# Ordered upstream release listing and walk-slice helpers for the Khipro
# layout repos. Shared by sync.sh (walks) and the Channel Check workflow.
#
# Release order: base version ascending; within a base, the beta release
# precedes the final (37.0.2-beta ships before 37.0.2). Plain `sort -V`
# cannot express that second rule, hence the explicit kind key.
#
# All functions print tab-separated "<version>\t<kind>" lines (kind:
# beta|final) unless noted. No side effects; safe to source.

# ordered_versions <list-file>          -> versions only, release order
ordered_versions() {
  cut -f1 "$1"
}

# newest_of_kind <list-file> <final|beta|any>  -> version or ""
newest_of_kind() {
  awk -F'\t' -v kind="$2" 'kind == "any" || $2 == kind { v = $1 } END { print v }' "$1"
}

# slice_after <list-file> <pointer>     -> entries strictly after pointer
# Empty pointer yields the whole list. A pointer that is not in the list
# (upstream deleted/renamed the tag) falls back to: every release whose
# base version is strictly newer than the pointer's base.
slice_after() {
  local list="$1" pointer="$2" base_p base v kind found=0
  if [[ -z "$pointer" ]]; then
    cat "$list"
    return
  fi
  base_p="${pointer%%-*}"
  # Pass 1: pointer present in the ordered list? Then it is a plain tail.
  while IFS=$'\t' read -r v kind; do
    [[ -z "$v" ]] && continue
    if [[ "$v" == "$pointer" ]]; then
      found=1
      break
    fi
  done <"$list"
  if (( found )); then
    awk -F'\t' -v p="$pointer" 'after { print $1 "\t" $2 } $1 == p { after = 1 }' "$list"
    return
  fi
  # Pass 2 (deleted/renamed pointer): keep releases that would come after it
  # in release order — newer bases, plus the final of the pointer's own base
  # when the pointer was that base's beta.
  while IFS=$'\t' read -r v kind; do
    [[ -z "$v" ]] && continue
    base="${v%%-*}"
    if [[ "$(printf '%s\n%s\n' "$base_p" "$base" | sort -V | tail -1)" == "$base" && "$base" != "$base_p" ]]; then
      printf '%s\t%s\n' "$v" "$kind"
    elif [[ "$base" == "$base_p" && "$pointer" == *-* && "$kind" == "final" ]]; then
      printf '%s\t%s\n' "$v" "$kind"
    fi
  done <"$list"
}

# ordered_from_tags <file-with-tag-names>  -> canonical "<version>\t<kind>" list
# Tag names carry the repo prefix (clsc-v37.0.2-beta); version and kind are
# derived here. Sort: base ascending (-V), beta before final within a base.
ordered_from_tags() {
  grep -E '^[0-9]+\.[0-9]+\.[0-9]+(-[a-z0-9.]+)?$' "$1" \
    | awk -F'-' '{ base = $1; kind = ($0 == base) ? "final" : "beta"; print base "\t" kind "\t" $0 }' \
    | sort -t"$(printf '\t')" -k1,1V -k2,2 -k3,3V \
    | awk -F'\t' '{ print $3 "\t" $2 }'
}

# fetch_ordered_releases <owner/repo> <prefix>  -> ordered list FILE path
# Queries the remote once; caller unlinks the file. Empty output = no
# matching releases (or repo unreachable).
fetch_ordered_releases() {
  local out
  out="$(mktemp)"
  git ls-remote --tags --quiet "https://github.com/$1.git" \
    | awk '{print $2}' \
    | sed 's|refs/tags/||' \
    | grep -v '\^{}$' \
    | grep -E "^${2}-v[0-9]+\.[0-9]+\.[0-9]+(-[a-z0-9.]+)?$" \
    | sed -E "s/^${2}-v//" \
    | ordered_from_tags /dev/stdin >"$out"
  echo "$out"
}
