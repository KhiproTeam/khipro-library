-keep class com.khiproteam.khipro.KhiproNative { *; }
-keep class com.khiproteam.khipro.KhiproEngine { *; }
-keep class com.khiproteam.khipro.KhiproEngine$Companion { *; }
-keep class com.khiproteam.khipro.KhiproIme { *; }
-keep class com.khiproteam.khipro.KhiproResult { *; }
-keep class com.khiproteam.khipro.KhiproResult$Companion { *; }
-keep class com.khiproteam.khipro.KhiproVariant { *; }
-keepclassmembers class com.khiproteam.khipro.** {
    public <init>(...);
    public <methods>;
}
