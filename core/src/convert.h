#pragma once

#include <string>

#include "engine.h"
#include "spec.h"

namespace khipro::internal {

std::string convert_spec(const Spec& spec, const std::string& ascii);

}
