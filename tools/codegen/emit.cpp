#include "emit.h"

#include <algorithm>
#include <map>

namespace khipro::codegen {

namespace {

using khipro::internal::Action;
using khipro::internal::ActionType;
using khipro::internal::CondBranch;
using khipro::internal::Condition;
using khipro::internal::ConditionType;
using khipro::internal::MapEntry;
using khipro::internal::Spec;
using khipro::internal::StateDef;
using khipro::internal::StateRule;

std::string escape_literal(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (const char c : s) {
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c; break;
    }
  }
  return out;
}

std::string quoted(const std::string& s) { return "\"" + escape_literal(s) + "\""; }

std::string emit_condition(const Condition& cond);

std::string emit_action(const Action& action) {
  switch (action.type) {
    case ActionType::Set:
      return "act_set(" + quoted(action.s1) + ", " + quoted(action.s2) + ")";
    case ActionType::Insert:
      return "act_insert(" + quoted(action.s1) + ")";
    case ActionType::Delete:
      return "act_delete(" + std::to_string(action.n) + ")";
    case ActionType::Move:
      return action.flag ? "act_move_end()" : "act_move(" + std::to_string(action.n) + ")";
    case ActionType::Mark:
      return "act_mark(" + quoted(action.s1) + ")";
    case ActionType::MoveToMark:
      return "act_move_to_mark(" + quoted(action.s1) + ")";
    case ActionType::DeleteMark:
      return "act_delete_mark(" + quoted(action.s1) + ")";
    case ActionType::Shift:
      return "act_shift(" + quoted(action.s1) + ")";
    case ActionType::Commit:
      return "act_commit()";
    case ActionType::Undo:
      return "act_undo()";
    case ActionType::CondBranch: {
      std::string out = "act_cond({";
      for (const CondBranch& branch : action.branches) {
        out += "{";
        out += emit_condition(branch.condition);
        out += ", {";
        for (const Action& child : branch.actions) {
          out += emit_action(child);
          out += ", ";
        }
        if (!branch.actions.empty()) {
          out.resize(out.size() - 2);  // trailing ", "
        }
        out += "}},";
      }
      if (!action.branches.empty()) {
        out.pop_back();  // trailing ","
      }
      out += "})";
      return out;
    }
  }
  return "";
}

std::string emit_condition(const Condition& cond) {
  switch (cond.type) {
    case ConditionType::Always:
      return "cond_always()";
    case ConditionType::Equals:
      return "cond_eq(" + quoted(cond.variable) + ", " + std::to_string(cond.value) + ")";
    case ConditionType::GreaterThan:
      return "cond_gt(" + quoted(cond.variable) + ", " + std::to_string(cond.value) + ")";
    case ConditionType::And:
      return "cond_and(" + emit_condition(*cond.left) + ", " + emit_condition(*cond.right) + ")";
    case ConditionType::Or:
      return "cond_or(" + emit_condition(*cond.left) + ", " + emit_condition(*cond.right) + ")";
  }
  return "";
}

std::string emit_actions_list(const std::vector<Action>& actions) {
  std::string out = "{";
  for (const Action& action : actions) {
    out += emit_action(action);
    out += ", ";
  }
  if (!actions.empty()) {
    out.resize(out.size() - 2);  // trailing ", "
  }
  out += "}";
  return out;
}

std::string emit_builder(const std::string& accessor, const Spec& spec) {
  std::string out = "static Spec build_" + accessor + "() {\n";
  out += "  Spec spec;\n";

  std::map<std::string, std::vector<MapEntry>> maps(spec.maps.begin(), spec.maps.end());
  for (const auto& [name, entries] : maps) {
    out += "\n  std::vector<MapEntry>& map_" + name + " = spec.maps[" + quoted(name) + "];\n";
    for (const MapEntry& entry : entries) {
      out += "  map_" + name + ".push_back(MapEntry{" + quoted(entry.key) + ", " +
             emit_actions_list(entry.actions) + "});\n";
    }
  }

  std::map<std::string, StateDef> states(spec.states.begin(), spec.states.end());
  for (const auto& [name, state] : states) {
    out += "\n  {\n";
    out += "    StateDef& st = spec.states[" + quoted(name) + "];\n";
    out += "    st.entry_actions = " + emit_actions_list(state.entry_actions) + ";\n";
    for (const StateRule& rule : state.rules) {
      out += "    st.rules.push_back(StateRule{" + quoted(rule.map_name) + ", " +
             emit_actions_list(rule.actions) + "});\n";
    }
    out += "  }\n";
  }

  out += "  return spec;\n";
  out += "}\n";
  return out;
}

// Emitted once per generated file: tiny constructors the generated tables call.
const char* const HELPERS = R"(namespace {

Action act_set(std::string var, std::string value) {
  Action a;
  a.type = ActionType::Set;
  a.s1 = std::move(var);
  a.s2 = std::move(value);
  return a;
}

Action act_insert(std::string text) {
  Action a;
  a.type = ActionType::Insert;
  a.s1 = std::move(text);
  return a;
}

Action act_delete(int n) {
  Action a;
  a.type = ActionType::Delete;
  a.n = n;
  return a;
}

Action act_move(int n) {
  Action a;
  a.type = ActionType::Move;
  a.n = n;
  return a;
}

Action act_move_end() {
  Action a;
  a.type = ActionType::Move;
  a.flag = true;
  return a;
}

Action act_mark(std::string name) {
  Action a;
  a.type = ActionType::Mark;
  a.s1 = std::move(name);
  return a;
}

Action act_move_to_mark(std::string name) {
  Action a;
  a.type = ActionType::MoveToMark;
  a.s1 = std::move(name);
  return a;
}

Action act_delete_mark(std::string name) {
  Action a;
  a.type = ActionType::DeleteMark;
  a.s1 = std::move(name);
  return a;
}

Action act_shift(std::string state) {
  Action a;
  a.type = ActionType::Shift;
  a.s1 = std::move(state);
  return a;
}

Action act_commit() { return Action{}; }

Action act_undo() {
  Action a;
  a.type = ActionType::Undo;
  return a;
}

Action act_cond(std::vector<CondBranch> branches) {
  Action a;
  a.type = ActionType::CondBranch;
  a.branches = std::move(branches);
  return a;
}

Condition cond_always() { return Condition{}; }

Condition cond_eq(std::string variable, int value) {
  Condition c;
  c.type = ConditionType::Equals;
  c.variable = std::move(variable);
  c.value = value;
  return c;
}

Condition cond_gt(std::string variable, int value) {
  Condition c;
  c.type = ConditionType::GreaterThan;
  c.variable = std::move(variable);
  c.value = value;
  return c;
}

Condition cond_and(Condition left, Condition right) {
  Condition c;
  c.type = ConditionType::And;
  c.left = std::make_shared<Condition>(std::move(left));
  c.right = std::make_shared<Condition>(std::move(right));
  return c;
}

Condition cond_or(Condition left, Condition right) {
  Condition c;
  c.type = ConditionType::Or;
  c.left = std::make_shared<Condition>(std::move(left));
  c.right = std::make_shared<Condition>(std::move(right));
  return c;
}

}  // namespace
)";

}  // namespace

