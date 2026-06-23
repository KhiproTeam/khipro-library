# Multi-target build image on Debian bookworm-slim (~75 MB base).
#
# Build one target:
#   podman build -f container/Dockerfile --target linux -t khipro-build-linux .
#   podman build -f container/Dockerfile --target windows -t khipro-build-windows .
#   podman build -f container/Dockerfile --target android -t khipro-build-android .
#   podman build -f container/Dockerfile --target wasm -t khipro-build-wasm .

FROM docker.io/library/debian:bookworm-slim AS base

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      ca-certificates \
      cmake \
      curl \
      g++ \
      git \
      make \
      python3 \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# ---------------------------------------------------------------------------
FROM base AS linux

COPY scripts/targets/linux.sh scripts/targets/common.sh scripts/targets/stage.sh /src/scripts/targets/
RUN chmod +x /src/scripts/targets/linux.sh

ENTRYPOINT ["/src/scripts/targets/linux.sh"]

# ---------------------------------------------------------------------------
FROM base AS windows

RUN apt-get update \
 && apt-get install -y --no-install-recommends mingw-w64 \
 && rm -rf /var/lib/apt/lists/*

COPY cmake/toolchains/mingw-w64-64.cmake /src/cmake/toolchains/
COPY scripts/targets/windows.sh scripts/targets/common.sh scripts/targets/stage.sh /src/scripts/targets/
RUN chmod +x /src/scripts/targets/windows.sh

ENTRYPOINT ["/src/scripts/targets/windows.sh"]

# ---------------------------------------------------------------------------
FROM base AS android

# Full Android toolchain: NDK (for .so), JDK + Gradle + Android SDK (for AAR).
# Lets ./scripts/build.sh android produce both per-ABI .so AND the .aar in one
# container run, no external setup needed in CI.
ARG NDK_VERSION=r27c
ARG GRADLE_VERSION=8.11.1
ARG ANDROID_CMDLINE_TOOLS_VERSION=11076708
ARG ANDROID_COMPILE_SDK=35
ARG ANDROID_BUILD_TOOLS=35.0.0

ENV ANDROID_HOME=/opt/android-sdk
ENV ANDROID_SDK_ROOT=/opt/android-sdk
ENV ANDROID_NDK_HOME=/opt/android-ndk
ENV ANDROID_NDK_ROOT=/opt/android-ndk
ENV JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
ENV PATH="${JAVA_HOME}/bin:${ANDROID_HOME}/cmdline-tools/latest/bin:${ANDROID_HOME}/platform-tools:/opt/gradle-${GRADLE_VERSION}/bin:${PATH}"

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      openjdk-17-jdk-headless \
      unzip \
 && rm -rf /var/lib/apt/lists/* \
 # --- NDK ---
 && curl -fsSL "https://dl.google.com/android/repository/android-ndk-${NDK_VERSION}-linux.zip" \
      -o /tmp/ndk.zip \
 && unzip -q /tmp/ndk.zip -d /opt \
 && mv "/opt/android-ndk-${NDK_VERSION}" "$ANDROID_NDK_HOME" \
 && rm /tmp/ndk.zip \
 # --- Android SDK command-line tools ---
 && curl -fsSL "https://dl.google.com/android/repository/commandlinetools-linux-${ANDROID_CMDLINE_TOOLS_VERSION}_latest.zip" \
      -o /tmp/cmdline.zip \
 && mkdir -p "$ANDROID_HOME/cmdline-tools" \
 && unzip -q /tmp/cmdline.zip -d "$ANDROID_HOME/cmdline-tools" \
 && mv "$ANDROID_HOME/cmdline-tools/cmdline-tools" "$ANDROID_HOME/cmdline-tools/latest" \
 && rm /tmp/cmdline.zip \
 && yes | sdkmanager --licenses > /dev/null \
 && sdkmanager "platform-tools" "build-tools;${ANDROID_BUILD_TOOLS}" "platforms;android-${ANDROID_COMPILE_SDK}" \
 # --- Gradle ---
 && curl -fsSL "https://services.gradle.org/distributions/gradle-${GRADLE_VERSION}-bin.zip" \
      -o /tmp/gradle.zip \
 && unzip -q /tmp/gradle.zip -d /opt \
 && rm /tmp/gradle.zip

COPY scripts/targets/android.sh scripts/targets/common.sh scripts/targets/stage.sh /src/scripts/targets/
RUN chmod +x /src/scripts/targets/android.sh

ENTRYPOINT ["/src/scripts/targets/android.sh"]

# ---------------------------------------------------------------------------
FROM base AS wasm

ARG EMSDK_VERSION=6.0.1

ENV EMSDK=/opt/emsdk
ENV PATH="${EMSDK}/upstream/emscripten:${PATH}"

RUN apt-get update \
 && apt-get install -y --no-install-recommends nodejs npm xz-utils \
 && rm -rf /var/lib/apt/lists/* \
 && git clone --depth 1 https://github.com/emscripten-core/emsdk.git "$EMSDK" \
 && "$EMSDK/emsdk" install "${EMSDK_VERSION}" \
 && "$EMSDK/emsdk" activate "${EMSDK_VERSION}"

COPY scripts/targets/wasm.sh scripts/targets/common.sh scripts/targets/stage.sh /src/scripts/targets/
RUN chmod +x /src/scripts/targets/wasm.sh

ENTRYPOINT ["/src/scripts/targets/wasm.sh"]
