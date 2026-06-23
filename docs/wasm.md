# Web / WebAssembly integration

## Archive contents

```
khipro-<ver>-wasm/
├── khipro.js         # Emscripten glue
├── khipro.wasm       # ~500 KB
├── index.js          # high-level wrapper (ESM)
├── index.d.ts        # TypeScript types
└── package.json
```

Ship all five files together from the same directory. The wrapper loads `khipro.wasm` relative to `khipro.js`, so they must stay co-located.

## Install

**Option A — local files:**

```html
<script type="module">
  import { loadKhiproModule, KhiproEngine } from "./index.js";

  const createModule = await loadKhiproModule("./khipro.js");
  const engine = await KhiproEngine.create(createModule, "default");

  const r = engine.feedKey("k");
  console.log(r.preedit);           // ক
</script>
```

**Option B — npm (after `npm install @khiproteam/khipro`):**

```ts
import loadKhiproWasm from "@khiproteam/khipro/khipro.js";
import { KhiproEngine } from "@khiproteam/khipro";

const engine = await KhiproEngine.create(loadKhiproWasm, "touchscreen");
```

## API

```ts
type KhiproVariant = 'default' | 'touchscreen';

interface KhiproResult {
  committedDelta: string;  // append to input.value
  preedit: string;         // show inline at cursor
  committed: string;       // full session total
  consumed: boolean;       // false → let the browser handle the key
}

class KhiproEngine {
  static create(createModule: CreateKhiproModule, variant?: KhiproVariant): Promise<KhiproEngine>;
  feedKey(key: string): KhiproResult;
  backspace(): KhiproResult;
  commit(): KhiproResult;
  reset(): void;
  destroy(): void;
}

// Top-level version accessors. Object literal with getters — empty strings
// until the first KhiproEngine.create() has loaded the module.
const Khipro: {
  readonly layoutVersion: string;   // e.g. "35.0.1"
  readonly libraryVersion: string;  // e.g. "35.0.1-1"
};
```

## Minimal example

```ts
import { loadKhiproModule, KhiproEngine } from "./index.js";

const createModule = await loadKhiproModule("./khipro.js");
const engine = await KhiproEngine.create(createModule, "default");

document.querySelector("input")!.addEventListener("keydown", (ev) => {
  if (ev.key.length !== 1) return;             // skip Enter, Arrow keys, etc.

  const r = engine.feedKey(ev.key);
  if (!r.consumed) return;                      // let the browser type the raw char

  ev.preventDefault();                          // we own this keystroke
  // apply r.committedDelta + r.preedit to the input
});
```

## Shortcut: `attachKhiproInput`

For a no-wiring integration into a single `<input>` or `<textarea>`:

```ts
import { attachKhiproInput } from "./index.js";

const detach = attachKhiproInput(
  document.querySelector("textarea")!,
  engine,
);
// later: detach() to remove listeners
```

The helper handles:

- **Printable keys** → `feedKey()`, render `committed + preedit`.
- **Backspace** → `backspace()`, re-render.
- **Enter** → `commit()` (flush preedit), then let the browser insert the newline.
- **Escape** → `reset()` (discard composing state), clear the field.
- **Blur** → `commit()` (flush preedit so it isn't lost when the field loses focus).

Use it as a reference implementation if you need richer UI (candidate windows, etc.).

## Variant choice

- **`default`** for desktop browsers with a physical keyboard.
- **`touchscreen`** for mobile browsers — pair with your own soft keyboard or a contenteditable composer.

## Notes

- `.wasm` is ~500 KB uncompressed, ~180 KB gzipped. Serve with `Content-Encoding: gzip` (or Brotli) for production.
- Loading is async (`KhiproEngine.create` returns a Promise). Show a loading state for the input.
- One engine instance per input field. Call `destroy()` on unmount to free WASM memory — the shim keeps a small per-engine result cache that's only reclaimed on `destroy()`.
- `Khipro.layoutVersion` and `Khipro.libraryVersion` are getters backed by a module reference that the first `KhiproEngine.create()` registers. They return empty strings before that point. Cheap to read after first load.
- Per-engine state is safe: two engines on the same page don't clobber each other's preedit.
- Cross-origin: serve `khipro.js` and `khipro.wasm` with `Cross-Origin-Resource-Policy: same-origin` or appropriate CORS headers if not co-hosted with your app.
