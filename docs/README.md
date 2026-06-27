# Khipro integration docs

Each release on the [releases page](https://github.com/khiproteam/library/releases) ships five prebuilt bundles — pick yours and follow its doc:

| Bundle | What's inside | Doc |
|---|---|---|
| `khipro-<ver>-linux-x86_64.tar.gz` | `.so` + `.a` + `khipro.pc` + headers | [linux.md](linux.md) |
| `khipro-<ver>-macos-universal.tar.gz` | universal `.dylib` + `.a` + headers | [macos.md](macos.md) |
| `khipro-<ver>-windows-x86_64.zip` | `khipro.dll` + import lib + headers | [windows.md](windows.md) |
| `khipro-<ver>-android.aar` | Per-ABI `.so` (4 ABIs) + Kotlin bindings | [android.md](android.md) |
| `khipro-<ver>-wasm.zip` | `khipro.js` + `khipro.wasm` + ESM wrapper + types | [wasm.md](wasm.md) |

Building from source instead? See [build-from-source.md](build-from-source.md) — same layout, same versions, but you bring the toolchain.

## API parity

Every platform binding exposes the same nine functions with the same semantics. Field names match modulo language case (`committed_delta` ↔ `committedDelta`).

| Function | C ABI | Android Kotlin | Wasm TypeScript |
|---|---|---|---|
| create | `khipro_create(variant)` | `KhiproEngine.create(variant)` | `KhiproEngine.create(mod, variant)` → Promise |
| destroy | `khipro_destroy(engine)` | `engine.close()` | `engine.destroy()` |
| feed_key | `khipro_feed_key(eng, key)` | `engine.feedKey(key)` | `engine.feedKey(key)` |
| backspace | `khipro_backspace(eng)` | `engine.backspace()` | `engine.backspace()` |
| commit | `khipro_commit(eng)` | `engine.commit()` | `engine.commit()` |
| reset | `khipro_reset(eng)` | `engine.reset()` | `engine.reset()` |
| convert | `khipro_convert(variant, ascii, out, cap)` | `Khipro.convert(ascii, variant)` | `khiproConvert(mod, ascii, variant)` |
| layout version | `khipro_version()` | `Khipro.layoutVersion` | `Khipro.layoutVersion` |
| library version | `khipro_library_version()` | `Khipro.libraryVersion` | `Khipro.libraryVersion` |
| host-input wrapper | — (consumer wires GTK/Qt/TSF directly) | `KhiproIme` (wraps `InputConnection`) | `attachKhiproInput` (wraps DOM events) |

### Result struct fields

All three platforms return a struct with the same four fields:

| Field | C | Kotlin | TS |
|---|---|---|---|
| Newly committed text | `committed_delta` (char*) | `committedDelta: String` | `committedDelta: string` |
| Uncommitted preview | `preedit` (char*) | `preedit: String` | `preedit: string` |
| Full session committed | `committed` (char*) | `committed: String` | `committed: string` |
| Did khipro take the key | `consumed` (int 0/1) | `consumed: Boolean` | `consumed: boolean` |

### Variant enum

| Value | C | Kotlin | TS |
|---|---|---|---|
| Desktop layout | `KHIPRO_VARIANT_DEFAULT = 0` | `KhiproVariant.DEFAULT` | `'default'` |
| Touch layout | `KHIPRO_VARIANT_TOUCHSCREEN = 1` | `KhiproVariant.TOUCHSCREEN` | `'touchscreen'` |

## Intentional differences across platforms

These four differences are language idioms, not bugs. Don't try to unify them — each is the natural way to express the behavior in its host language.

### 1. `create` failure reporting

- **C** returns `NULL`. Consumers must check.
- **Kotlin** throws `IllegalArgumentException` via `require(handle != 0L)`.
- **TypeScript** throws `Error('khipro_create failed')`.

**As a consumer:** in C, write `if (!engine) { ... }`. In Kotlin/TS, wrap in try/catch if you need to recover.

### 2. `convert` placement

- **C** — free function: `khipro_convert(variant, ascii, out, cap)`.
- **Kotlin** — static method on `object Khipro`: `Khipro.convert(ascii, variant)`. Also available as an instance method `engine.convert(ascii)` which uses the engine's variant.
- **TypeScript** — async free function (module loading is async on Wasm): `await khiproConvert(mod, ascii, variant)`.

**As a consumer:** use whichever shape fits your call site. The semantics are identical.

### 3. Kotlin-only `engine.preedit` / `engine.committed` accessors

`KhiproEngine` exposes `committed` and `preedit` as instance properties that read the most recent `KhiproResult` ([KhiproEngine.kt:9-10](../bindings/android/library/src/main/kotlin/com/khiproteam/khipro/KhiproEngine.kt#L9)). C and Wasm don't have an equivalent — consumers capture the returned `Result` and read fields from it.

**Why:** Kotlin property syntax is idiomatic. C returns structs by value (no "last result" to access), and adding accessors to Wasm would require extra cross-boundary calls.

**As a consumer:** on Kotlin, either form works (`engine.preedit` or `result.preedit`). On C/Wasm, always capture and read the `Result`.

### 4. Per-platform convenience wrapper

Each platform has a thin wrapper that adapts the engine to the host input model:

- **Android** — `KhiproIme` class wraps `KhiproEngine` + `InputConnection` ([KhiproIme.kt](../bindings/android/library/src/main/kotlin/com/khiproteam/khipro/KhiproIme.kt)).
- **Wasm** — `attachKhiproInput` function wraps `KhiproEngine` + DOM events (handles Backspace, Enter, Escape, blur).
- **Linux / Windows** — no wrapper. Consumers wire into `GtkIMContext` / `QPlatformInputContext` / `ITfKeyEventSink` directly. See [linux.md](linux.md) and [windows.md](windows.md).

**Why:** each host's input API is fundamentally different (Android `InputConnection`, DOM `Event`, GTK/Qt/TSF). A single shared wrapper wouldn't fit any of them.

## API model

```c
khipro_engine* khipro_create(khipro_variant variant);
void           khipro_destroy(khipro_engine* engine);
khipro_result  khipro_feed_key(khipro_engine* engine, const char* utf8_key);
khipro_result  khipro_backspace(khipro_engine* engine);
khipro_result  khipro_commit(khipro_engine* engine);   // flush preedit → committed
void           khipro_reset(khipro_engine* engine);    // clear state
int            khipro_convert(khipro_variant v, const char* ascii, char* out, int cap);
```

### Lifecycle

```
create → feed_key × N → commit (or reset) → destroy
```

One engine = one input field's session. Call `destroy` on field blur; `reset` on input cancel.

### Interpreting `khipro_result`

| Field | What the UI does |
|---|---|
| `committed_delta` | Append to the document buffer immediately. |
| `preedit` | Render inline at the cursor. **Clear** the previous preedit before drawing the new one (the engine does not retain it). |
| `committed` | Full session total — ignore unless you need a snapshot. |
| `consumed` | If `0`, forward the raw keystroke to the host (IME is inactive for this key). |

Note: `Engine::feed_key` always returns `consumed=true`. Only `backspace` past empty state returns `consumed=false`. So `consumed=false` is mostly a "nothing happened" signal.

### Thread safety

A single `khipro_engine*` is **not** thread-safe. Create one per input field. Different engines in different threads are fine — the library has no globals after `create`.

### Stateless one-shot: `convert`

When you do not need preedit or backspace (batch transcription, clipboard conversion), skip the engine entirely:

```c
char out[256];
khipro_convert(KHIPRO_VARIANT_DEFAULT, "ka", out, sizeof(out));
// out == "কা"
```
