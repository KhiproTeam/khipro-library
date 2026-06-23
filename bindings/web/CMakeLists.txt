if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "bindings/web requires the Emscripten toolchain (emcmake).")
endif()

add_executable(khipro_wasm_module
  wasm_shim.cpp
)
target_link_libraries(khipro_wasm_module PRIVATE khipro_static)
target_compile_options(khipro_wasm_module PRIVATE -Os)

set_target_properties(khipro_wasm_module PROPERTIES
  OUTPUT_NAME khipro
  SUFFIX ".js"
)

target_link_options(khipro_wasm_module PRIVATE
  "-sEXPORTED_FUNCTIONS=['_khipro_create','_khipro_destroy','_khipro_reset','_khipro_convert','_khipro_version','_khipro_library_version','_wasm_khipro_feed_key','_wasm_khipro_backspace','_wasm_khipro_commit','_wasm_khipro_destroy','_wasm_khipro_last_committed_delta','_wasm_khipro_last_preedit','_wasm_khipro_last_committed','_wasm_khipro_layout_version','_wasm_khipro_library_version','_malloc','_free']"
  "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','stringToUTF8','lengthBytesUTF8']"
  "-sMODULARIZE=1"
  "-sEXPORT_NAME=createKhiproModule"
  "-sALLOW_MEMORY_GROWTH=1"
  "-sENVIRONMENT=web,worker"
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/khipro.js
  ${CMAKE_CURRENT_BINARY_DIR}/khipro.wasm
  DESTINATION share/khipro/web
)
