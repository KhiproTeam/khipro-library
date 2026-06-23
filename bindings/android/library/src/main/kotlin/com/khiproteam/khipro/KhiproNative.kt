package com.khiproteam.khipro

object KhiproNative {
    init {
        System.loadLibrary("khipro")
    }

    external fun create(variant: Int): Long
    external fun destroy(handle: Long)
    external fun feedKey(handle: Long, key: String): KhiproResult
    external fun backspace(handle: Long): KhiproResult
    external fun commit(handle: Long): KhiproResult
    external fun reset(handle: Long)
    external fun convert(variant: Int, ascii: String): String
    external fun layoutVersion(): String
    external fun libraryVersion(): String
}
