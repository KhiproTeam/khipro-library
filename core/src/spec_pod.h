#pragma once

// POD layout tables. tools/codegen emits these as plain integer arrays plus
// a string pool; spec_pod.cpp materializes them into the runtime Spec.
// All references are (offset, length) pairs into the string pool, or index
// ranges into sibling tables. NO_REF marks an absent reference.

#include "spec.h"

#include <cstdint>

namespace khipro::internal {

using U32 = std::uint32_t;

inline constexpr U32 POD_NO_REF = 0xFFFFFFFFu;

struct PodRef {
  U32 off;
  U32 len;
};

struct PodLayout {
  const unsigned char* str;
  U32 str_len;
  const U32* map;    U32 map_cnt;    // stride 4: name_off, name_len, entry_off, entry_cnt
  const U32* entry;  U32 entry_cnt;  // stride 4: key_off, key_len, act_off, act_cnt
  const U32* action; U32 action_cnt; // stride 9: type, n, flag, s1o, s1l, s2o, s2l, br_off, br_cnt
  const U32* branch; U32 branch_cnt; // stride 3: cond, act_off, act_cnt
  const U32* cond;   U32 cond_cnt;   // stride 6: type, var_off, var_len, value, left, right
  const U32* state;  U32 state_cnt;  // stride 6: name_off, name_len, entry_off, entry_cnt, rule_off, rule_cnt
  const U32* rule;   U32 rule_cnt;   // stride 4: map_off, map_len, act_off, act_cnt
};

// Materializes the layout. Aborts loudly on malformed tables.
Spec spec_from_pod(const PodLayout& layout);

}  // namespace khipro::internal
