# Android integration

## Archive

`khipro-<ver>-android.aar` — single AAR, all ABIs inside:

```
jniLibs/
├── arm64-v8a/libkhipro.so       (Android 15+ 16KB-aligned)
├── armeabi-v7a/libkhipro.so
├── x86_64/libkhipro.so
└── x86/libkhipro.so
```

Plus the Kotlin API under `com.khiproteam.khipro` (pre-bundled — no extra Gradle plugins needed).

## Gradle setup

Drop the AAR into `app/libs/`:

```
app/
└── libs/
    └── khipro-<ver>.aar
```

`app/build.gradle.kts`:

```kotlin
dependencies {
    implementation(files("libs/khipro-35.0.1-1.aar"))
}
```

That's it — no `jniLibs` extraction, no `CMakeLists.txt`. The AAR packages the native libs and the Kotlin bindings.

`minSdk = 21` is supported. All `.so` files are aligned to 16 KB so they load on Android 15+ devices.

## Kotlin API

```kotlin
import com.khiproteam.khipro.KhiproEngine
import com.khiproteam.khipro.KhiproVariant

val engine = KhiproEngine.create(KhiproVariant.TOUCHSCREEN)

val r = engine.feedKey("k")           // returns KhiproResult
println(r.preedit)                     // ক
println(r.committedDelta)              // (empty while composing)

val r2 = engine.feedKey("a")
println(r2.committedDelta)             // কা

engine.commit()                        // flush remaining preedit
engine.close()                         // calls khipro_destroy
```

`KhiproEngine` implements `AutoCloseable` — use `use { }` or close on field blur.

## IME integration pattern

Inside an `InputMethodService`:

```kotlin
class KhiproImeService : InputMethodService() {
    private val engine = KhiproEngine.create(KhiproVariant.TOUCHSCREEN)
    private val ic by lazy { currentInputConnection }

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        val ch = event.unicodeChar.toChar().toString()
        if (ch.isEmpty()) return super.onKeyDown(keyCode, event)

        val r = engine.feedKey(ch)

        // 1. Clear previous preedit
        ic.finishComposingText()

        // 2. Append any newly committed text
        if (r.committedDelta.isNotEmpty()) {
            ic.commitText(r.committedDelta, 1)
        }

        // 3. Show new preedit
        if (r.preedit.isNotEmpty()) {
            ic.setComposingText(r.preedit, 1)
        }

        return r.consumed  // false → let the system handle the key
    }

    override fun onDestroy() {
        engine.close()
        super.onDestroy()
    }
}
```

The key idea: call `finishComposingText()` **before** setting new state. The engine does not retain preedit — what you draw must match the latest result.

## Backspace

`engine.backspace()` returns a normal `KhiproResult`. If `committedDelta` is empty and `preedit` becomes empty too, the user has cleared the composing buffer — forward the backspace to the host so it deletes from the document.

## Variant choice

- **`TOUCHSCREEN`** for phones/tablets (the layout is tuned for soft keyboards).
- **`DEFAULT`** works if you're targeting ChromeOS or DeX with a physical keyboard.

## Notes

- Native libs are stripped (~600 KB per ABI).
- No network or storage permissions needed.
- Engine instances are not thread-safe — keep one per `InputConnection`.
- Diagnostics: `Khipro.layoutVersion` and `Khipro.libraryVersion` are available statically (no engine needed) for logging or a "What version am I running?" debug screen. **Heads-up:** reading either one triggers `System.loadLibrary("khipro")` on first access — they're `val`s initialized at class-init time. Avoid touching them in cold paths before the IME service is ready.
