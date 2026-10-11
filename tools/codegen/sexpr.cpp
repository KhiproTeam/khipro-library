#include "sexpr.h"

#include <cctype>
#include <stdexcept>

namespace khipro::internal {

SexprParser::SexprParser(std::string text) : text_(std::move(text)) {}

std::vector<Sexp> SexprParser::parse_all() {
  std::vector<Sexp> out;
  while (skip_ws()) {
    out.push_back(read_expr());
  }
  return out;
}

bool SexprParser::skip_ws() {
  while (idx_ < text_.size()) {
    if (std::isspace(static_cast<unsigned char>(text_[idx_]))) {
      ++idx_;
      continue;
    }
    if (text_[idx_] == ';' && idx_ + 1 < text_.size() && text_[idx_ + 1] == ';') {
      while (idx_ < text_.size() && text_[idx_] != '\n') {
        ++idx_;
      }
      continue;
    }
    return true;
  }
  return false;
}

Sexp SexprParser::read_expr() {
  if (!skip_ws()) {
    throw std::runtime_error("Unexpected EOF in S-expression parser");
  }
  if (text_[idx_] == '(') {
    ++idx_;
    Sexp list_node;
    list_node.is_atom = false;
    while (skip_ws() && text_[idx_] != ')') {
      list_node.list.push_back(read_expr());
    }
    if (idx_ >= text_.size() || text_[idx_] != ')') {
      throw std::runtime_error("Unclosed list in S-expression parser");
    }
    ++idx_;
    return list_node;
  }
  if (text_[idx_] == '"') {
    return Sexp{true, read_string(), {}};
  }
  return Sexp{true, read_atom(), {}};
}

std::string SexprParser::read_string() {
  std::string out;
  ++idx_;
  while (idx_ < text_.size() && text_[idx_] != '"') {
    if (text_[idx_] == '\\' && idx_ + 1 < text_.size()) {
      const char nxt = text_[idx_ + 1];
      switch (nxt) {
        case '\\': out.push_back('\\'); break;
        case '"':  out.push_back('"');  break;
        case 'n':  out.push_back('\n'); break;
        case 't':  out.push_back('\t'); break;
        case 'r':  out.push_back('\r'); break;
        default:   out.push_back(nxt);  break;
      }
      idx_ += 2;
      continue;
    }
    out.push_back(text_[idx_]);
    ++idx_;
  }
  if (idx_ >= text_.size()) {
    throw std::runtime_error("Unterminated string in S-expression parser");
  }
  ++idx_;
  return out;
}

std::string SexprParser::read_atom() {
  const std::size_t start = idx_;
  while (idx_ < text_.size() && !std::isspace(static_cast<unsigned char>(text_[idx_])) &&
         text_[idx_] != '(' && text_[idx_] != ')') {
    ++idx_;
  }
  return text_.substr(start, idx_ - start);
}

}
