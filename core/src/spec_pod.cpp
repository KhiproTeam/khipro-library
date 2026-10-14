// Materializes POD layout tables (emitted by tools/codegen) into the runtime
// Spec. Fail-loud: malformed tables abort with a reason — a corrupted or
// hand-edited generated file must never pass silently.

#include "spec_pod.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace khipro::internal {

namespace {

constexpr U32 STRIDE_MAP = 4;
constexpr U32 STRIDE_ENTRY = 4;
constexpr U32 STRIDE_ACTION = 9;
constexpr U32 STRIDE_BRANCH = 3;
constexpr U32 STRIDE_COND = 6;
constexpr U32 STRIDE_STATE = 6;
constexpr U32 STRIDE_RULE = 4;

constexpr U32 MAX_DEPTH = 64;

[[noreturn]] void pod_fail(const char* what) {
  std::fprintf(stderr, "khipro: corrupted spec tables: %s\n", what);
  std::abort();
}

struct Loader {
  const PodLayout& L;

  std::string str(U32 off, U32 len) const {
    if (off > L.str_len || len > L.str_len - off) {
      pod_fail("string reference out of range");
    }
    return std::string(reinterpret_cast<const char*>(L.str + off), len);
  }

  Condition cond(U32 i, U32 depth) const {
    if (depth > MAX_DEPTH) {
      pod_fail("condition nesting too deep");
    }
    if (i >= L.cond_cnt) {
      pod_fail("condition index out of range");
    }
    const U32* c = L.cond + i * STRIDE_COND;
    Condition out;
    switch (c[0]) {
      case 0:
        out.type = ConditionType::Always;
        return out;
      case 1:
        out.type = ConditionType::Equals;
        out.variable = str(c[1], c[2]);
        out.value = static_cast<int>(c[3]);
        return out;
      case 2:
        out.type = ConditionType::GreaterThan;
        out.variable = str(c[1], c[2]);
        out.value = static_cast<int>(c[3]);
        return out;
      case 3:
      case 4: {
        out.type = (c[0] == 3) ? ConditionType::And : ConditionType::Or;
        if (c[4] == POD_NO_REF || c[5] == POD_NO_REF || c[4] >= L.cond_cnt ||
            c[5] >= L.cond_cnt) {
          pod_fail("condition operand out of range");
        }
        out.left = std::make_shared<Condition>(cond(c[4], depth + 1));
        out.right = std::make_shared<Condition>(cond(c[5], depth + 1));
        return out;
      }
      default:
        pod_fail("unknown condition type");
    }
  }

  void load_actions(U32 off, U32 cnt, std::vector<Action>* out, U32 depth) const {
    // 64-bit compares: off + cnt must not wrap modulo 2^32, or a corrupted
    // table would pass the check and walk wild memory below.
    if (off == POD_NO_REF ||
        static_cast<std::uint64_t>(off) + cnt > L.action_cnt) {
      pod_fail("action range out of range");
    }
    for (U32 i = 0; i < cnt; ++i) {
      const U32* a = L.action + static_cast<std::size_t>(off + i) * STRIDE_ACTION;
      if (a[0] > static_cast<U32>(ActionType::Undo)) {
        pod_fail("unknown action type");
      }
      Action loaded;
      loaded.type = static_cast<ActionType>(a[0]);
      loaded.n = static_cast<int>(a[1]);
      loaded.flag = a[2] != 0;
      loaded.s1 = str(a[3], a[4]);
      loaded.s2 = str(a[5], a[6]);
      if (loaded.type == ActionType::CondBranch) {
        const U32 br_off = a[7];
        const U32 br_cnt = a[8];
        if (br_off == POD_NO_REF ||
            static_cast<std::uint64_t>(br_off) + br_cnt > L.branch_cnt) {
          pod_fail("branch range out of range");
        }
        U32 child_rows = 0;
        for (U32 b = 0; b < br_cnt; ++b) {
          const U32* br = L.branch + static_cast<std::size_t>(br_off + b) * STRIDE_BRANCH;
          CondBranch cb;
          cb.condition = cond(br[0], depth + 1);
          load_actions(br[1], br[2], &cb.actions, depth + 1);
          // The branch's rows are part of THIS list's linear range — account
          // for them so they are not loaded twice.
          child_rows += br[2];
          loaded.branches.push_back(std::move(cb));
        }
        i += child_rows;
      }
      out->push_back(std::move(loaded));
    }
  }

  void load_entries(U32 off, U32 cnt, std::vector<MapEntry>* out, U32 depth) const {
    if (off == POD_NO_REF ||
        static_cast<std::uint64_t>(off) + cnt > L.entry_cnt) {
      pod_fail("entry range out of range");
    }
    for (U32 i = 0; i < cnt; ++i) {
      const U32* e = L.entry + static_cast<std::size_t>(off + i) * STRIDE_ENTRY;
      MapEntry entry;
      entry.key = str(e[0], e[1]);
      load_actions(e[2], e[3], &entry.actions, depth);
      out->push_back(std::move(entry));
    }
  }

  void load_rules(U32 off, U32 cnt, std::vector<StateRule>* out) const {
    if (off == POD_NO_REF ||
        static_cast<std::uint64_t>(off) + cnt > L.rule_cnt) {
      pod_fail("rule range out of range");
    }
    for (U32 i = 0; i < cnt; ++i) {
      const U32* r = L.rule + static_cast<std::size_t>(off + i) * STRIDE_RULE;
      StateRule rule;
      rule.map_name = str(r[0], r[1]);
      load_actions(r[2], r[3], &rule.actions, 0);
      out->push_back(std::move(rule));
    }
  }

  void load_maps(std::unordered_map<std::string, std::vector<MapEntry>>* out) const {
    for (U32 i = 0; i < L.map_cnt; ++i) {
      const U32* m = L.map + i * STRIDE_MAP;
      std::vector<MapEntry> entries;
      load_entries(m[2], m[3], &entries, 0);
      (*out)[str(m[0], m[1])] = std::move(entries);
    }
  }

  void load_states(std::vector<StateDef>* out) const {
    for (U32 i = 0; i < L.state_cnt; ++i) {
      const U32* s = L.state + i * STRIDE_STATE;
      StateDef def;
      def.name = str(s[0], s[1]);
      load_actions(s[2], s[3], &def.entry_actions, 0);
      load_rules(s[4], s[5], &def.rules);
      out->push_back(std::move(def));
    }
  }
};

}  // namespace

Spec spec_from_pod(const PodLayout& layout) {
  Loader loader{layout};
  Spec spec;
  loader.load_maps(&spec.maps);
  std::vector<StateDef> states;
  loader.load_states(&states);
  for (auto& def : states) {
    spec.states[def.name] = std::move(def);
  }
  return spec;
}

}  // namespace khipro::internal
