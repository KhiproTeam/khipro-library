#pragma once

#include <string>
#include <vector>

#include "spec.h"

namespace khipro::codegen {

// One layout to emit: accessor suffix ("classic") plus its parsed spec.
struct LayoutInput {
  std::string accessor;
  const khipro::internal::Spec* spec = nullptr;
};

// Writes the spec_generated.h/.cpp pair. `versions` carries
// {library_version, classic, touchscreen_classic, dark, touchscreen_dark}.
// Output is deterministic: the same inputs always produce the same bytes.
void emit_spec_sources(const std::vector<LayoutInput>& layouts,
                       const std::vector<std::string>& versions, std::string* out_h,
                       std::string* out_cpp);

}
