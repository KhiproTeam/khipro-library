# Building from source

Skip the release archives and build locally when you need a debug build, a different ABI, or the latest unreleased m17n layout.

## Two paths

| Path | Host needs | Use when |
|---|---|---|
| **Container** (default) | `docker` **or** `podman` + `make` | Recommended. Identical to CI. |
| **Native** (`--native`) | Full toolchain per target | You can't run containers, or you're iterating fast on one platform. |

## Container path (default)

```bash
git clone https://github.com/khiproteam/library.git khipro
cd khipro

make linux      # .so + .a + pkg-config in dist/linux/x86_64/
make windows    # .dll in dist/windows/x86_64/bin/
make wasm       # .js + .wasm in dist/wasm/
make android    # .aar in dist/android/
make test       # Linux build + conformance cases (per-CSV count)
make all        # all four platforms
```

That's it. The Dockerfile (under `container/`) has the full toolchain for every target; the Makefile just dispatches to `scripts/build.sh`.

### What `make` actually invokes

```bash
./scripts/build.sh <target>     # target: linux | windows | android | wasm | test | all
```

Useful flags:

```bash
./scripts/build.sh linux --native    # skip container, run on host
./scripts/build.sh linux --quiet     # errors only
./scripts/build.sh linux --verbose   # per-step detail
```

Useful env:

```bash
KHIPRO_NO_SYNC=1   make linux      # skip the m17n upstream check (faster, offline)
KHIPRO_DIST=/tmp/k make linux      # override output directory
KHIPRO_CONTAINER=podman make linux # force podman over docker
```

## Native path (`--native`)

No container — you provide the toolchain.

### Common to all targets

- **CMake** ≥ 3.16
- **Python** ≥ 3.9 (codegen for `embedded_specs.h`)
- A C++17 compiler (gcc ≥ 9, clang ≥ 10)

### Linux

```bash
sudo dnf install gcc-c++ cmake python3   # Fedora
sudo apt install g++ cmake python3        # Debian/Ubuntu

KHIPRO_NO_SYNC=1 ./scripts/build.sh linux --native
```

### Windows (cross-compile from Linux)

```bash
sudo dnf install mingw64-gcc-c++ cmake
sudo apt install mingw-w64 g++ cmake

KHIPRO_NO_SYNC=1 ./scripts/build.sh windows --native
```

The Windows target always cross-compiles — there is no native-Windows build path today.

### Android

The heaviest native setup. You need:

- **Android NDK** r25+ — set `ANDROID_NDK_HOME`
- **Android SDK** with `build-tools` + `platforms;android-34` — set `ANDROID_HOME`
- **JDK** 17 — set `JAVA_HOME`
- **Gradle** 8+ (or use the wrapper at `bindings/android/gradlew`)

```bash
export ANDROID_NDK_HOME=/opt/android-ndk
export ANDROID_HOME=/opt/android-sdk
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk

KHIPRO_NO_SYNC=1 ./scripts/build.sh android --native
```

### Wasm

- **Emscripten** SDK (emsdk) — activate the latest `emsdk install latest && emsdk activate latest && source ./emsdk_env.sh`

```bash
KHIPRO_NO_SYNC=1 ./scripts/build.sh wasm --native
```

## Output layout

After a build, everything lands under `dist/`:

```
dist/
├── linux/x86_64/
│   ├── include/khipro/khipro.h
│   └── lib/{libkhipro.so, libkhipro.a, pkgconfig/khipro.pc}
├── windows/x86_64/
│   ├── include/khipro/khipro.h
│   ├── bin/khipro.dll
│   └── lib/{libkhipro.dll.a, libkhipro.a}
├── android/
│   ├── khipro-<ver>.aar
│   └── jniLibs/<abi>/libkhipro.so
└── wasm/
    ├── khipro.js, khipro.wasm
    ├── index.js, index.d.ts
    └── package.json
```

`dist/` is gitignored — it's purely build output.

## Tests

Conformance tests run as part of `make test` (Linux build + tests). They execute every case from `resources/khipro-testcases*.tsv` against the engine:

```bash
make test
# ...
# [ok] N cases   (one count per CSV — feed_key + convert each verified)
```

For other platforms, the build step compiles the library but does not run tests — only Linux has a test harness today. Cross-platform verification in CI is "it builds and links."

## Syncing layouts

By default every build first runs `scripts/resources/sync.sh` to check if the upstream m17n repos (`rank-coder/khipro-m17n`, `KhiproTeam/khipro-mim-touchscreen`) published a new tag. If they did, the build aborts with a "metadata out of date — run sync.sh" error.

To skip this check (for offline builds, fast iteration, or building a specific git revision):

```bash
KHIPRO_NO_SYNC=1 make linux
```

To force a refresh of the bundled `.mim` files and testcase CSVs:

```bash
./scripts/resources/fetch.sh
```

This regenerates `resources/bn-khipro*.mim`, `resources/khipro-testcases*.tsv`, `resources/.lock`, and `core/src/embedded_specs.h` (gitignored). Commit the first three if you're cutting a release manually; never commit `embedded_specs.h`.

## Clean

```bash
make clean      # wipes build/ build-*/ dist/
```

Per-target builds cache CMake state under `build-<target>/`. Deleting just one keeps the others:

```bash
rm -rf build-linux
```

## Troubleshooting

| Symptom | Fix |
|---|---|
| `No podman/docker found` | Install one, or use `--native`. |
| `metadata out of date` on every build | You're offline and sync can't reach GitHub. Set `KHIPRO_NO_SYNC=1`. |
| `ANDROID_NDK_HOME must point to the Android NDK` | Native Android build without NDK. Export the env var or use the container path. |
| Container build fails on SELinux (Fedora) | Already handled via `:Z` mount flag — file a bug if it still breaks. |
| Wasm build can't find `emcc` | Run `source ./emsdk_env.sh` in the shell before invoking `make wasm --native`. |
