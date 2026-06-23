package com.khiproteam.khipro

import android.inputmethodservice.InputMethodService
import android.view.inputmethod.EditorInfo

class KotlinImeExample : InputMethodService() {
    private var khipro: KhiproIme? = null

    override fun onStartInput(info: EditorInfo, restarting: Boolean) {
        khipro = KhiproIme(currentInputConnection, KhiproVariant.TOUCHSCREEN)
    }

    override fun onFinishInput() {
        khipro?.close()
        khipro = null
    }

    fun onKeyPress(latinKey: String) {
        val ic = currentInputConnection ?: return
        val ime = khipro ?: return
        if (!ime.onKey(latinKey)) {
            ic.commitText(latinKey, 1)
        }
    }

    fun onBackspace() {
        val ic = currentInputConnection ?: return
        val ime = khipro ?: return
        if (!ime.onBackspace()) {
            ic.deleteSurroundingText(1, 0)
        }
    }

    fun onEnter() {
        khipro?.commit()
    }
}
