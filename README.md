# khipro

[![Release](https://github.com/khiproteam/library/actions/workflows/release.yml/badge.svg)](https://github.com/khiproteam/library/actions/workflows/release.yml)
[![Release](https://img.shields.io/github/v/release/khiproteam/library)](https://github.com/khiproteam/library/releases)

Cross-platform C++ library that implements the **Khipro compositional Bangla input method**. Four layout variants are bundled:

- **Classic** — desktop keyboard layout, sourced from [`rank-coder/khipro-m17n`](https://github.com/rank-coder/khipro-m17n)
- **Classic touchscreen** — phone/tablet variant, sourced from [`KhiproTeam/khipro-mim-touchscreen`](https://github.com/KhiproTeam/khipro-mim-touchscreen)
- **Dark** — dark-grammar variant, sourced from [`KhiproTeam/khipro-dark-m17n`](https://github.com/KhiproTeam/khipro-dark-m17n)
- **Dark touchscreen** — dark variant for touch devices, sourced from [`KhiproTeam/khipro-dark-mim-touchscreen`](https://github.com/KhiproTeam/khipro-dark-mim-touchscreen)

All four `.mim` specs follow the [m17n](https://www.nongnu.org/m17n/) format. They are compiled into generated C++ tables at build time (`tools/codegen` → `core/src/spec_generated.cpp`); the shipped library contains data only — no runtime parser.

## Build

```bash
make linux     # Linux static + shared lib (native by default)
make windows   # cross-compile Windows DLL (container by default)
make android   # cross-compile Android .so per ABI (container by default)
make wasm      # WebAssembly module (container by default)
make test      # Build + all four conformance suites
make sync      # Pull layouts, regenerate spec tables, build + test
```

Linux builds run natively on the host; cross targets (windows/android/wasm) run in containers (see [`container/`](container/)) — swap any target with `--native` / `--container`. The Android AAR build (with Kotlin bindings) additionally needs the Android SDK/NDK and Gradle — see [`scripts/targets/android.sh`](scripts/targets/android.sh).

## Layout

```
core/         C++ library: engine, generated spec tables, C ABI
tools/        codegen: compiles .mim specs into C++ tables at build time
bindings/     Language bindings (Android Kotlin, Web JS/Wasm)
container/    Dockerfiles for cross-platform builds
cmake/        CMake helper modules
resources/    Four bundled .mim specs + four conformance TSVs
tests/        Conformance suites per layout + engine unit tests
scripts/      Build orchestration, layout sync
khipro.meta   Single source of truth for package metadata + versions
```

## Versioning

Five independent versions live in [`khipro.meta`](khipro.meta):

- `library_version` — this library's own release line (e.g. `2.0.0`); releases tag as `lib-v<library_version>`
- `khipro_classic_version`, `khipro_touchscreen_classic_version`, `khipro_dark_version`, `khipro_touchscreen_dark_version` — each tracks its upstream layout's release (e.g. `clsc-v37.0.1`, `drk-v1.1.0`), independently of the others

Every build compiles the four layouts from `resources/*.mim` via `tools/codegen`; the versions are stamped into `core/src/spec_generated.h`. CMake and the bash scripts all read from `khipro.meta`.

## Release

A scheduled CI job runs daily:

1. Syncs each layout from its upstream repo — newest **stable** release by default, newest of any kind on the beta track (see `scripts/resources/sync.sh latest|beta|pinned`)
2. Regenerates the spec tables and verifies they reproduce byte-identically from the fetched inputs
3. Rebuilds all platforms and runs the conformance suites
4. On success: commits, tags `lib-v<library_version>`, and publishes a GitHub release (beta layout versions are marked as prereleases)
5. On failure: nothing is published — the repo stays on the previous version

Library-only releases (bumping `library_version` without a layout change) are done manually by editing `khipro.meta` and pushing the `lib-v…` tag.

## License

MIT License — see [LICENSE](LICENSE).
