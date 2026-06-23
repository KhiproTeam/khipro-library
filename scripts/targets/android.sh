#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
source "$(dirname "${BASH_SOURCE[0]}")/stage.sh"
source "$(dirname "${BASH_SOURCE[0]}")/../resources/meta.sh"

: "${ANDROID_NDK_HOME:?ANDROID_NDK_HOME must point to the Android NDK}"

ABIS=(arm64-v8a armeabi-v7a x86 x86_64)
TOOLCHAIN="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake"
JOBS="$(nproc 2>/dev/null || echo 4)"

find_jdk() {
  if [[ -n "${JAVA_HOME:-}" && -x "$JAVA_HOME/bin/java" ]]; then
    echo "$JAVA_HOME"
    return
  fi
  if command -v java >/dev/null 2>&1; then
    readlink -f "$(dirname "$(command -v java)")/.."
    return
  fi
  for candidate in \
      /usr/lib/jvm/temurin-21-jdk \
      /usr/lib/jvm/java-21-openjdk \
      /usr/lib/jvm/java-17-openjdk; do
    if [[ -x "$candidate/bin/java" ]]; then
      echo "$candidate"
      return
    fi
  done
}

find_android_sdk() {
  if [[ -n "${ANDROID_HOME:-}" && -d "$ANDROID_HOME" ]]; then
    echo "$ANDROID_HOME"
    return
  fi
  if [[ -n "${ANDROID_SDK_ROOT:-}" && -d "$ANDROID_SDK_ROOT" ]]; then
    echo "$ANDROID_SDK_ROOT"
    return
  fi
}

build_aar() {
  if [[ "${KHIPRO_NO_AAR:-}" == "1" ]]; then
    skip "KHIPRO_NO_AAR=1"
    return 0
  fi

  local jdk sdk
  jdk="$(find_jdk || true)"
  sdk="$(find_android_sdk || true)"

  if [[ -z "$jdk" ]]; then
    skip "no JDK (set JAVA_HOME or install Temurin 17+)"
    return 0
  fi
  if [[ -z "$sdk" ]]; then
    skip "no Android SDK (set ANDROID_HOME)"
    return 0
  fi

  step "aar"
  local gradle_cmd
  if [[ -f "$ROOT/bindings/android/gradle/wrapper/gradle-wrapper.jar" ]] \
     && [[ -x "$ROOT/bindings/android/gradlew" ]]; then
    gradle_cmd="$ROOT/bindings/android/gradlew"
  elif command -v gradle >/dev/null 2>&1; then
    gradle_cmd="gradle"
  else
    fail "no gradle (run once: gradle wrapper --gradle-version 8.11.1 in bindings/android/)"
    return 1
  fi

  local version patch
  version="$(meta_get layout_version)"
  patch="$(meta_get library_patch)"

  local gradle_flags=(
    "--no-daemon"
    "-Dorg.gradle.internal.http.connectionTimeout=120000"
    "-Dorg.gradle.internal.http.socketTimeout=120000"
    "-Dorg.gradle.internal.repository.max.retries=3"
    "-Dorg.gradle.internal.repository.initial.backoff=2000"
  )

  if ! JAVA_HOME="$jdk" ANDROID_HOME="$sdk" \
       PATH="$jdk/bin:$sdk/cmdline-tools/latest/bin:$sdk/platform-tools:$PATH" \
       "$gradle_cmd" -p "$ROOT/bindings/android" "${gradle_flags[@]}" \
       :library:assembleRelease \
       >/tmp/khipro-aar.log 2>&1; then
    fail "see /tmp/khipro-aar.log"
    return 1
  fi

  local aar
  aar="$(ls "$ROOT/bindings/android/library/build/outputs/aar/"*.aar 2>/dev/null | head -1)"
  if [[ -z "$aar" || ! -f "$aar" ]]; then
    fail "no .aar produced"
    return 1
  fi

  local dest="$DIST/android/khipro-${version}-${patch}.aar"
  cp "$aar" "$dest"
  ok "$(basename "$dest")"
}

section "khipro" "android · ${ABIS[*]}"

sync_resources

step "clean"
rm -rf "$DIST/android"
ok

for abi in "${ABIS[@]}"; do
  step "build $abi"
  BUILD="$ROOT/build/android-$abi"
  rm -rf "$BUILD"
  cmake -S "$ROOT" -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DANDROID_ABI="$abi" \
    -DANDROID_PLATFORM=android-24 \
    -DANDROID_STL=c++_shared \
    -DKHIPRO_BUILD_SHARED=OFF \
    -DKHIPRO_BUILD_STATIC=ON \
    -DKHIPRO_BUILD_TESTS=OFF \
    -DCMAKE_SHARED_LINKER_FLAGS="-Wl,-z,max-page-size=16384" \
    >/dev/null 2>&1
  cmake --build "$BUILD" --target khipro_jni -j"$JOBS" >/dev/null 2>&1

  so="$BUILD/bindings/android/libkhipro.so"
  if [[ ! -f "$so" ]]; then
    so="$(find "$BUILD" -name 'libkhipro.so' -type f | head -1)"
  fi
  if [[ -z "$so" || ! -f "$so" ]]; then
    fail "libkhipro.so not found"
    exit 1
  fi
  stage_android_sdk "$abi" "$so"
  ok
done

step "stage"
ok "$DIST/android/"

build_aar

finish
