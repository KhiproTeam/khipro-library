#include "spec.h"

#include <algorithm>
#include <cstdlib>

namespace khipro::internal {

namespace {

int parse_int_or_default(const std::string& s, int fallback) {
  if (s.empty()) {
    return fallback;
  }
  char* end = nullptr;
  const long v = std::strtol(s.c_str(), &end, 10);
  if (end == s.c_str() || *end != '\0') {
    return fallback;
  }
  return static_cast<int>(v);
}

std::shared_ptr<Condition> parse_condition(const Sexp& node);
Action parse_action(const Sexp& node, bool* ok);

std::shared_ptr<Condition> parse_condition(const Sexp& node) {
  if (node.is_atom || node.list.empty() || !node.list[0].is_atom) {
    return nullptr;
  }
  auto out = std::make_shared<Condition>();
  const std::string& head = node.list[0].atom;

  if (head == "1") {
    out->type = ConditionType::Always;
    return out;
  }
  if (head == "=") {
    if (node.list.size() >= 3 && node.list[1].is_atom && node.list[2].is_atom) {
      out->type = ConditionType::Equals;
      out->variable = node.list[1].atom;
      out->value = parse_int_or_default(node.list[2].atom, 0);
      return out;
    }
    return nullptr;
  }
  if (head == "&" || head == "|") {
    if (node.list.size() >= 3) {
      auto left = parse_condition(node.list[1]);
      auto right = parse_condition(node.list[2]);
      if (!left || !right) {
        return nullptr;
      }
      out->type = (head == "&") ? ConditionType::And : ConditionType::Or;
      out->left = left;
      out->right = right;
      return out;
    }
  }
  return nullptr;
}

Action parse_action(const Sexp& node, bool* ok) {
  *ok = false;
  Action out;
  if (node.is_atom || node.list.empty() || !node.list[0].is_atom) {
    return out;
  }

  const std::string& head = node.list[0].atom;
  if (head == "set") {
    if (node.list.size() >= 3 && node.list[1].is_atom && node.list[2].is_atom) {
      out.type = ActionType::Set;
      out.s1 = node.list[1].atom;
      out.s2 = node.list[2].atom;
      *ok = true;
    }
    return out;
  }
  if (head == "insert") {
    if (node.list.size() >= 2 && node.list[1].is_atom) {
      out.type = ActionType::Insert;
      out.s1 = node.list[1].atom;
      if (!out.s1.empty() && out.s1[0] == '?') {
        out.s1.erase(0, 1);
      }
      *ok = true;
    }
    return out;
  }
  if (head == "delete") {
    if (node.list.size() >= 2 && node.list[1].is_atom) {
      const std::string token = node.list[1].atom;
      if (token.rfind("@-", 0) == 0) {
        out.type = ActionType::Delete;
        const std::string count = token.substr(2);
        out.n = count.empty() ? 1 : parse_int_or_default(count, 1);
        *ok = true;
      }
    }
    return out;
  }
  if (head == "move") {
    if (node.list.size() >= 2 && node.list[1].is_atom) {
      const std::string token = node.list[1].atom;
      out.type = ActionType::Move;
      if (token == "@>") {
        out.flag = true;
        out.n = 0;
        *ok = true;
      } else if (token == "@-") {
        out.n = -1;
        *ok = true;
      } else if (token.rfind("@-", 0) == 0) {
        out.n = -parse_int_or_default(token.substr(2), 1);
        *ok = true;
      }
    }
    return out;
  }
  if (head == "shift") {
    if (node.list.size() >= 2 && node.list[1].is_atom) {
      out.type = ActionType::Shift;
      out.s1 = node.list[1].atom;
      *ok = true;
    }
    return out;
  }
  if (head == "commit") {
    out.type = ActionType::Commit;
    *ok = true;
    return out;
  }
  if (head == "undo") {
    out.type = ActionType::Undo;
    *ok = true;
    return out;
  }
  if (head == "cond") {
    Action cond_action;
    cond_action.type = ActionType::CondBranch;

    for (std::size_t i = 1; i < node.list.size(); ++i) {
      const auto& branch = node.list[i];
      if (branch.is_atom || branch.list.empty()) {
        continue;
      }
      CondBranch cb;
      if (branch.list[0].is_atom && branch.list[0].atom == "1") {
        cb.condition.type = ConditionType::Always;
      } else {
        auto cond = parse_condition(branch.list[0]);
        if (!cond) {
          continue;
        }
        cb.condition = *cond;
      }

      for (std::size_t j = 1; j < branch.list.size(); ++j) {
        bool child_ok = false;
        Action child = parse_action(branch.list[j], &child_ok);
        if (child_ok) {
          cb.actions.push_back(std::move(child));
        }
      }
      cond_action.branches.push_back(std::move(cb));
    }

    if (!cond_action.branches.empty()) {
      *ok = true;
      return cond_action;
    }
  }

  return out;
}

void parse_maps(const std::vector<Sexp>& nodes, Spec* spec) {
  for (const auto& map_node : nodes) {
    if (map_node.is_atom || map_node.list.empty() || !map_node.list[0].is_atom) {
      continue;
    }
    const std::string map_name = map_node.list[0].atom;
    auto& out_entries = spec->maps[map_name];

    for (std::size_t i = 1; i < map_node.list.size(); ++i) {
      const auto& entry = map_node.list[i];
      if (entry.is_atom || entry.list.empty()) {
        continue;
      }

      std::string key;
      const auto& key_node = entry.list[0];
      if (key_node.is_atom) {
        key = key_node.atom;
      } else if (!key_node.list.empty() && key_node.list[0].is_atom) {
        key = key_node.list[0].atom == "BackSpace" ? "\b" : key_node.list[0].atom;
      }
      if (key.empty()) {
        continue;
      }

      MapEntry e;
      e.key = key;
      std::string output;
      for (std::size_t j = 1; j < entry.list.size(); ++j) {
        const auto& item = entry.list[j];
        if (item.is_atom) {
          output = item.atom;
          continue;
        }
        bool ok = false;
        Action a = parse_action(item, &ok);
        if (ok) {
          e.actions.push_back(a);
        }
      }
      if (!output.empty()) {
        Action insert;
        insert.type = ActionType::Insert;
        insert.s1 = output;
        e.actions.push_back(insert);
      }
      out_entries.push_back(std::move(e));
    }

    std::sort(out_entries.begin(), out_entries.end(),
              [](const MapEntry& a, const MapEntry& b) { return a.key.size() > b.key.size(); });
  }
}

void parse_states(const std::vector<Sexp>& nodes, Spec* spec) {
  for (const auto& state_node : nodes) {
    if (state_node.is_atom || state_node.list.empty() || !state_node.list[0].is_atom) {
      continue;
    }

    StateDef def;
    def.name = state_node.list[0].atom;
    for (std::size_t i = 1; i < state_node.list.size(); ++i) {
      const auto& entry = state_node.list[i];
      if (entry.is_atom || entry.list.empty() || !entry.list[0].is_atom) {
        continue;
      }
      const std::string head = entry.list[0].atom;
      if (head == "t") {
        for (std::size_t j = 1; j < entry.list.size(); ++j) {
          bool ok = false;
          Action a = parse_action(entry.list[j], &ok);
          if (ok) {
            def.entry_actions.push_back(a);
          }
        }
      } else {
        StateRule r;
        r.map_name = head;
        for (std::size_t j = 1; j < entry.list.size(); ++j) {
          bool ok = false;
          Action a = parse_action(entry.list[j], &ok);
          if (ok) {
            r.actions.push_back(a);
          }
        }
        def.rules.push_back(std::move(r));
      }
    }
    spec->states[def.name] = std::move(def);
  }
}

}

Spec parse_spec(const std::string& spec_text) {
  SexprParser parser(spec_text);
  const auto root = parser.parse_all();

  Spec spec;
  for (const auto& node : root) {
    if (node.is_atom || node.list.empty() || !node.list[0].is_atom) {
      continue;
    }
    const std::string& head = node.list[0].atom;
    if (head == "map") {
      parse_maps(std::vector<Sexp>(node.list.begin() + 1, node.list.end()), &spec);
    } else if (head == "state") {
      parse_states(std::vector<Sexp>(node.list.begin() + 1, node.list.end()), &spec);
    }
  }
  return spec;
}

}