void emit_spec_sources(const std::vector<LayoutInput>& layouts,
                       const std::vector<std::string>& versions, std::string* out_h,
                       std::string* out_cpp) {
  const char* const version_macros[] = {
      "KHIPRO_LIBRARY_VERSION",
      "KHIPRO_CLASSIC_VERSION",
      "KHIPRO_TOUCHSCREEN_CLASSIC_VERSION",
      "KHIPRO_DARK_VERSION",
      "KHIPRO_TOUCHSCREEN_DARK_VERSION",
  };
  out_h->clear();
  out_cpp->clear();

  *out_h += "// GENERATED by tools/codegen from resources/*.mim. Do not edit.\n";
  *out_h += "#pragma once\n\n";
  for (std::size_t i = 0; i < sizeof(version_macros) / sizeof(version_macros[0]) && i < versions.size(); ++i) {
    *out_h += "#define " + std::string(version_macros[i]) + " \"" + versions[i] + "\"\n";
  }
  *out_h += "\n";
  *out_h += "#include \"spec.h\"\n\n";
  *out_h += "namespace khipro::internal {\n";
  for (const LayoutInput& layout : layouts) {
    *out_h += "const Spec& " + layout.accessor + "_spec();\n";
  }
  *out_h += "}\n";

  *out_cpp += "// GENERATED by tools/codegen from resources/*.mim. Do not edit.\n";
  *out_cpp += "#include \"spec_generated.h\"\n\n";
  *out_cpp += "#include <utility>\n\n";
  *out_cpp += "#include \"spec.h\"\n\n";
  *out_cpp += "namespace khipro::internal {\n\n";
  *out_cpp += HELPERS;
  *out_cpp += "\n";
  for (const LayoutInput& layout : layouts) {
    *out_cpp += emit_builder(layout.accessor, *layout.spec);
    *out_cpp += "\nconst Spec& " + layout.accessor + "_spec() {\n";
    *out_cpp += "  static const Spec spec = build_" + layout.accessor + "();\n";
    *out_cpp += "  return spec;\n";
    *out_cpp += "}\n\n";
  }
  *out_cpp += "}  // namespace khipro::internal\n";
}

}
