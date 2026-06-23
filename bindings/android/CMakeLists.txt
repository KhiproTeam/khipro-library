cmake_minimum_required(VERSION 3.16)

if(NOT ANDROID)
  message(FATAL_ERROR "bindings/android requires the Android NDK toolchain.")
endif()

add_library(khipro_jni SHARED
  jni_bridge.cpp
)
target_link_libraries(khipro_jni PRIVATE khipro_static)
target_include_directories(khipro_jni PRIVATE
  ${CMAKE_SOURCE_DIR}/core/include
)
set_target_properties(khipro_jni PROPERTIES OUTPUT_NAME khipro)
