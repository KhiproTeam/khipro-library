# Linux integration

## Archive contents

```
khipro-<ver>-linux-x86_64/
└── linux/x86_64/
    ├── include/khipro/khipro.h
    └── lib/
        ├── libkhipro.so          # shared (use this)
        ├── libkhipro.a           # static
        └── pkgconfig/khipro.pc
```

## Install

```bash
sudo tar -xzf khipro-*-linux-x86_64.tar.gz --strip-components=2 -C /usr/local
sudo ldconfig
```

Verifies with `pkg-config --modversion khipro`.

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

Build:

```bash
gcc app.c $(pkg-config --cflags --libs khipro) -o app
```

## Static link

```bash
gcc app.c $(pkg-config --cflags khipro) \
    /usr/local/lib/libkhipro.a -lpthread -o app
```

## CMake consumer

```cmake
find_package(khipro CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE khipro::khipro)
```

## GTK / Qt

- **GTK**: in your `GtkIMContext` subclass, call `khipro_feed_key` from `filter_keypress`, render `preedit` via `gtk_im_context_set_preedit_string`, and emit `commit` for `committed_delta`.
- **Qt**: same shape inside `QPlatformInputContext::filterEvent`. Use `setAttribute(QInputMethodEvent::TextFormat, ...)` for the preedit highlight.

## Notes

- `.so` ships with SONAME `libkhipro.so` — drop in `/usr/local/lib` and let `ldconfig` handle it.
- No runtime dependencies beyond glibc and libstdc++.
