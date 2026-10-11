#include "spec.h"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <utility>

namespace khipro::internal {

namespace {

using Macros = std::map<std::string, std::vector<Sexp>>;

[[noreturn]] void reject(const std::string& ctx, const std::string& what) {
  throw std::runtime_error(ctx + ": " + what);
}

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

std::shared_ptr<Condition> parse_condition(const Sexp& node, const std::string& ctx);
Action parse_action(const Sexp& node, const Macros& macros, const std::string& ctx);
std::vector<Action> expand_item(const Sexp& item, const Macros& macros, const std::string& ctx);

std::shared_ptr<Condition> parse_condition(const Sexp& node, const std::string& ctx) {
  if (node.is_atom || node.list.empty() || !node.list[0].is_atom) {
    reject(ctx, "unparsable condition");
  }
  auto out = std::make_shared<Condition>();
  const std::string& head = node.list[0].atom;

  if (head == "1") {
    out->type = ConditionType::Always;
    return out;
  }
  if (head == "=" || head == ">") {
    if (node.list.size() >= 3 && node.list[1].is_atom && node.list[2].is_atom) {
      out->type = head == "=" ? ConditionType::Equals : ConditionType::GreaterThan;
      out->variable = node.list[1].atom;
      out->value = parse_int_or_default(node.list[2].atom, 0);
      return out;
    }
    reject(ctx, "condition " + head + " needs a variable and a value");
  }
  if (head == "&" || head == "|") {
    if (node.list.size() >= 3) {
      auto left = parse_condition(node.list[1], ctx);
      auto right = parse_condition(node.list[2], ctx);
      out->type = (head == "&") ? ConditionType::And : ConditionType::Or;
      out->left = std::move(left);
      out->right = std::move(right);
      return out;
    }
    reject(ctx, "condition " + head + " needs two operands");
  }
  reject(ctx, "unknown condition '" + head + "'");
}

Action parse_action(const Sexp& node, const Macros& macros, const std::string& ctx) {
  if (node.is_atom || node.list.empty() || !node.list[0].is_atom) {
    reject(ctx, "unparsable action");
  }

  const std::string& head = node.list[0].atom;
  if (head == "set") {
    if (node.list.size() >= 3 && node.list[1].is_atom && node.list[2].is_atom) {
      Action out;
      out.type = ActionType::Set;
      out.s1 = node.list[1].atom;
      out.s2 = node.list[2].atom;
      return out;
    }
    reject(ctx, "set needs a variable and a value");
  }
  if (head == "insert") {
    if (node.list.size() >= 2 && node.list[1].is_atom) {
      Action out;
      out.type = ActionType::Insert;
      out.s1 = node.list[1].atom;
      if (!out.s1.empty() && out.s1[0] == '?') {
        out.s1.erase(0, 1);
      }
      return out;
    }
    reject(ctx, "insert needs text");
  }
  if (head == "delete") {
    if (node.list.size() >= 2 && node.list[1].is_atom) {
      const std::string token = node.list[1].atom;
      Action out;
      if (token.rfind("@-", 0) == 0) {
        out.type = ActionType::Delete;
        const std::string count = token.substr(2);
        out.n = count.empty() ? 1 : parse_int_or_default(count, 1);
        return out;
      }
      out.type = ActionType::DeleteMark;
      out.s1 = token;
      return out;
    }
    reject(ctx, "delete needs a target");
  }
  if (head == "move") {
    if (node.list.size() >= 2 && node.list[1].is_atom) {
      const std::string token = node.list[1].atom;
      Action out;
      out.type = ActionType::Move;
      if (token == "@>") {
        out.flag = true;
        return out;
      }
      if (token == "@-") {
        out.n = -1;
        return out;
      }
      if (token.rfind("@-", 0) == 0) {
        out.n = -parse_int_or_default(token.substr(2), 1);
        return out;
      }
      out.type = ActionType::MoveToMark;
      out.s1 = token;
      return out;
    }
    reject(ctx, "move needs a target");
  }
  if (head == "mark") {
    if (node.list.size() >= 2 && node.list[1].is_atom) {
      Action out;
      out.type = ActionType::Mark;
      out.s1 = node.list[1].atom;
      return out;
    }
    reject(ctx, "mark needs a name");
  }
  if (head == "shift") {
    if (node.list.size() >= 2 && node.list[1].is_atom) {
      Action out;
      out.type = ActionType::Shift;
      out.s1 = node.list[1].atom;
      return out;
    }
    reject(ctx, "shift needs a state");
  }
  if (head == "commit") {
    Action out;
    out.type = ActionType::Commit;
    return out;
  }
  if (head == "undo") {
    Action out;
    out.type = ActionType::Undo;
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
        cb.condition = *parse_condition(branch.list[0], ctx);
      }

      for (std::size_t j = 1; j < branch.list.size(); ++j) {
        std::vector<Action> expanded =
            expand_item(branch.list[j], macros, ctx + " branch " + std::to_string(i));
        cb.actions.insert(cb.actions.end(), expanded.begin(), expanded.end());
      }
      cond_action.branches.push_back(std::move(cb));
    }

