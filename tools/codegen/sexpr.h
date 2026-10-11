#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace khipro::internal {

struct Sexp {
  bool is_atom = true;
  std::string atom;
  std::vector<Sexp> list;
};

class SexprParser {
 public:
  explicit SexprParser(std::string text);

  std::vector<Sexp> parse_all();

 private:
  bool skip_ws();
  Sexp read_expr();
  std::string read_string();
  std::string read_atom();

  std::string text_;
  std::size_t idx_ = 0;
};

}
