#include "engine.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace khipro::internal {

Engine::Engine(Spec spec) : spec_(std::move(spec)) { reset(); }

void Engine::reset() {
  tape_.clear();
  committed_.clear();
  preedit_.clear();
  last_committed_delta_.clear();
}

std::size_t Engine::utf8_char_len(unsigned char b0) {
  if (b0 < 0x80) {
    return 1;
  }
  if ((b0 & 0xE0) == 0xC0) {
    return 2;
  }
  if ((b0 & 0xF0) == 0xE0) {
    return 3;
  }
  if ((b0 & 0xF8) == 0xF0) {
    return 4;
  }
  return 1;
}

bool Engine::pop_last_utf8(std::string* s) {
  if (!s || s->empty()) {
    return false;
  }
  std::size_t i = s->size() - 1;
  while (i > 0 && (((*s)[i] & 0xC0) == 0x80)) {
    --i;
  }
  s->erase(i);
  return true;
}

std::u32string Engine::utf8_to_u32(const std::string& s) {
  std::u32string out;
  std::size_t i = 0;
  while (i < s.size()) {
    char32_t cp = 0;
    const unsigned char b = static_cast<unsigned char>(s[i]);
    if (b < 0x80) {
      cp = b;
      ++i;
    } else if ((b & 0xE0) == 0xC0 && i + 1 < s.size()) {
      cp = (static_cast<char32_t>(b & 0x1F) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3F);
      i += 2;
    } else if ((b & 0xF0) == 0xE0 && i + 2 < s.size()) {
      cp = (static_cast<char32_t>(b & 0x0F) << 12) |
           ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6) |
           (static_cast<unsigned char>(s[i + 2]) & 0x3F);
      i += 3;
    } else if ((b & 0xF8) == 0xF0 && i + 3 < s.size()) {
      cp = (static_cast<char32_t>(b & 0x07) << 18) |
           ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12) |
           ((static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6) |
           (static_cast<unsigned char>(s[i + 3]) & 0x3F);
      i += 4;
    } else {
      ++i;
      continue;
    }
    out.push_back(cp);
  }
  return out;
}

