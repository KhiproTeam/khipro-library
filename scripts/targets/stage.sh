#!/usr/bin/env bash
# Per-target dist/ staging helpers.

# shellcheck source=../resources/meta.sh
source "$(dirname "${BASH_SOURCE[0]}")/../resources/meta.sh"

stage_c_sdk() {
  local dest="$1"
  mkdir -p "$dest/include/khipro"
  cp "$ROOT/core/include/khipro/khipro.h" "$dest/include/khipro/"
}

stage_android_sdk() {
  local abi="$1"
  local so="$2"
  local jni_dir="$DIST/android/jniLibs/$abi"
  mkdir -p "$jni_dir"
  cp "$so" "$jni_dir/libkhipro.so"

  local api_root="$DIST/android/api/com/khiproteam/khipro"
  if [[ ! -d "$api_root" ]]; then
    mkdir -p "$api_root"
    cp "$ROOT/bindings/android/library/src/main/kotlin/com/khiproteam/khipro/"*.kt "$api_root/"
  fi
}

stage_wasm_package() {
  local build_dir="$1"
  local out="$DIST/wasm"
  rm -rf "$out"
  mkdir -p "$out"

  cp "$build_dir/bindings/web/khipro.js" "$out/"
  cp "$build_dir/bindings/web/khipro.wasm" "$out/"
  cp "$ROOT/bindings/web/dist/index.js" "$out/"
  cp "$ROOT/bindings/web/dist/index.d.ts" "$out/"

  local name desc version patch
  name="$(meta_get name)"
  desc="$(meta_get description)"
  version="$(meta_get layout_version)"
  patch="$(meta_get library_patch)"

  cat >"$out/package.json" <<EOF
{
  "name": "@khiproteam/${name}",
  "version": "${version}-${patch}",
  "description": "${desc} (WebAssembly)",
  "type": "module",
  "main": "index.js",
  "types": "index.d.ts",
  "files": [
    "index.js",
    "index.d.ts",
    "khipro.js",
    "khipro.wasm"
  ],
  "sideEffects": false
}
EOF
}
