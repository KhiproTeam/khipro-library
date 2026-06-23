# khipro

[![Release](https://github.com/khiproteam/library/actions/workflows/release.yml/badge.svg)](https://github.com/khiproteam/library/actions/workflows/release.yml)
[![Release](https://img.shields.io/github/v/release/khiproteam/library)](https://github.com/khiproteam/library/releases)

Cross-platform C++ library that implements the **Khipro compositional Bangla input method**. Two layout variants are bundled:

- **Default** — desktop keyboard layout, sourced from [`rank-coder/khipro-m17n`](https://github.com/rank-coder/khipro-m17n)
- **Touchscreen** — phone/tablet variant, sourced from [`KhiproTeam/khipro-mim-touchscreen`](https://github.com/KhiproTeam/khipro-mim-touchscreen)

Both `.mim` specs follow the [m17n](https://www.nongnu.org/m17n/) format and are embedded into the library at build time.

## Build

```bash
make linux     # native Linux static + shared lib
make windows   # cross-compile Windows DLL (via MinGW container)
make android   # cross-compile Android .so per ABI
make wasm      # WebAssembly module
make test      # Linux build + conformance tests
```

All builds run inside Docker containers (see [`container/`](container/)) so the host needs only `docker` and `make`. The Android AAR build (with Kotlin bindings) additionally needs the Android SDK/NDK and Gradle installed — see [`scripts/targets/android.sh`](scripts/targets/android.sh).

## Layout

```
core/         C++ library: engine, m17n parser, C ABI
bindings/     Language bindings (Android Kotlin, Web JS/Wasm)
container/    Dockerfiles for cross-platform builds
cmake/        CMake helper modules
resources/    Bundled bn-khipro.mim specs + .lock (fetch provenance)
tests/        Conformance tests against khipro-testcases CSV
scripts/      Build orchestration, m17n sync, codegen
khipro.meta   Single source of truth for package metadata + version
```

## Versioning

`library_version = ${layout_version}-${library_patch}` — for example `35.0.1-1`.

- `layout_version` tracks the upstream m17n tag (e.g. `v35.0.1`). Bumps when a new m17n release is pulled in.
- `library_patch` counts library-only changes (build fixes, ABI tweaks, etc.). Resets to `0` when `layout_version` bumps.

Both fields live in [`khipro.meta`](khipro.meta); CMake, Python codegen, and bash scripts all read from there.

## Daily release

A scheduled CI job runs daily, checks the upstream m17n repositories for a new stable tag, and if one exists:

1. Pulls the new `.mim` files
2. Bumps `layout_version`, resets `library_patch=0`
3. Rebuilds all platforms and runs conformance tests
4. On success: commits, tags `v${layout_version}-0`, and publishes a GitHub release
5. On failure: nothing is published — the repo stays on the previous version

Library-only patch releases (bumping `library_patch` without an m17n change) are done manually by editing `khipro.meta` and pushing a tag.

## License

MIT License — see [LICENSE](LICENSE).