std::string Engine::u32_to_utf8(const std::u32string& s) {
  std::string out;
  for (char32_t cp : s) {
    if (cp < 0x80) {
      out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
      out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  }
  return out;
}

bool Engine::eval_cond(const Condition& cond, const Vars& vars) const {
  switch (cond.type) {
    case ConditionType::Always:
      return true;
    case ConditionType::Equals: {
      auto it = vars.find(cond.variable);
      return (it != vars.end() ? it->second : 0) == cond.value;
    }
    case ConditionType::And:
      return cond.left && cond.right && eval_cond(*cond.left, vars) && eval_cond(*cond.right, vars);
    case ConditionType::Or:
      return cond.left && cond.right && (eval_cond(*cond.left, vars) || eval_cond(*cond.right, vars));
  }
  return false;
}

void Engine::execute_actions(const std::vector<Action>& actions, std::u32string* out, std::size_t* cursor,
                             Vars* vars, std::string* state_name) const {
  for (const auto& action : actions) {
    switch (action.type) {
      case ActionType::Set: {
        int value = 0;
        auto it = vars->find(action.s2);
        if (it != vars->end()) {
          value = it->second;
        } else {
          char* end = nullptr;
          const long parsed = std::strtol(action.s2.c_str(), &end, 10);
          if (end != action.s2.c_str() && *end == '\0') {
            value = static_cast<int>(parsed);
          }
        }
        (*vars)[action.s1] = value;
        break;
      }
      case ActionType::Insert: {
        const std::u32string text = utf8_to_u32(action.s1);
        out->insert(out->begin() + static_cast<std::ptrdiff_t>(*cursor), text.begin(), text.end());
        *cursor += text.size();
        break;
      }
      case ActionType::Delete: {
        if (*cursor == 0 || action.n <= 0) {
          break;
        }
        const std::size_t n = static_cast<std::size_t>(action.n);
        const std::size_t start = (*cursor > n) ? (*cursor - n) : 0;
        if (start < *cursor && start < out->size()) {
          const std::size_t end = std::min(*cursor, out->size());
          out->erase(out->begin() + static_cast<std::ptrdiff_t>(start),
                     out->begin() + static_cast<std::ptrdiff_t>(end));
          *cursor = start;
        }
        break;
      }
      case ActionType::Move: {
        if (action.flag) {
          *cursor = out->size();
        } else {
          const int next = static_cast<int>(*cursor) + action.n;
          *cursor = static_cast<std::size_t>(std::clamp(next, 0, static_cast<int>(out->size())));
        }
        break;
      }
      case ActionType::Shift: {
        *state_name = action.s1;
        auto state_it = spec_.states.find(*state_name);
        if (state_it != spec_.states.end()) {
          execute_actions(state_it->second.entry_actions, out, cursor, vars, state_name);
        }
        break;
      }
      case ActionType::Commit:
        break;
      case ActionType::Undo: {
        if (*cursor > 0 && !out->empty()) {
          const std::size_t at = *cursor - 1;
          out->erase(out->begin() + static_cast<std::ptrdiff_t>(at));
          *cursor = at;
        }
        break;
      }
      case ActionType::CondBranch: {
        for (const auto& branch : action.branches) {
          if (eval_cond(branch.condition, *vars)) {
            execute_actions(branch.actions, out, cursor, vars, state_name);
            break;
          }
        }
        break;
      }
    }
  }
}

Engine::Match Engine::find_match(const StateDef& state, const std::string& input,
                                 std::size_t index) const {
  Match best;
  std::size_t best_len = 0;

  for (const auto& rule : state.rules) {
    auto it = spec_.maps.find(rule.map_name);
    if (it == spec_.maps.end()) {
      continue;
    }
    for (const auto& entry : it->second) {
      if (entry.key.size() <= best_len) {
        continue;
      }
      if (index + entry.key.size() <= input.size() &&
          input.compare(index, entry.key.size(), entry.key) == 0) {
        best.entry = &entry;
        best.rule = &rule;
        best_len = entry.key.size();
      }
    }
  }
  return best;
}

std::string Engine::run_conversion(const std::string& input) const {
  std::string current_state = "init";
  Vars vars;
  std::u32string out;
  std::size_t cursor = 0;
  std::size_t i = 0;

  auto apply_entry = [&](const std::string& state_name) {
    auto state_it = spec_.states.find(state_name);
    if (state_it == spec_.states.end()) {
      return;
    }
    execute_actions(state_it->second.entry_actions, &out, &cursor, &vars, &current_state);
  };

  apply_entry(current_state);

  while (i < input.size()) {
    auto state_it = spec_.states.find(current_state);
    if (state_it == spec_.states.end()) {
      break;
    }

    const Match match = find_match(state_it->second, input, i);
    if (!match.entry || !match.rule) {
      const unsigned char b0 = static_cast<unsigned char>(input[i]);
      const std::size_t len = std::min(utf8_char_len(b0), input.size() - i);
      const std::u32string cp = utf8_to_u32(input.substr(i, len));
      if (!cp.empty()) {
        out.insert(out.begin() + static_cast<std::ptrdiff_t>(cursor), cp[0]);
        ++cursor;
      }
      i += len;
      if (current_state != "init") {
        current_state = "init";
        apply_entry(current_state);
      }
      continue;
    }

    execute_actions(match.entry->actions, &out, &cursor, &vars, &current_state);
    execute_actions(match.rule->actions, &out, &cursor, &vars, &current_state);
    cursor = std::min(cursor, out.size());
    i += match.entry->key.size();
  }

  return u32_to_utf8(out);
}

std::string Engine::convert(const std::string& input) const { return run_conversion(input); }

void Engine::recompute_preedit() { preedit_ = run_conversion(tape_); }

bool Engine::is_commit_trigger(const std::string& key) const {
  if (key.empty()) {
    return false;
  }
  if (key == " " || key == "\n" || key == "\r" || key == "\t") {
    return true;
  }
  const std::u32string cp = utf8_to_u32(key);
  if (cp.size() == 1) {
    const char32_t c = cp[0];
    if (c == 0x20 || c == 0x0A || c == 0x0D || c == 0x09) {
      return true;
    }
  }
  return false;
}

FeedResult Engine::make_result(bool consumed) {
  FeedResult result;
  result.committed_delta = last_committed_delta_;
  result.preedit = preedit_;
  result.consumed = consumed;
  last_committed_delta_.clear();
  return result;
}

FeedResult Engine::feed_key(const std::string& key) {
  last_committed_delta_.clear();

  if (key == "\b") {
    return backspace();
  }

  if (is_commit_trigger(key)) {
    if (!preedit_.empty()) {
      committed_ += preedit_;
      last_committed_delta_ = preedit_;
      tape_.clear();
      preedit_.clear();
    }
    committed_ += key;
    last_committed_delta_ += key;
    return make_result(true);
  }

  tape_ += key;
  recompute_preedit();
  return make_result(true);
}

FeedResult Engine::backspace() {
  last_committed_delta_.clear();

  if (!tape_.empty()) {
    pop_last_utf8(&tape_);
    recompute_preedit();
    return make_result(true);
  }

  if (!committed_.empty()) {
    pop_last_utf8(&committed_);
    last_committed_delta_ = committed_;
    return make_result(true);
  }

  return make_result(false);
}

FeedResult Engine::commit() {
  last_committed_delta_.clear();
  if (!preedit_.empty()) {
    committed_ += preedit_;
    last_committed_delta_ = preedit_;
    tape_.clear();
    preedit_.clear();
  }
  return make_result(true);
}

}
