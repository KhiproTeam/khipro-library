#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "khipro/khipro.h"

namespace {

// Decode the next UTF-8 codepoint starting at `i`. Advances `i` past the
// codepoint and returns the bytes for it. Inputs are typically ASCII, but
// iterating byte-by-byte would feed partial sequences to the engine if a
// testcase ever contains non-ASCII characters.
std::string next_utf8(const std::string& s, std::size_t& i) {
  const unsigned char c = static_cast<unsigned char>(s[i]);
  std::size_t len = 1;
  if      ((c & 0x80) == 0x00) len = 1;
  else if ((c & 0xE0) == 0xC0) len = 2;
  else if ((c & 0xF0) == 0xE0) len = 3;
  else if ((c & 0xF8) == 0xF0) len = 4;
  if (i + len > s.size()) len = s.size() - i;
  std::string out = s.substr(i, len);
  i += len;
  return out;
}

struct Case {
  std::string input;
  std::string expected;
};

std::string trim(const std::string& s) {
  std::size_t a = 0;
  while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  std::size_t b = s.size();
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}

std::vector<std::string> parse_tsv_row(const std::string& line) {
  std::vector<std::string> fields;
  std::string cur;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (c == '\t') {
      fields.push_back(trim(cur));
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  fields.push_back(trim(cur));
  return fields;
}

std::vector<Case> load_tsv(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("Cannot open " + path);
  }
  std::vector<Case> cases;
  std::string line;
  bool header = true;
  while (std::getline(in, line)) {
    if (line.empty()) {
      continue;
    }
    if (header) {
      header = false;
      continue;
    }
    const auto fields = parse_tsv_row(line);
    if (fields.size() < 2) {
      continue;
    }
    Case c;
    c.input = std::move(fields[0]);
    c.expected = std::move(fields[1]);
    cases.push_back(std::move(c));
  }
  return cases;
}

bool run_variant(khipro_variant variant, const std::string& tsv_path) {
  const auto cases = load_tsv(tsv_path);
  int failed = 0;

  for (const auto& c : cases) {
    khipro_engine* engine = khipro_create(variant);
    if (!engine) {
      std::cerr << "Failed to create engine for " << tsv_path << '\n';
      return false;
    }

    std::string actual;
    for (std::size_t i = 0; i < c.input.size(); ) {
      const std::string key = next_utf8(c.input, i);
      const khipro_result result = khipro_feed_key(engine, key.c_str());
      actual = std::string(result.committed ? result.committed : "") +
               std::string(result.preedit ? result.preedit : "");
      (void)result;
    }

    if (actual != c.expected) {
      ++failed;
      std::cerr << "FAIL [" << tsv_path << "] input=\"" << c.input << "\" expected=\"" << c.expected
                << "\" got=\"" << actual << "\"\n";
    }

    khipro_destroy(engine);

    char buf[4096];
    const int len = khipro_convert(variant, c.input.c_str(), buf, static_cast<int>(sizeof(buf)));
    if (len < 0 || std::string(buf) != c.expected) {
      ++failed;
      std::cerr << "FAIL convert [" << tsv_path << "] input=\"" << c.input << "\" expected=\""
                << c.expected << "\" got=\"" << buf << "\"\n";
    }
  }

  if (failed > 0) {
    std::cerr << failed << " failures in " << tsv_path << '\n';
    return false;
  }
  std::cout << "PASS " << cases.size() << " cases in " << tsv_path << '\n';
  return true;
}

}

int main() {
  const std::string default_tsv =
      (std::string(CMAKE_SOURCE_DIR) + "/resources/khipro-testcases.tsv");
  const std::string touch_tsv =
      (std::string(CMAKE_SOURCE_DIR) + "/resources/khipro-testcases-touch.tsv");

  bool ok = true;
  ok = run_variant(KHIPRO_VARIANT_DEFAULT, default_tsv) && ok;
  ok = run_variant(KHIPRO_VARIANT_TOUCHSCREEN, touch_tsv) && ok;

  if (!ok) {
    return 1;
  }
  std::cout << "All conformance tests passed.\n";
  return 0;
}
