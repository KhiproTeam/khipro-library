#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

#include "spec.h"

namespace khipro::internal {

struct FeedResult {
  std::string committed_delta;
  std::string preedit;
  bool consumed = false;
};

class Engine {
 public:
  explicit Engine(Spec spec);

  void reset();
  FeedResult feed_key(const std::string& key);
  FeedResult backspace();
  FeedResult commit();
  std::string convert(const std::string& input) const;

  const std::string& committed() const { return committed_; }
  const std::string& preedit() const { return preedit_; }

 private:
  using Vars = std::unordered_map<std::string, int>;

  struct Match {
    const MapEntry* entry = nullptr;
    const StateRule* rule = nullptr;
  };

  Match find_match(const StateDef& state, const std::string& input, std::size_t index) const;
  void execute_actions(const std::vector<Action>& actions, std::u32string* out, std::size_t* cursor,
                       Vars* vars, std::string* state_name) const;
  bool eval_cond(const Condition& cond, const Vars& vars) const;
  std::string run_conversion(const std::string& input) const;
  void recompute_preedit();
  bool is_commit_trigger(const std::string& key) const;
  FeedResult make_result(bool consumed);

  static std::u32string utf8_to_u32(const std::string& s);
  static std::string u32_to_utf8(const std::u32string& s);
  static std::size_t utf8_char_len(unsigned char b0);
  static bool pop_last_utf8(std::string* s);

  Spec spec_;
  std::string tape_;
  std::string committed_;
  std::string preedit_;
  std::string last_committed_delta_;
};

}
