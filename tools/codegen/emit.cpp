// Emitter: serializes parsed layouts into compact POD tables (see
// core/src/spec_pod.h) plus a shared string pool, and emits them with
// per-layout accessors that materialize the runtime Spec on first use.
// The output file contains data, not construction code. Emission is
// deterministic: same inputs produce byte-identical output.

#include "emit.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace khipro::codegen {

namespace {

using khipro::internal::Action;
using khipro::internal::CondBranch;
using khipro::internal::Condition;
using khipro::internal::ActionType;
using khipro::internal::ConditionType;
using khipro::internal::MapEntry;
using khipro::internal::Spec;
using khipro::internal::StateDef;
using khipro::internal::StateRule;

constexpr std::uint32_t NO_REF = 0xFFFFFFFFu;

class StringPool {
 public:
  std::pair<std::uint32_t, std::uint32_t> intern(const std::string& s) {
    auto it = index_.find(s);
    if (it != index_.end()) {
      return it->second;
    }
    const auto off = static_cast<std::uint32_t>(bytes_.size());
    const auto len = static_cast<std::uint32_t>(s.size());
    bytes_.insert(bytes_.end(), s.begin(), s.end());
    const auto ref = std::make_pair(off, len);
    index_.emplace(s, ref);
    return ref;
  }

  const std::vector<unsigned char>& bytes() const { return bytes_; }

 private:
  std::vector<unsigned char> bytes_;
  std::unordered_map<std::string, std::pair<std::uint32_t, std::uint32_t>> index_;
};

class U32Table {
 public:
  explicit U32Table(std::uint32_t stride) : stride_(stride) {}

  std::uint32_t append(const std::vector<std::uint32_t>& row) {
    const auto index = static_cast<std::uint32_t>(rows_.size() / stride_);
    rows_.insert(rows_.end(), row.begin(), row.end());
    return index;
  }
  std::uint32_t start() const { return count(); }
  std::uint32_t& at(std::uint32_t index, std::uint32_t field) {
    return rows_[index * stride_ + field];
  }
  std::uint32_t count() const { return static_cast<std::uint32_t>(rows_.size() / stride_); }
  std::uint32_t stride() const { return stride_; }
  const std::vector<std::uint32_t>& data() const { return rows_; }

 private:
  std::uint32_t stride_;
  std::vector<std::uint32_t> rows_;
};

struct Tables {
  U32Table maps{4};      // name_off, name_len, entry_off, entry_cnt
  U32Table entries{4};   // key_off, key_len, act_off, act_cnt
  U32Table actions{9};   // type, n, flag, s1o, s1l, s2o, s2l, br_off, br_cnt
  U32Table branches{3};  // cond, act_off, act_cnt
  U32Table conds{6};     // type, var_off, var_len, value, left, right
  U32Table states{6};    // name_off, name_len, entry_off, entry_cnt, rule_off, rule_cnt
  U32Table rules{4};     // map_off, map_len, act_off, act_cnt
};

class Serializer {
 public:
  explicit Serializer(StringPool& pool) : pool_(pool) {}

  void serialize(const Spec& spec) {
    std::map<std::string, std::vector<MapEntry>> maps(spec.maps.begin(), spec.maps.end());
    for (const auto& [name, entries] : maps) {
      const auto name_ref = pool_.intern(name);
      const std::uint32_t entry_off = tables_.entries.start();
      for (const MapEntry& entry : entries) {
        const auto key = pool_.intern(entry.key);
        const auto act = write_list(entry.actions);
        tables_.entries.append({key.first, key.second, act.first, act.second});
      }
      const std::uint32_t entry_cnt = tables_.entries.count() - entry_off;
      tables_.maps.append({name_ref.first, name_ref.second, entry_off, entry_cnt});
    }

    std::map<std::string, StateDef> states(spec.states.begin(), spec.states.end());
    for (const auto& [name, state] : states) {
      const auto name_ref = pool_.intern(name);
      const auto entry_actions = write_list(state.entry_actions);
      const std::uint32_t rule_off = tables_.rules.start();
      for (const StateRule& rule : state.rules) {
        const auto map_ref = pool_.intern(rule.map_name);
        const auto act = write_list(rule.actions);
        tables_.rules.append({map_ref.first, map_ref.second, act.first, act.second});
      }
      const std::uint32_t rule_cnt = tables_.rules.count() - rule_off;
      tables_.states.append({name_ref.first, name_ref.second, entry_actions.first,
                      entry_actions.second, rule_off, rule_cnt});
    }
  }

  const Tables& tables() const { return tables_; }

