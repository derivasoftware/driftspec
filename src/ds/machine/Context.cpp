#include "ds/machine/Context.hpp"

#include <algorithm>
#include <vector>

#include "ds/machine/WrittenFields.hpp"

namespace ds {
namespace machine {

bool Context::allowed(const Subject& subject, const StateRule& rule, Context* out) {
  for (const Literal& l : rule.literals) {
    std::vector<std::string> all = subject.values(l.name);
    if (all.empty()) all = {l.value, "(another)"};  // not listable: what it asks, and anything else
    std::set<std::string> ok;
    if (l.op.empty()) ok = {"true"};
    if (l.op == "=") ok = {l.value};
    if (l.op == "!=") {
      ok.insert(all.begin(), all.end());
      ok.erase(l.value);
    }
    if (l.negated) {
      std::set<std::string> rest(all.begin(), all.end());
      for (const auto& v : ok) rest.erase(v);
      ok = rest;
    }
    const auto found = out->values_.find(l.name);
    if (found == out->values_.end()) {
      out->values_[l.name] = ok;
      continue;
    }
    std::set<std::string> both;
    for (const auto& v : found->second)
      if (ok.count(v)) both.insert(v);
    found->second = both;
  }
  return std::all_of(out->values_.begin(), out->values_.end(), [](const auto& entry) { return !entry.second.empty(); });
}

bool Context::together(const Context& b, Context* out) const {
  *out = *this;
  for (const auto& entry : b.values_) {
    const auto found = out->values_.find(entry.first);
    if (found == out->values_.end()) {
      out->values_[entry.first] = entry.second;
      continue;
    }
    std::set<std::string> both;
    for (const auto& v : found->second)
      if (entry.second.count(v)) both.insert(v);
    if (both.empty()) return false;
    found->second = both;
  }
  return true;
}

std::string Context::told() const {
  std::vector<std::string> parts;
  for (const auto& entry : values_) parts.push_back(entry.first + " = " + *entry.second.begin());
  return WrittenFields::joined(parts, ", ");
}

bool Context::within(const Context& rule) const {
  for (const auto& entry : rule.values_) {
    const auto known = values_.find(entry.first);
    if (known == values_.end()) return false;
    for (const auto& v : known->second)
      if (!entry.second.count(v)) return false;
  }
  return true;
}

}  // namespace machine
}  // namespace ds
