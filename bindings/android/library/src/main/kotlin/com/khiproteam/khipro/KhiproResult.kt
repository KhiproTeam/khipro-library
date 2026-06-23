package com.khiproteam.khipro

data class KhiproResult(
    val committed: String,
    val committedDelta: String,
    val preedit: String,
    val consumed: Boolean,
) {
    val displayText: String
        get() = committed + preedit

    companion object {
        @JvmField
        val EMPTY = KhiproResult("", "", "", false)
    }
}
