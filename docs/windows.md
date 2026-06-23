# Windows integration

## Archive contents

```
khipro-<ver>-windows-x86_64/
└── windows/x86_64/
    ├── include/khipro/khipro.h
    ├── bin/
    │   └── khipro.dll              # ship next to your .exe
    └── lib/
        ├── libkhipro.dll.a         # MinGW import lib
        └── libkhipro.a             # static (MinGW)
```

The DLL is cross-compiled with MinGW and works with both MinGW and MSVC consumers.

## Layout at runtime

Put `khipro.dll` next to your executable:

```
MyApp.exe
khipro.dll
```

No system install. The Windows build does not use `pkg-config`.

## Minimal C example (MinGW)

```c
#include <khipro/khipro.h>
#include <stdio.h>

int main(void) {
    khipro_engine* e = khipro_create(KHIPRO_VARIANT_DEFAULT);

    khipro_result r = khipro_feed_key(e, "k");
    printf("preedit: %s\n", r.preedit);

    r = khipro_commit(e);
    printf("delta : %s\n", r.committed_delta);

    khipro_destroy(e);
    return 0;
}
```

Build:

```powershell
gcc app.c -Iwindows/x86_64/include -Lwindows/x86_64/lib -lkhipro -o app.exe
```

## MSVC consumer

The MinGW-built `.dll.a` will not link directly with `link.exe`. Two options:

1. **Generate an import lib from the DLL** (recommended):
   ```powershell
   gendef windows\x86_64\bin\khipro.dll
   dlltool -d khipro.def -l khipro.lib -k
   ```
   Then `link /LIBPATH:. khipro.lib app.obj`.

2. **Use `LoadLibrary` + `GetProcAddress`** at runtime — no import lib needed.

For broad MSVC support, set `KHIPRO_USE_SHARED` when including the header so the declarations use `__declspec(dllimport)`:

```c
#define KHIPRO_USE_SHARED
#include <khipro/khipro.h>
```

## Static link

Link `libkhipro.a` directly with MinGW. Add `-lstdc++ -lwinpthread` to satisfy dependencies.

## TSF (Text Services Framework)

For a proper Windows IME, implement `ITfKeyEventSink` and route `TestKeyDown`/`KeyDown` to `khipro_feed_key`. Use `ITfContextComposition::MakeStartComposition` for the preedit span and `ITfRange::SetText` for the committed delta.

## Notes

- The DLL is x86_64 only — no x86 or ARM builds today.
- No C++ runtime DLLs need shipping; static libstdc++ is used.