 private:
  std::uint32_t write_cond(const Condition& cond) {
    switch (cond.type) {
      case ConditionType::Always:
        return tables_.conds.append({0u, NO_REF, 0, 0, NO_REF, NO_REF});
      case ConditionType::Equals: {
        const auto var = pool_.intern(cond.variable);
        return tables_.conds.append({1u, var.first, var.second,
                              static_cast<std::uint32_t>(cond.value), NO_REF, NO_REF});
      }
      case ConditionType::GreaterThan: {
        const auto var = pool_.intern(cond.variable);
        return tables_.conds.append({2u, var.first, var.second,
                              static_cast<std::uint32_t>(cond.value), NO_REF, NO_REF});
      }
      case ConditionType::And:
      case ConditionType::Or: {
        if (!cond.left || !cond.right) {
          // The parser cannot produce operand-less And/Or, but emitting NO_REF
          // here would only surface as a loader abort at library runtime —
          // reject at the tool where the context is known.
          std::fputs("codegen: And/Or condition missing an operand\n", stderr);
          std::exit(1);
        }
        const auto left = write_cond(*cond.left);
        const auto right = write_cond(*cond.right);
        const auto type = (cond.type == ConditionType::And) ? 3u : 4u;
        return tables_.conds.append({type, NO_REF, 0, 0, left, right});
      }
    }
    return NO_REF;
  }

  void write_action(const Action& a) {
    const auto s1 = pool_.intern(a.s1);
    const auto s2 = pool_.intern(a.s2);
    if (a.type == ActionType::CondBranch) {
      // Cond row first (placeholders, patched below); children follow it so
      // the loader's linear walk skips them after the recursion.
      const auto self = tables_.actions.append({static_cast<std::uint32_t>(ActionType::CondBranch),
                                                0u,
                                                0u,
                                                s1.first,
                                                s1.second,
                                                s2.first,
                                                s2.second,
                                                NO_REF,
                                                0u});
      // Child serialization first (nested conds append their own branch
      // rows beyond this point), then this cond's branch rows as ONE
      // contiguous block — so [br_off, br_cnt] never covers nested rows.
      std::vector<std::vector<std::uint32_t>> rows;
      rows.reserve(a.branches.size());
      for (const CondBranch& branch : a.branches) {
        const std::uint32_t cond = write_cond(branch.condition);
        const auto act = write_list(branch.actions);
        rows.push_back({cond, act.first, act.second});
      }
      // Children were serialized first (nested conds appended their own
      // branch rows along the way); this cond's rows form one contiguous
      // block AFTER all of that — [br_off, br_cnt] covers only its rows.
      const std::uint32_t br_off = tables_.branches.count();
      for (const auto& row : rows) {
        tables_.branches.append(row);
      }
      tables_.actions.at(self, 7) = br_off;
      tables_.actions.at(self, 8) = static_cast<std::uint32_t>(rows.size());
      return;
    }
    tables_.actions.append({static_cast<std::uint32_t>(a.type),
                            static_cast<std::uint32_t>(a.n),
                            a.flag ? 1u : 0u,
                            s1.first,
                            s1.second,
                            s2.first,
                            s2.second,
                            NO_REF,
                            0u});
  }

  std::pair<std::uint32_t, std::uint32_t> write_list(const std::vector<Action>& actions) {
    const std::uint32_t off = tables_.actions.start();
    for (const Action& a : actions) {
      write_action(a);
    }
    return {off, tables_.actions.count() - off};
  }

  StringPool& pool_;
  Tables tables_;
};

std::string u32_rows(const std::vector<std::uint32_t>& data, std::uint32_t stride) {
  std::string out;
  for (std::size_t i = 0; i < data.size(); ++i) {
    out += std::to_string(data[i]);
    out += ((i + 1) % stride == 0) ? ",\n    " : ", ";
  }
  return out;
}

std::string byte_rows(const std::vector<unsigned char>& bytes) {
  std::string out;
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    out += std::to_string(bytes[i]);
    out += ((i + 1) % 20 == 0) ? ",\n    " : ", ";
  }
  return out;
}

}  // namespace

