# Building from source

Skip the release archives and build locally when you need a debug build, a different ABI, or the latest unreleased layout.

## Two paths

Each target picks its sane default; flip any of them with a flag.

| Target | Default | Host needs | Flip with |
|---|---|---|---|
| `linux`, `macos`, `test`, `sync` | **Native** | CMake + C++17 toolchain | `--container` |
| `windows`, `android`, `wasm` | **Container** | `docker` **or** `podman` + `make` | `--native` |

## Quick start

```bash
git clone https://github.com/khiproteam/library.git khipro
cd khipro

make linux      # .so + .a + pkg-config in dist/linux/x86_64/ (native)
make windows    # .dll in dist/windows/x86_64/bin/ (container)
make wasm       # .js + .wasm in dist/wasm/ (container)
make android    # .aar in dist/android/ (container)
make test       # Build + all four conformance suites
make sync       # Pull layouts, regenerate spec tables, build + test
```

### What `make` actually invokes

```bash
./scripts/build.sh <target>     # target: linux | macos | windows | android | wasm | test | sync | all
```

Useful flags:

```bash
./scripts/build.sh linux --container   # build linux in a container (CI-identical)
./scripts/build.sh windows --native    # cross-compile on the host instead
./scripts/build.sh linux --quiet       # errors only
./scripts/build.sh linux --verbose     # per-step detail
```

Useful env:

```bash
KHIPRO_NO_SYNC=1   make linux        # skip the upstream layout sync (offline)
KHIPRO_DIST=/tmp/k make linux        # override output directory
KHIPRO_CONTAINER=podman make wasm   # force podman over docker
```

## Native builds

No container — you provide the toolchain.

### Common to all targets

- **CMake** ≥ 3.16
- A C++17 compiler (gcc ≥ 9, clang ≥ 10) — also used to build `tools/codegen`, which compiles the `.mim` layouts into `core/src/spec_generated.cpp` before CMake runs

There is no Python and no other interpreter in the build.

### Linux

```bash
sudo dnf install gcc-c++ cmake   # Fedora
sudo apt install g++ cmake       # Debian/Ubuntu

./scripts/build.sh linux
```

### Windows (cross-compile from Linux)

```bash
sudo dnf install mingw64-gcc-c++ cmake
sudo apt install mingw-w64 g++ cmake

./scripts/build.sh windows --native
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

./scripts/build.sh android --native
```

### Wasm

- **Emscripten** SDK (emsdk) — activate the latest: `emsdk install latest && emsdk activate latest && source ./emsdk_env.sh`

```bash
./scripts/build.sh wasm --native
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

`make test` builds natively and runs everything:

- **Conformance suites** — every case from the four `resources/khipro-testcases*.tsv` files, fed through the engine char-by-char (`feed_key` + stateless `convert` both verified per case)
- **Unit tests** — engine mark semantics and the POD spec-table loader (including malformed-table abort paths)

```bash
make test
# ...
# test: ok classic 104/104 · classic-touch 96/96 · dark 107/107 · dark-touch 99/99
# PASS 14 unit tests
```

For other platforms, the build step compiles the library but does not run tests — only Linux has a test harness today. Cross-platform verification in CI is "it builds and links."

## Syncing layouts

Each of the four layouts tracks its own upstream repo and version field in `khipro.meta`:

| Layout | Upstream | Tag scheme |
|---|---|---|
| Classic | [`rank-coder/khipro-m17n`](https://github.com/rank-coder/khipro-m17n) | `clsc-vX.Y.Z` |
| Classic touchscreen | [`KhiproTeam/khipro-mim-touchscreen`](https://github.com/KhiproTeam/khipro-mim-touchscreen) | `clsc-vX.Y.Z` |
| Dark | [`KhiproTeam/khipro-dark-m17n`](https://github.com/KhiproTeam/khipro-dark-m17n) | `drk-vX.Y.Z` |
| Dark touchscreen | [`KhiproTeam/khipro-dark-mim-touchscreen`](https://github.com/KhiproTeam/khipro-dark-mim-touchscreen) | `drk-vX.Y.Z` |

Every build first runs `scripts/resources/sync.sh` (stable track) to pull each layout's newest `X.Y.Z` release — the `.mim` spec **and** its paired conformance TSV. Then `tools/codegen` regenerates the spec tables; because emission is deterministic, any content change upstream shows up as a diff in `core/src/spec_generated.cpp`. Files are re-fetched even when versions match, so a re-tagged upstream fix is still detected.

Modes:

```bash
./scripts/resources/sync.sh latest    # stable track — what every build runs
./scripts/resources/sync.sh beta      # also picks up newer -beta prereleases
./scripts/resources/sync.sh pinned    # re-fetch exactly the versions khipro.meta records (release gate)
```

To skip syncing entirely (offline builds, fast iteration, building a specific git revision):

```bash
KHIPRO_NO_SYNC=1 make linux
```

A failed sync (e.g. offline) fails the build — use `KHIPRO_NO_SYNC=1` to opt out. Commit `resources/`, `khipro.meta`, and the regenerated `core/src/spec_generated.*` together when a sync changes them.

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
| `No podman/docker found` on a cross target | Install one, or use `--native` with the per-target toolchain. |
| `see /tmp/khipro-sync.log` on every build | You're offline and sync can't reach GitHub. Set `KHIPRO_NO_SYNC=1`. |
| `ANDROID_NDK_HOME must point to the Android NDK` | Native Android build without NDK. Export the env var or use the container path. |
| Container build fails on SELinux (Fedora) | Already handled via `:Z` mount flag — file a bug if it still breaks. |
| Wasm build can't find `emcc` | Run `source ./emsdk_env.sh` in the shell before invoking `make wasm --native`. |
| `corrupted spec tables` at runtime | The compiled-in tables don't match the loader — rebuild; if it persists, file a bug with the `khipro.meta` versions. |
