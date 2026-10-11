// Dev-time generator: parses resources/*.mim and emits core/src/spec_generated.{h,cpp}.
// Build natively on the host and run before target compilation (see
// scripts/targets/common.sh).

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "emit.h"
#include "spec.h"

namespace {

std::string read_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    std::cerr << "codegen: cannot read " << path << "\n";
    std::exit(1);
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

std::map<std::string, std::string> read_meta(const std::string& path) {
  const std::string text = read_file(path);
  std::map<std::string, std::string> out;
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    const auto first = line.find_first_not_of(" \t");
    if (first == std::string::npos || line[first] == '#') {
      continue;
    }
    const auto eq = line.find('=');
    if (eq == std::string::npos) {
      continue;
    }
    const auto key = line.substr(first, eq - first);
    const auto value_first = line.find_first_not_of(" \t", eq + 1);
    const auto value_last = line.find_last_not_of(" \t\r");
    if (value_first == std::string::npos) {
      continue;
    }
    out[key] = line.substr(value_first, value_last - value_first + 1);
  }
  return out;
}

void usage() {
  std::cerr << "usage: codegen --meta <khipro.meta> --layouts <name>:<mim>... "
               "--out-h <path> --out-cpp <path>\n";
  std::exit(2);
}

}  // namespace

int main(int argc, char** argv) {
  std::string meta_path;
  std::vector<std::pair<std::string, std::string>> layout_paths;
  std::string out_h;
  std::string out_cpp;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    const auto value = [&]() -> std::string {
      if (i + 1 >= argc) usage();
      return argv[++i];
    };
    if (arg == "--meta") {
      meta_path = value();
    } else if (arg == "--layouts") {
      const std::string spec = value();
      const auto colon = spec.find(':');
      if (colon == std::string::npos) usage();
      layout_paths.emplace_back(spec.substr(0, colon), spec.substr(colon + 1));
    } else if (arg == "--out-h") {
      out_h = value();
    } else if (arg == "--out-cpp") {
      out_cpp = value();
    } else {
      usage();
    }
  }
  if (meta_path.empty() || layout_paths.empty() || out_h.empty() || out_cpp.empty()) {
    usage();
  }

  const auto meta = read_meta(meta_path);
  const std::vector<std::string> version_keys = {
      "library_version",
      "khipro_classic_version",
      "khipro_touchscreen_classic_version",
      "khipro_dark_version",
      "khipro_touchscreen_dark_version",
  };
  std::vector<std::string> versions;
  for (const auto& key : version_keys) {
    auto it = meta.find(key);
    if (it == meta.end()) {
      std::cerr << "codegen: khipro.meta missing " << key << "\n";
      return 1;
    }
    versions.push_back(it->second);
  }
  const std::string& library_version = versions[0];

  std::vector<khipro::codegen::LayoutInput> layouts;
  std::vector<khipro::internal::Spec> storage;
  storage.reserve(layout_paths.size());
  for (const auto& [accessor, path] : layout_paths) {
    storage.push_back(khipro::internal::parse_spec(read_file(path)));
    const khipro::internal::Spec& spec = storage.back();
    if (spec.maps.empty() || spec.states.empty()) {
      std::cerr << "codegen: " << path << " produced an empty spec\n";
      return 1;
    }
    layouts.push_back({accessor, &spec});
  }

  std::string header;
  std::string source;
  khipro::codegen::emit_spec_sources(layouts, versions, &header, &source);

  const auto write = [](const std::string& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
      std::cerr << "codegen: cannot write " << path << "\n";
      std::exit(1);
    }
    file << text;
  };
  write(out_h, header);
  write(out_cpp, source);

  std::cout << "codegen: wrote " << out_h << " and " << out_cpp << " ("
            << layouts.size() << " layouts, library " << library_version << ")\n";
  return 0;
}
