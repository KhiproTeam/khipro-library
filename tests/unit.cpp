// Engine unit tests for the mark semantics the dark layouts rely on.

#include <cassert>
#include <iostream>
#include <string>

#include "engine.h"
#include "spec.h"

using namespace khipro::internal;

namespace {

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

Action act_set(std::string var, std::string value) {
  Action a;
  a.type = ActionType::Set;
  a.s1 = std::move(var);
  a.s2 = std::move(value);
  return a;
}

Action act_cond(Condition condition, std::vector<Action> actions) {
  CondBranch branch;
  branch.condition = std::move(condition);
  branch.actions = std::move(actions);
  Action a;
  a.type = ActionType::CondBranch;
  a.branches.push_back(std::move(branch));
  return a;
}

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

// One state "init" with a single rule keyed "q" carrying the given actions.
Spec key_spec(std::vector<Action> actions) {
  Spec spec;
  spec.maps["m"] = {MapEntry{"q", std::move(actions)}};
  spec.states["init"] = StateDef{"init", {}, {StateRule{"m", {}}}};
  return spec;
}

std::string run(std::vector<Action> actions) {
  Engine engine(key_spec(std::move(actions)));
  return engine.convert("q");
}

int failures = 0;

void expect(const std::string& label, const std::string& got, const std::string& want) {
  if (got != want) {
    std::cerr << label << ": got \"" << got << "\" want \"" << want << "\"\n";
    ++failures;
  }
}

}  // namespace

int main() {
  // Mark → move away → move to mark splices at the marked offset.
  expect("splice", run({act_insert("কখ"), act_move(-1), act_mark("cstm1"), act_move_end(),
                        act_insert("গ"), act_move_to_mark("cstm1"), act_insert("X")}),
         "কXখগ");

  // Moving to an unset mark clamps to 0; re-marking overwrites.
  expect("unset-remark", run({act_insert("আ"), act_move_to_mark("nope"), act_insert("_"),
                              act_mark("m1"), act_move_end(), act_mark("m1"),
                              act_move_to_mark("m1"), act_insert("!")}),
         "_আ!");

  // Delete-mark removes the region between mark and cursor, cursor at start.
  expect("delete-mark", run({act_insert("কখগ"), act_move(-2), act_mark("m1"), act_move_end(),
                             act_insert("।"), act_delete_mark("m1"), act_insert("X")}),
         "কX");

  // Marks are bookmarks: an insert before a mark shifts it right.
  expect("bookmark-shift", run({act_insert("ক"), act_mark("m"), act_move(-1), act_insert("XY"),
                                act_move_to_mark("m"), act_insert("!")}),
         "XYক!");

  // Deleting text under a mark clamps it to the region start.
  expect("delete-clamp", run({act_insert("abcd"), act_move(-3), act_mark("a"), act_move_end(),
                              act_move(-1), act_mark("b"), act_move_end(), act_delete_mark("a"),
                              act_move_to_mark("b"), act_insert("#")}),
         "a#");

  // GreaterThan compares the variable's resolved value.
  expect("greater-than", run({act_set("N", "5"), act_cond(cond_gt("N", 3), {act_insert("big")})}),
         "big");

  // Relative delete shifts marks left of the removed region to its start.
  expect("delete-shift", run({act_insert("abc"), act_mark("m"), act_move_end(), act_delete(2),
                              act_move_to_mark("m"), act_insert("#")}),
         "a#");

  if (failures != 0) {
    std::cerr << failures << " unit test(s) failed\n";
    return 1;
  }
  std::cout << "PASS 7 unit tests\n";
  return 0;
}
