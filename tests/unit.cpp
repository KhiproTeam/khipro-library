// Engine unit tests for the mark semantics the dark layouts rely on, plus
// direct tests for the POD table loader (spec_from_pod).

#include <sys/wait.h>
#include <unistd.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include "engine.h"
#include "spec.h"
#include "spec_generated.h"
#include "spec_pod.h"

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

// ---------------------------------------------------------------------------
// POD loader tests (spec_from_pod)
// ---------------------------------------------------------------------------

namespace {

int checks = 0;

// A minimal valid layout: map "m" with one entry "q" -> Insert "x", and a
// state "init" with a single rule on "m". Hand-built so malformed variants
// can twist a single field.
struct PodTables {
  static constexpr const char* kStr = "m\0q\0x\0init\0";  // 11 bytes
  static constexpr U32 kStrLen = 11;

  U32 map[4] = {0, 1, 0, 1};   // name "m", 1 entry @0
  U32 entry[4] = {2, 1, 0, 1}; // key "q", 1 action @0
  // Insert "x": type, n, flag, s1o, s1l, s2o, s2l, br_off, br_cnt
  U32 action[9] = {1, 0, 0, 4, 1, 0, 0, POD_NO_REF, 0};
  U32 branch[27] = {};         // 9 padding rows (wrap-target headroom)
  U32 cond[6] = {0, 0, 0, 0, 0, 0};  // Always
  U32 state[6] = {6, 4, 0, 0, 0, 1}; // "init", no entry actions, 1 rule
  U32 rule[4] = {0, 1, 0, 0};        // "m", no rule actions
};

PodLayout base_layout(const PodTables& t) {
  PodLayout L{};
  L.str = reinterpret_cast<const unsigned char*>(PodTables::kStr);
  L.str_len = PodTables::kStrLen;
  L.map = t.map;        L.map_cnt = 1;
  L.entry = t.entry;    L.entry_cnt = 1;
  L.action = t.action;  L.action_cnt = 1;
  L.branch = t.branch;  L.branch_cnt = 9;
  L.cond = t.cond;      L.cond_cnt = 1;
  L.state = t.state;    L.state_cnt = 1;
  L.rule = t.rule;      L.rule_cnt = 1;
  return L;
}

// Runs fn() in a forked child and asserts it dies from abort() — the loader's
// fail-loud contract. A segfault or clean exit both count as failures.
#ifndef _WIN32
void expect_abort(const char* label, void (*fn)()) {
  std::fflush(nullptr);
  const pid_t pid = fork();
  if (pid == 0) {
    std::freopen("/dev/null", "w", stderr);  // the signal is the assertion
    fn();
    std::_Exit(0);
  }
  int status = 0;
  waitpid(pid, &status, 0);
  if (!(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT)) {
    std::cerr << label << ": expected clean abort, got "
              << (WIFSIGNALED(status) ? "signal " + std::to_string(WTERMSIG(status))
                                      : "exit " + std::to_string(WEXITSTATUS(status)))
              << "\n";
    ++failures;
  }
  ++checks;
}
#endif

void aborts_on_bad_action_type() {
  PodTables t;
  t.action[0] = 99;
  spec_from_pod(base_layout(t));
}

void aborts_on_bad_string_ref() {
  PodTables t;
  t.entry[1] = 50;  // key length runs past the string pool
  spec_from_pod(base_layout(t));
}

// off + cnt wraps modulo 2^32 to exactly action_cnt, defeating the range
// check; the subsequent row walk must still not read wild memory.
void aborts_on_wrapped_action_range() {
  PodTables t;
  t.entry[2] = 0xFFFFFFF3u;
  t.entry[3] = 14;  // 0xFFFFFFF3 + 14 (mod 2^32) == 1 == action_cnt
  spec_from_pod(base_layout(t));
}

void aborts_on_wrapped_branch_range() {
  PodTables t;
  t.action[0] = 9;  // CondBranch
  t.action[7] = 0xFFFFFFF7u;
  t.action[8] = 16;  // 0xFFFFFFF7 + 16 (mod 2^32) == 7 <= branch_cnt
  spec_from_pod(base_layout(t));
}

void aborts_on_cond_operand_out_of_range() {
  PodTables t;
  t.cond[0] = 3;  // And
  t.cond[4] = 5;  // left operand >= cond_cnt
  t.action[0] = 9;
  t.action[7] = 0;
  t.action[8] = 1;
  spec_from_pod(base_layout(t));
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
  checks += 7;

  // --- POD loader (spec_from_pod) -----------------------------------------
  // A hand-built minimal layout materializes into a working engine.
  {
    Engine engine(spec_from_pod(base_layout(PodTables{})));
    expect("pod-minimal", engine.convert("q"), "x");
  }
  ++checks;

  // All four shipped layouts materialize through spec_from_pod.
  if (classic_spec().states.empty() || classic_touch_spec().states.empty() ||
      dark_spec().states.empty() || dark_touch_spec().states.empty()) {
    std::cerr << "pod-shipped: a materialized layout has no states\n";
    ++failures;
  }
  ++checks;

#ifndef _WIN32
  expect_abort("pod-abort-bad-action-type", aborts_on_bad_action_type);
  expect_abort("pod-abort-bad-string-ref", aborts_on_bad_string_ref);
  expect_abort("pod-abort-wrapped-action-range", aborts_on_wrapped_action_range);
  expect_abort("pod-abort-wrapped-branch-range", aborts_on_wrapped_branch_range);
  expect_abort("pod-abort-cond-operand-out-of-range", aborts_on_cond_operand_out_of_range);
#endif

  // --- m17n incremental-prefix semantics -----------------------------------
  // Real m17n matches keys incrementally: when a shorter key of the state is
  // extended to a longer one, the shorter key fires first and its text is
  // rolled back, but its Set actions persist into the longer key's run
  // (probed against libm17n 1.8.7; cross-map leak confirmed).
  {
    const std::string zwj = "\xE2\x80\x8D";  // U+200D, the ra-fixup joiner

    // Same-map chain: kar "ae" (set KAR 1 + insert) extended by "aef"; the
    // state rule observes KAR == 1 — the leaked set — on the winner's run.
    {
      Spec spec;
      spec.maps["kar"] = {MapEntry{"ae", {act_set("KAR", "1"), act_insert("্যা")}},
                          MapEntry{"aef", {act_insert("অ্যা")}}};
      spec.states["init"] = StateDef{"init", {},
                                     {StateRule{"kar", {act_cond(cond_eq("KAR", 1), {act_insert("*")})}}}};
      Engine engine(std::move(spec));
      expect("prefix-leak-same-map", engine.convert("aef"), "অ্যা*");
    }

    // Cross-map chain: the prefix lives in a different state-listed map than
    // the winner (m17n leaks across all maps of the current state).
    {
      Spec spec;
      spec.maps["m1"] = {MapEntry{"ae", {act_set("K", "5"), act_insert("A")}}};
      spec.maps["m2"] = {MapEntry{"aef", {act_insert("B")}}};
      spec.states["init"] = StateDef{"init", {},
                                     {StateRule{"m1", {}},
                                      StateRule{"m2", {act_cond(cond_eq("K", 5), {act_insert("!")})}}}};
      Engine engine(std::move(spec));
      expect("prefix-leak-cross-map", engine.convert("aef"), "B!");
    }

    // The real classic layout: raef must carry the ra-fixup ZWJ like m17n.
    {
      Engine engine(classic_spec());
      expect("classic-raef-zwj", engine.convert("raef"), "রঅ" + zwj + "্যা");
      expect("classic-rae-zwj", engine.convert("rae"), "র" + zwj + "্যা");
    }
    checks += 4;
  }

  if (failures != 0) {
    std::cerr << failures << " unit test(s) failed\n";
    return 1;
  }
  std::cout << "PASS " << checks << " unit tests\n";
  return 0;
}
