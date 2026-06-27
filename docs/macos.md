# macOS integration

## Archive contents

```
khipro-<ver>-macos-universal/
└── macos/universal/
    ├── include/khipro/khipro.h
    └── lib/
        ├── libkhipro.dylib       # shared, universal arm64+x86_64 (use this)
        └── libkhipro.a           # static
```

The release artifact is a **universal** binary (Apple Silicon + Intel in one
file). Verify with `lipo -info lib/libkhipro.dylib`.

> Downloaded from a release, the dylib is quarantined by Gatekeeper. Clear it
> once before linking:
> ```bash
> xattr -d com.apple.quarantine lib/libkhipro.dylib
> ```

## Install

No system install required — point the compiler at the unpacked `include` and
`lib`. To install system-wide anyway:

```bash
sudo cp lib/libkhipro.dylib /usr/local/lib/
sudo cp -r include/khipro    /usr/local/include/
```

The Apple build does not use `pkg-config`.

## Minimal C example

```c
#include <khipro/khipro.h>
#include <stdio.h>

int main(void) {
    khipro_engine* e = khipro_create(KHIPRO_VARIANT_DEFAULT);

    khipro_result r = khipro_feed_key(e, "k");
    printf("preedit: %s\n", r.preedit);          // ক
    printf("delta : %s\n", r.committed_delta);   // (empty — still composing)

    r = khipro_feed_key(e, "a");
    printf("delta : %s\n", r.committed_delta);   // কা (composed)

    khipro_commit(e);                             // flush any remaining preedit
    khipro_destroy(e);
    return 0;
}
```

Build (shared):

```bash
clang app.c -Iinclude -Llib -lkhipro -Wl,-rpath,@loader_path/lib -o app
```

The dylib's install name is `@rpath/libkhipro.dylib`, so the consumer decides
where it lives via `-rpath`. The `@loader_path/lib` above resolves relative to
the executable — drop `libkhipro.dylib` in a `lib/` folder next to `app`, or
adjust the rpath to wherever you ship it.

## Static link

```bash
clang app.c -Iinclude lib/libkhipro.a -lc++ -o app
```

`-lc++` (not `-lstdc++`) — macOS uses libc++. No other runtime dependencies.

## App bundle layout

For a `.app`, put the dylib in `Contents/Frameworks` and link with an rpath
relative to the executable:

```bash
clang app.c -Iinclude -Llib -lkhipro \
    -Wl,-rpath,@loader_path/../Frameworks -o MyApp
# then copy libkhipro.dylib into MyApp.app/Contents/Frameworks/
```

Code-sign the dylib along with the rest of the bundle (`codesign --deep` or
sign each Mach-O individually) before distribution/notarization.

## NSTextInputClient

There's no shipped wrapper for macOS (same as Linux/Windows — see the table in
[README.md](README.md)). Wire the engine into your `NSTextInputClient` /
`IMKInputController`:

- Call `khipro_feed_key` from `interpretKeyEvents:` / `inputText:`.
- Render `preedit` via `setMarkedText:` (clear the previous marked text first —
  the engine does not retain it).
- Insert `committed_delta` via `insertText:`.
- If `consumed == 0`, forward the raw key to the system.

## Building from source

On a Mac with Xcode Command Line Tools + CMake:

```bash
make macos                              # arm64 or x86_64 (host arch)
KHIPRO_MACOS_ARCH=universal make macos  # fat arm64+x86_64 (what releases ship)
KHIPRO_MACOS_ARCH=x86_64  make macos    # force Intel
```

Output lands in `dist/macos/<arch>/`. The macOS target is host-only — it has no
container/cross-compile path, so it always runs natively on the Mac.

## Notes

- Requires macOS 11+ (universal slices target arm64 + x86_64).
- The dylib has no dependencies beyond the system libc++ and libSystem.