void emit_spec_sources(const std::vector<LayoutInput>& layouts,
                       const std::vector<std::string>& versions, std::string* out_h,
                       std::string* out_cpp) {
  out_h->clear();
  out_cpp->clear();

  *out_h += "// GENERATED by tools/codegen from resources/*.mim. Do not edit.\n";
  *out_h += "#pragma once\n\n";
  const char* const version_macros[] = {
      "KHIPRO_LIBRARY_VERSION",
      "KHIPRO_CLASSIC_VERSION",
      "KHIPRO_TOUCHSCREEN_CLASSIC_VERSION",
      "KHIPRO_DARK_VERSION",
      "KHIPRO_TOUCHSCREEN_DARK_VERSION",
  };
  for (std::size_t i = 0; i < versions.size() && i < 5; ++i) {
    *out_h += std::string("#define ") + version_macros[i] + " \"" + versions[i] + "\"\n";
  }
  *out_h += "\n#include \"spec.h\"\n\nnamespace khipro::internal {\n";
  for (const LayoutInput& layout : layouts) {
    *out_h += "const Spec& " + layout.accessor + "_spec();\n";
  }
  *out_h += "}\n";

  *out_cpp += "// GENERATED by tools/codegen from resources/*.mim. Do not edit.\n";
  *out_cpp += "#include \"spec_generated.h\"\n\n";
  *out_cpp += "#include \"spec_pod.h\"\n\n";
  *out_cpp += "namespace khipro::internal {\n\nnamespace {\n\n";
  *out_cpp += "using khipro::internal::PodLayout;\n\n";

  StringPool pool;
  // unique_ptr, not by-value elements: `tables` below stores pointers into
  // the serializers, which would dangle if a growth reallocation moved them.
  std::vector<std::unique_ptr<Serializer>> serializers;
  serializers.reserve(layouts.size());
  std::vector<const Tables*> tables;
  tables.reserve(layouts.size());
  std::vector<std::string> uppers;
  uppers.reserve(layouts.size());
  for (const LayoutInput& layout : layouts) {
    serializers.push_back(std::make_unique<Serializer>(pool));
    serializers.back()->serialize(*layout.spec);
    tables.push_back(&serializers.back()->tables());

    std::string upper;
    for (char c : layout.accessor) {
      upper += (c == '-') ? '_' : static_cast<char>(toupper(static_cast<unsigned char>(c)));
    }
    uppers.push_back(upper);
  }

  *out_cpp += "// Shared string pool.\n";
  *out_cpp += "static const unsigned char STR[] = {\n    ";
  *out_cpp += byte_rows(pool.bytes());
  *out_cpp += "\n};\n\n";

  const char* table_names[] = {"MAP", "ENTRY", "ACTION", "BRANCH", "COND", "STATE", "RULE"};
  const std::uint32_t strides[] = {4, 4, 9, 3, 6, 6, 4};
  for (std::size_t i = 0; i < layouts.size(); ++i) {
    const std::string& upper = uppers[i];
    const Tables* t = tables[i];
    const std::vector<std::uint32_t>* data[] = {
        &t->maps.data(),          &t->entries.data(),  &t->actions.data(),
        &t->branches.data(),      &t->conds.data(),    &t->states.data(),
        &t->rules.data()};
    for (std::size_t j = 0; j < 7; ++j) {
      *out_cpp += "static const U32 " + upper + "_" + table_names[j] + "[] = {\n    ";
      *out_cpp += u32_rows(*data[j], strides[j]);
      *out_cpp += "\n};\n";
    }
    *out_cpp += "\n";
  }

  for (std::size_t i = 0; i < layouts.size(); ++i) {
    const std::string& upper = uppers[i];
    const Tables* t = tables[i];
    *out_cpp += "static const PodLayout POD_" + upper + " = {\n";
    *out_cpp += "    STR, sizeof(STR),\n";
    *out_cpp += "    " + upper + "_MAP, " + std::to_string(t->maps.count()) + ",\n";
    *out_cpp += "    " + upper + "_ENTRY, " + std::to_string(t->entries.count()) + ",\n";
    *out_cpp += "    " + upper + "_ACTION, " + std::to_string(t->actions.count()) + ",\n";
    *out_cpp += "    " + upper + "_BRANCH, " + std::to_string(t->branches.count()) + ",\n";
    *out_cpp += "    " + upper + "_COND, " + std::to_string(t->conds.count()) + ",\n";
    *out_cpp += "    " + upper + "_STATE, " + std::to_string(t->states.count()) + ",\n";
    *out_cpp += "    " + upper + "_RULE, " + std::to_string(t->rules.count()) + ",\n";
    *out_cpp += "};\n\n";
  }
  *out_cpp += "}  // namespace\n\n";

  for (std::size_t i = 0; i < layouts.size(); ++i) {
    *out_cpp += "const Spec& " + layouts[i].accessor + "_spec() {\n";
    *out_cpp += "  static const Spec spec = spec_from_pod(POD_" + uppers[i] + ");\n";
    *out_cpp += "  return spec;\n";
    *out_cpp += "}\n\n";
  }
  *out_cpp += "}  // namespace khipro::internal\n";
}

}  // namespace khipro::codegen
