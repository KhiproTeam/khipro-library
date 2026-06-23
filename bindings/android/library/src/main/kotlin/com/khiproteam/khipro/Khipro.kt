package com.khiproteam.khipro

object Khipro {
    @JvmStatic
    @JvmOverloads
    fun convert(ascii: String, variant: KhiproVariant = KhiproVariant.DEFAULT): String =
        KhiproNative.convert(variant.nativeValue, ascii)

    @JvmStatic
    val layoutVersion: String = KhiproNative.layoutVersion()

    @JvmStatic
    val libraryVersion: String = KhiproNative.libraryVersion()
}
