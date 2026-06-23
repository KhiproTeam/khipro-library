export type KhiproVariant = 'default' | 'touchscreen';

export interface KhiproResult {
  committedDelta: string;
  preedit: string;
  committed: string;
  consumed: boolean;
}

export interface KhiproModule {
  _khipro_create(variant: number): number;
  _khipro_destroy(engine: number): void;
  _khipro_reset(engine: number): void;
  _khipro_convert(variant: number, asciiPtr: number, outPtr: number, outCap: number): number;
  _khipro_version(): number;
  _khipro_library_version(): number;
  _wasm_khipro_feed_key(engine: number, keyPtr: number): number;
  _wasm_khipro_backspace(engine: number): number;
  _wasm_khipro_commit(engine: number): number;
  _wasm_khipro_destroy(engine: number): void;
  _wasm_khipro_last_committed_delta(engine: number): number;
  _wasm_khipro_last_preedit(engine: number): number;
  _wasm_khipro_last_committed(engine: number): number;
  _wasm_khipro_layout_version(): number;
  _wasm_khipro_library_version(): number;
  _malloc(size: number): number;
  _free(ptr: number): void;
  UTF8ToString(ptr: number): string;
  stringToUTF8(str: string, outPtr: number, maxBytes: number): void;
  lengthBytesUTF8(str: string): number;
}

export type CreateKhiproModule = (config?: Record<string, unknown>) => Promise<KhiproModule>;

function variantToInt(variant: KhiproVariant): number {
  return variant === 'touchscreen' ? 1 : 0;
}

function readLastResult(mod: KhiproModule, handle: number, consumed: number): KhiproResult {
  return {
    committedDelta: mod.UTF8ToString(mod._wasm_khipro_last_committed_delta(handle)),
    preedit: mod.UTF8ToString(mod._wasm_khipro_last_preedit(handle)),
    committed: mod.UTF8ToString(mod._wasm_khipro_last_committed(handle)),
    consumed: consumed !== 0,
  };
}

// Module registry — the first KhiproEngine.create() registers its module here
// so the top-level Khipro layoutVersion / libraryVersion accessors can read
// version strings without a separate load.
let registeredModule: KhiproModule | null = null;

export class KhiproEngine {
  private readonly mod: KhiproModule;
  private readonly handle: number;

  private constructor(mod: KhiproModule, handle: number) {
    this.mod = mod;
    this.handle = handle;
  }

  static async create(
    createModule: CreateKhiproModule,
    variant: KhiproVariant = 'default',
  ): Promise<KhiproEngine> {
    const mod = await createModule();
    const handle = mod._khipro_create(variantToInt(variant));
    if (!handle) {
      throw new Error('khipro_create failed');
    }
    if (registeredModule === null) {
      registeredModule = mod;
    }
    return new KhiproEngine(mod, handle);
  }

  feedKey(key: string): KhiproResult {
    const ptr = this.mod._malloc(this.mod.lengthBytesUTF8(key) + 1);
    this.mod.stringToUTF8(key, ptr, this.mod.lengthBytesUTF8(key) + 1);
    const consumed = this.mod._wasm_khipro_feed_key(this.handle, ptr);
    this.mod._free(ptr);
    return readLastResult(this.mod, this.handle, consumed);
  }

  backspace(): KhiproResult {
    const consumed = this.mod._wasm_khipro_backspace(this.handle);
    return readLastResult(this.mod, this.handle, consumed);
  }

  commit(): KhiproResult {
    const consumed = this.mod._wasm_khipro_commit(this.handle);
    return readLastResult(this.mod, this.handle, consumed);
  }

  reset(): void {
    this.mod._khipro_reset(this.handle);
  }

  destroy(): void {
    // Go through the shim wrapper so it can erase the engine's entry from
    // its per-engine result map before freeing the handle.
    this.mod._wasm_khipro_destroy(this.handle);
  }
}

export const Khipro = {
  /** Layout version (e.g. "35.0.1"). Empty until at least one KhiproEngine has been created. */
  get layoutVersion(): string {
    if (!registeredModule) return '';
    return registeredModule.UTF8ToString(registeredModule._wasm_khipro_layout_version());
  },
  /** Library version (e.g. "35.0.1-1"). Empty until at least one KhiproEngine has been created. */
  get libraryVersion(): string {
    if (!registeredModule) return '';
    return registeredModule.UTF8ToString(registeredModule._wasm_khipro_library_version());
  },
};

export async function khiproConvert(
  createModule: CreateKhiproModule,
  ascii: string,
  variant: KhiproVariant = 'default',
): Promise<string> {
  const mod = await createModule();
  const inPtr = mod._malloc(mod.lengthBytesUTF8(ascii) + 1);
  mod.stringToUTF8(ascii, inPtr, mod.lengthBytesUTF8(ascii) + 1);
  const required = mod._khipro_convert(variantToInt(variant), inPtr, 0, 0);
  const outPtr = mod._malloc(required);
  mod._khipro_convert(variantToInt(variant), inPtr, outPtr, required);
  const out = mod.UTF8ToString(outPtr);
  mod._free(inPtr);
  mod._free(outPtr);
  return out;
}

export function attachKhiproInput(
  input: HTMLInputElement | HTMLTextAreaElement,
  engine: KhiproEngine,
): () => void {
  const render = (result: KhiproResult) => {
    input.value = result.committed + result.preedit;
  };

  const onKeyDown = (event: Event) => {
    const keyEvent = event as KeyboardEvent;

    if (keyEvent.key === 'Backspace') {
      keyEvent.preventDefault();
      render(engine.backspace());
      return;
    }

    if (keyEvent.key === 'Enter') {
      // Commit any pending preedit, then let the host insert the newline.
      engine.commit();
      return;
    }

    if (keyEvent.key === 'Escape') {
      // Discard the composing state — clear the field's composing span.
      engine.reset();
      render({ committedDelta: '', preedit: '', committed: '', consumed: false });
      return;
    }

    if (keyEvent.key.length === 1 && !keyEvent.ctrlKey && !keyEvent.metaKey && !keyEvent.altKey) {
      keyEvent.preventDefault();
      render(engine.feedKey(keyEvent.key));
    }
  };

  const onBlur = () => {
    // Flush any pending preedit so it isn't lost when the field loses focus.
    engine.commit();
  };

  input.addEventListener('keydown', onKeyDown);
  input.addEventListener('blur', onBlur);
  return () => {
    input.removeEventListener('keydown', onKeyDown);
    input.removeEventListener('blur', onBlur);
  };
}

export async function loadKhiproModule(wasmJsUrl: string): Promise<CreateKhiproModule> {
  const imported = await import(/* @vite-ignore */ wasmJsUrl);
  return imported.default as CreateKhiproModule;
}
