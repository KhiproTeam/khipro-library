#include "khipro/khipro.h"

#include <cstring>
#include <memory>
#include <string>

#include "convert.h"
#include "embedded_specs.h"
#include "engine.h"
#include "spec.h"

namespace {

using khipro::internal::Engine;
using khipro::internal::Spec;
using khipro::internal::convert_spec;
using khipro::internal::parse_spec;

std::string spec_text(khipro_variant variant) {
  if (variant == KHIPRO_VARIANT_TOUCHSCREEN) {
    return std::string(reinterpret_cast<const char*>(khipro_spec_touchscreen),
                       khipro_spec_touchscreen_len);
  }
  return std::string(reinterpret_cast<const char*>(khipro_spec_default), khipro_spec_default_len);
}

const Spec& cached_spec(khipro_variant variant) {
  if (variant == KHIPRO_VARIANT_TOUCHSCREEN) {
    static const Spec spec = parse_spec(spec_text(KHIPRO_VARIANT_TOUCHSCREEN));
    return spec;
  }
  static const Spec spec = parse_spec(spec_text(KHIPRO_VARIANT_DEFAULT));
  return spec;
}

struct EngineHandle {
  Engine engine;
  std::string committed_delta;
  std::string preedit;
  std::string committed;

  explicit EngineHandle(khipro_variant variant)
      : engine(Engine(cached_spec(variant))) {}
};

khipro_result make_c_result(EngineHandle* handle, const khipro::internal::FeedResult& feed,
                            bool consumed_override = false) {
  handle->committed_delta = feed.committed_delta;
  handle->preedit = feed.preedit;
  handle->committed = handle->engine.committed();

  khipro_result result{};
  result.committed_delta = handle->committed_delta.c_str();
  result.preedit = handle->preedit.c_str();
  result.committed = handle->committed.c_str();
  result.consumed = consumed_override ? 1 : (feed.consumed ? 1 : 0);
  return result;
}

}

extern "C" {

khipro_engine* khipro_create(khipro_variant variant) {
  try {
    return reinterpret_cast<khipro_engine*>(new EngineHandle(variant));
  } catch (...) {
    return nullptr;
  }
}

void khipro_destroy(khipro_engine* engine) {
  delete reinterpret_cast<EngineHandle*>(engine);
}

khipro_result khipro_feed_key(khipro_engine* engine, const char* utf8_key) {
  if (!engine || !utf8_key) {
    khipro_result empty{};
    return empty;
  }
  auto* handle = reinterpret_cast<EngineHandle*>(engine);
  const auto feed = handle->engine.feed_key(utf8_key);
  return make_c_result(handle, feed);
}

khipro_result khipro_backspace(khipro_engine* engine) {
  if (!engine) {
    khipro_result empty{};
    return empty;
  }
  auto* handle = reinterpret_cast<EngineHandle*>(engine);
  const auto feed = handle->engine.backspace();
  return make_c_result(handle, feed);
}

khipro_result khipro_commit(khipro_engine* engine) {
  if (!engine) {
    khipro_result empty{};
    return empty;
  }
  auto* handle = reinterpret_cast<EngineHandle*>(engine);
  const auto feed = handle->engine.commit();
  return make_c_result(handle, feed);
}

void khipro_reset(khipro_engine* engine) {
  if (!engine) {
    return;
  }
  auto* handle = reinterpret_cast<EngineHandle*>(engine);
  handle->engine.reset();
  handle->committed_delta.clear();
  handle->preedit.clear();
  handle->committed.clear();
}

int khipro_convert(khipro_variant variant, const char* ascii, char* out, int out_cap) {
  if (!ascii) {
    return 0;
  }
  try {
    const std::string converted = convert_spec(cached_spec(variant), ascii);
    const int required = static_cast<int>(converted.size()) + 1;
    if (!out || out_cap < required) {
      return required;
    }
    std::memcpy(out, converted.c_str(), converted.size() + 1);
    return required - 1;
  } catch (...) {
    return -1;
  }
}

const char* khipro_version(void) { return KHIPRO_LAYOUT_VERSION; }

const char* khipro_library_version(void) { return KHIPRO_LIBRARY_VERSION; }

}
