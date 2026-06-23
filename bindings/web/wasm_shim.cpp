#include <emscripten.h>

#include <cstring>
#include <unordered_map>

#include "khipro/khipro.h"

namespace {

// Per-engine result storage. Each engine gets its own slot, so two engines
// on the same page don't clobber each other's last-result state.
std::unordered_map<khipro_engine*, khipro_result>& results() {
  static std::unordered_map<khipro_engine*, khipro_result> map;
  return map;
}

const char* or_empty(const char* s) { return s ? s : ""; }

}  // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE
int wasm_khipro_feed_key(khipro_engine* engine, const char* key) {
  auto& slot = results()[engine];
  slot = khipro_feed_key(engine, key);
  return slot.consumed;
}

EMSCRIPTEN_KEEPALIVE
int wasm_khipro_backspace(khipro_engine* engine) {
  auto& slot = results()[engine];
  slot = khipro_backspace(engine);
  return slot.consumed;
}

EMSCRIPTEN_KEEPALIVE
int wasm_khipro_commit(khipro_engine* engine) {
  auto& slot = results()[engine];
  slot = khipro_commit(engine);
  return slot.consumed;
}

EMSCRIPTEN_KEEPALIVE
void wasm_khipro_destroy(khipro_engine* engine) {
  results().erase(engine);
  khipro_destroy(engine);
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_khipro_last_committed_delta(khipro_engine* engine) {
  auto it = results().find(engine);
  return it != results().end() ? or_empty(it->second.committed_delta) : "";
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_khipro_last_preedit(khipro_engine* engine) {
  auto it = results().find(engine);
  return it != results().end() ? or_empty(it->second.preedit) : "";
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_khipro_last_committed(khipro_engine* engine) {
  auto it = results().find(engine);
  return it != results().end() ? or_empty(it->second.committed) : "";
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_khipro_layout_version(void) {
  return or_empty(khipro_version());
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_khipro_library_version(void) {
  return or_empty(khipro_library_version());
}

}