    if (!cond_action.branches.empty()) {
      return cond_action;
    }
    reject(ctx, "cond has no usable branch");
  }
  if (macros.count(head) != 0) {
    reject(ctx, "nested macro call '" + head + "'");
  }
  reject(ctx, "unknown action '" + head + "'");
}

// One item from an action list: a call to a defined macro splices its body's
// actions in; anything else parses as a single action.
std::vector<Action> expand_item(const Sexp& item, const Macros& macros, const std::string& ctx) {
  if (!item.is_atom && !item.list.empty() && item.list[0].is_atom) {
    auto it = macros.find(item.list[0].atom);
    if (it != macros.end()) {
      std::vector<Action> out;
      for (const auto& body_item : it->second) {
        out.push_back(parse_action(body_item, macros, ctx));
      }
      return out;
    }
  }
  return {parse_action(item, macros, ctx)};
}

void parse_maps(const std::vector<Sexp>& nodes, const Macros& macros, Spec* spec) {
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

      const std::string ctx = "map " + map_name + " key \"" + key + "\"";
      MapEntry e;
      e.key = key;
      std::string output;
      for (std::size_t j = 1; j < entry.list.size(); ++j) {
        const auto& item = entry.list[j];
        if (item.is_atom) {
          output = item.atom;
          continue;
        }
        std::vector<Action> expanded = expand_item(item, macros, ctx);
        e.actions.insert(e.actions.end(), expanded.begin(), expanded.end());
      }
      if (!output.empty()) {
        Action insert;
        insert.type = ActionType::Insert;
        insert.s1 = output;
        e.actions.push_back(std::move(insert));
      }
      out_entries.push_back(std::move(e));
    }

    std::sort(out_entries.begin(), out_entries.end(),
              [](const MapEntry& a, const MapEntry& b) { return a.key.size() > b.key.size(); });
  }
}

void parse_states(const std::vector<Sexp>& nodes, const Macros& macros, Spec* spec) {
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
          std::vector<Action> expanded =
              expand_item(entry.list[j], macros, "state " + def.name + " entry");
          def.entry_actions.insert(def.entry_actions.end(), expanded.begin(), expanded.end());
        }
      } else {
        StateRule r;
        r.map_name = head;
        for (std::size_t j = 1; j < entry.list.size(); ++j) {
          std::vector<Action> expanded =
              expand_item(entry.list[j], macros, "state " + def.name + " map " + head);
          r.actions.insert(r.actions.end(), expanded.begin(), expanded.end());
        }
        def.rules.push_back(std::move(r));
      }
    }
    spec->states[def.name] = std::move(def);
  }
}

Macros collect_macros(const std::vector<Sexp>& root) {
  Macros macros;
  for (const auto& node : root) {
    if (node.is_atom || node.list.empty() || !node.list[0].is_atom) {
      continue;
    }
    if (node.list[0].atom != "macro") {
      continue;
    }
    for (std::size_t i = 1; i < node.list.size(); ++i) {
      const auto& def = node.list[i];
      if (def.is_atom || def.list.empty() || !def.list[0].is_atom) {
        continue;
      }
      macros[def.list[0].atom] = std::vector<Sexp>(def.list.begin() + 1, def.list.end());
    }
  }
  return macros;
}

}  // namespace

Spec parse_spec(const std::string& spec_text) {
  SexprParser parser(spec_text);
  const auto root = parser.parse_all();

  const Macros macros = collect_macros(root);

  Spec spec;
  for (const auto& node : root) {
    if (node.is_atom || node.list.empty() || !node.list[0].is_atom) {
      continue;
    }
    const std::string& head = node.list[0].atom;
    if (head == "map") {
      parse_maps(std::vector<Sexp>(node.list.begin() + 1, node.list.end()), macros, &spec);
    } else if (head == "state") {
      parse_states(std::vector<Sexp>(node.list.begin() + 1, node.list.end()), macros, &spec);
    }
  }
  return spec;
}

}  // namespace khipro::internal
