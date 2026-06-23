package com.khiproteam.khipro

class KhiproEngine private constructor(
    private val handle: Long,
    val variant: KhiproVariant,
) : AutoCloseable {
    private var lastResult: KhiproResult = KhiproResult.EMPTY

    val committed: String get() = lastResult.committed
    val preedit: String get() = lastResult.preedit

    fun feedKey(key: String): KhiproResult {
        lastResult = KhiproNative.feedKey(handle, key)
        return lastResult
    }

    fun backspace(): KhiproResult {
        lastResult = KhiproNative.backspace(handle)
        return lastResult
    }

    fun commit(): KhiproResult {
        lastResult = KhiproNative.commit(handle)
        return lastResult
    }

    fun reset() {
        KhiproNative.reset(handle)
        lastResult = KhiproResult.EMPTY
    }

    fun convert(ascii: String): String =
        KhiproNative.convert(variant.nativeValue, ascii)

    override fun close() {
        KhiproNative.destroy(handle)
    }

    companion object {
        @JvmStatic
        @JvmOverloads
        fun create(variant: KhiproVariant = KhiproVariant.DEFAULT): KhiproEngine {
            val handle = KhiproNative.create(variant.nativeValue)
            require(handle != 0L) { "khipro_create failed" }
            return KhiproEngine(handle, variant)
        }

        @JvmStatic
        val layoutVersion: String = KhiproNative.layoutVersion()

        @JvmStatic
        val libraryVersion: String = KhiproNative.libraryVersion()
    }
}
