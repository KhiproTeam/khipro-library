#include "convert.h"

namespace khipro::internal {

std::string convert_spec(const Spec& spec, const std::string& ascii) {
  Engine engine(spec);
  return engine.convert(ascii);
}

}
