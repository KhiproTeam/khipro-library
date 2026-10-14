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

Three lines, each pulling from the four upstream layout repos — a daily CI check dispatches only the lines that have work:

| Line | Walks | Publishes |
|---|---|---|
| `develop` | everything — every release, any kind, **one commit per release**, oldest first | nothing (it's the union ledger) |
| `main` | finals only | `lib-vX.Y.Z` — full release, "Latest" |
| `beta` | betas only | `lib-vX.Y.Z-beta` — prerelease |

Each release-line walk takes one upstream release per commit, regenerates the spec tables, and gates on the full conformance suites before pushing — a broken upstream release stops its line instead of reaching consumers. Because each line follows only its own channel, purity is structural: main never contains a beta layout, beta never contains a final, and neither line ever moves backward. A single layout updating releases on its own; the other three ride along unchanged.

`library_version` is bumped per release on the release lines (`2.0.1`, `2.0.2-beta`, …); the four layout versions in [`khipro.meta`](khipro.meta) track their upstreams independently. Library-only releases (code changes without a layout change) land on main and release on the next push; the beta line picks code up via a periodic merge of main.

See `scripts/resources/sync.sh fetch|develop|stable|beta|pinned` and `.github/workflows/` for the machinery.

## License

MIT License — see [LICENSE](LICENSE).
