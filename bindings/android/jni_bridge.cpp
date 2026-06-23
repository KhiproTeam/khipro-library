#include <jni.h>

#include <string>

#include "khipro/khipro.h"

namespace {

std::string jstring_to_utf8(JNIEnv* env, jstring value) {
  if (!value) {
    return {};
  }
  const char* utf = env->GetStringUTFChars(value, nullptr);
  std::string out(utf ? utf : "");
  if (utf) {
    env->ReleaseStringUTFChars(value, utf);
  }
  return out;
}

jstring utf8_to_jstring(JNIEnv* env, const char* value) {
  return env->NewStringUTF(value ? value : "");
}

khipro_variant to_variant(jint variant) {
  return variant == 1 ? KHIPRO_VARIANT_TOUCHSCREEN : KHIPRO_VARIANT_DEFAULT;
}

jobject make_result(JNIEnv* env, const khipro_result& result) {
  jclass cls = env->FindClass("com/khiproteam/khipro/KhiproResult");
  if (!cls) return nullptr;
  jmethodID ctor = env->GetMethodID(
      cls, "<init>",
      "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Z)V");
  if (!ctor) {
    env->DeleteLocalRef(cls);
    return nullptr;
  }
  jstring committed = utf8_to_jstring(env, result.committed);
  jstring delta = utf8_to_jstring(env, result.committed_delta);
  jstring preedit = utf8_to_jstring(env, result.preedit);
  jobject obj = env->NewObject(cls, ctor, committed, delta, preedit,
                               result.consumed ? JNI_TRUE : JNI_FALSE);
  env->DeleteLocalRef(cls);
  return obj;
}

}

extern "C" JNIEXPORT jlong JNICALL
Java_com_khiproteam_khipro_KhiproNative_create(JNIEnv*, jclass, jint variant) {
  return reinterpret_cast<jlong>(khipro_create(to_variant(variant)));
}

extern "C" JNIEXPORT void JNICALL
Java_com_khiproteam_khipro_KhiproNative_destroy(JNIEnv*, jclass, jlong handle) {
  khipro_destroy(reinterpret_cast<khipro_engine*>(handle));
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_khiproteam_khipro_KhiproNative_feedKey(JNIEnv* env, jclass, jlong handle, jstring key) {
  const std::string input = jstring_to_utf8(env, key);
  const khipro_result result =
      khipro_feed_key(reinterpret_cast<khipro_engine*>(handle), input.c_str());
  return make_result(env, result);
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_khiproteam_khipro_KhiproNative_backspace(JNIEnv* env, jclass, jlong handle) {
  const khipro_result result = khipro_backspace(reinterpret_cast<khipro_engine*>(handle));
  return make_result(env, result);
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_khiproteam_khipro_KhiproNative_commit(JNIEnv* env, jclass, jlong handle) {
  const khipro_result result = khipro_commit(reinterpret_cast<khipro_engine*>(handle));
  return make_result(env, result);
}

extern "C" JNIEXPORT void JNICALL
Java_com_khiproteam_khipro_KhiproNative_reset(JNIEnv*, jclass, jlong handle) {
  khipro_reset(reinterpret_cast<khipro_engine*>(handle));
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_khiproteam_khipro_KhiproNative_convert(JNIEnv* env, jclass, jint variant, jstring ascii) {
  const std::string input = jstring_to_utf8(env, ascii);
  const int required = khipro_convert(to_variant(variant), input.c_str(), nullptr, 0);
  if (required <= 0) {
    return utf8_to_jstring(env, "");
  }
  std::string buffer(static_cast<std::size_t>(required), '\0');
  khipro_convert(to_variant(variant), input.c_str(), buffer.data(), required);
  if (!buffer.empty() && buffer.back() == '\0') {
    buffer.pop_back();
  }
  return utf8_to_jstring(env, buffer.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_khiproteam_khipro_KhiproNative_layoutVersion(JNIEnv* env, jclass) {
  return utf8_to_jstring(env, khipro_version());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_khiproteam_khipro_KhiproNative_libraryVersion(JNIEnv* env, jclass) {
  return utf8_to_jstring(env, khipro_library_version());
}
