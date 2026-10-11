#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "sexpr.h"

namespace khipro::internal {

enum class ConditionType { Always, Equals, And, Or };

struct Condition {
  ConditionType type = ConditionType::Always;
  std::string variable;
  int value = 0;
  std::shared_ptr<Condition> left;
  std::shared_ptr<Condition> right;
};

enum class ActionType {
  Set,
  Insert,
  Delete,
  Move,
  Shift,
  Commit,
  CondBranch,
  Undo,
};

struct Action;

struct CondBranch {
  Condition condition;
  std::vector<Action> actions;
};

struct Action {
  ActionType type = ActionType::Commit;
  std::string s1;
  std::string s2;
  int n = 0;
  bool flag = false;
  std::vector<CondBranch> branches;
};

struct MapEntry {
  std::string key;
  std::vector<Action> actions;
};

struct StateRule {
  std::string map_name;
  std::vector<Action> actions;
};

struct StateDef {
  std::string name;
  std::vector<Action> entry_actions;
  std::vector<StateRule> rules;
};

struct Spec {
  std::unordered_map<std::string, std::vector<MapEntry>> maps;
  std::unordered_map<std::string, StateDef> states;
};

Spec parse_spec(const std::string& spec_text);

}
