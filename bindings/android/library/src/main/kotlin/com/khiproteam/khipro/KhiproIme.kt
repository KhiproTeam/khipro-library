package com.khiproteam.khipro

import android.view.inputmethod.InputConnection

class KhiproIme : AutoCloseable {
    private val engine: KhiproEngine
    private val inputConnection: InputConnection?

    @JvmOverloads
    constructor(
        inputConnection: InputConnection?,
        variant: KhiproVariant = KhiproVariant.DEFAULT,
    ) {
        this.inputConnection = inputConnection
        this.engine = KhiproEngine.create(variant)
    }

    constructor(inputConnection: InputConnection?, engine: KhiproEngine) {
        this.inputConnection = inputConnection
        this.engine = engine
    }

    fun onKey(key: String): Boolean {
        val result = engine.feedKey(key)
        if (!result.consumed) return false
        applyResult(inputConnection, result)
        return true
    }

    fun onBackspace(): Boolean {
        val result = engine.backspace()
        if (!result.consumed) return false
        applyResult(inputConnection, result)
        return true
    }

    fun commit() {
        val result = engine.commit()
        applyResult(inputConnection, result)
    }

    fun reset() {
        engine.reset()
        inputConnection?.finishComposingText()
    }

    val preedit: String
        get() = engine.preedit

    val committed: String
        get() = engine.committed

    val variant: KhiproVariant
        get() = engine.variant

    override fun close() {
        engine.close()
    }
}

internal fun applyResult(ic: InputConnection?, result: KhiproResult) {
    if (ic == null) return
    if (result.committedDelta.isNotEmpty()) {
        ic.commitText(result.committedDelta, 1)
    }
    if (result.preedit.isEmpty()) {
        ic.finishComposingText()
    } else {
        ic.setComposingText(result.preedit, 1)
    }
}
