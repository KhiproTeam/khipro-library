#!/usr/bin/env bash
# Per-target dist/ staging helpers.

# shellcheck source=../resources/meta.sh
source "$(dirname "${BASH_SOURCE[0]}")/../resources/meta.sh"

stage_c_sdk() {
  local dest="$1"
  mkdir -p "$dest/include/khipro"

  # Stamp version macros into the distributed header. The source header stays
  # build-agnostic (the library itself receives these as compile definitions);
  # SDK consumers get the released values inline, with #ifndef guards so they
  # can still override. Same stage-time stamping as the wasm package.json.
  local library_version classic touchscreen dark dark_touch
  library_version="$(meta_get library_version)"
  classic="$(meta_get khipro_classic_version)"
  touchscreen="$(meta_get khipro_touchscreen_classic_version)"
  dark="$(meta_get khipro_dark_version)"
  dark_touch="$(meta_get khipro_touchscreen_dark_version)"
  awk -v libver="$library_version" -v classic="$classic" -v touch="$touchscreen" \
      -v dark="$dark" -v darktouch="$dark_touch" '
    { print }
    /^#define KHIPRO_H$/ && !stamped {
      print ""
      print "/* Version macros - stamped by the khipro-library build from khipro.meta. */"
      print "#ifndef KHIPRO_LIBRARY_VERSION"
      print "#define KHIPRO_LIBRARY_VERSION \"" libver "\""
      print "#endif"
      print "#ifndef KHIPRO_CLASSIC_VERSION"
      print "#define KHIPRO_CLASSIC_VERSION \"" classic "\""
      print "#endif"
      print "#ifndef KHIPRO_TOUCHSCREEN_CLASSIC_VERSION"
      print "#define KHIPRO_TOUCHSCREEN_CLASSIC_VERSION \"" touch "\""
      print "#endif"
      print "#ifndef KHIPRO_DARK_VERSION"
      print "#define KHIPRO_DARK_VERSION \"" dark "\""
      print "#endif"
      print "#ifndef KHIPRO_TOUCHSCREEN_DARK_VERSION"
      print "#define KHIPRO_TOUCHSCREEN_DARK_VERSION \"" darktouch "\""
      print "#endif"
      stamped=1
    }
  ' "$ROOT/core/include/khipro/khipro.h" >"$dest/include/khipro/khipro.h"
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

  local name desc version
  name="$(meta_get name)"
  desc="$(meta_get description)"
  version="$(meta_get library_version)"

  cat >"$out/package.json" <<EOF
{
  "name": "@khiproteam/${name}",
  "version": "${version}",
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
