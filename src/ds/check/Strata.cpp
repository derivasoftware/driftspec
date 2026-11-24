#include "ds/check/Strata.hpp"

#include <functional>

namespace ds {
namespace check {

using Form = model::LiteralForm;

// A definition sits one stratum above every definition it negates; a negation through recursion has
// no stratum and is an error (the evaluation would not be decided).
std::vector<std::string> Strata::stratify(std::set<std::string>& invalid) {
  std::vector<std::string> definitions;
  for (const auto& f : facts_.formulas()) {
    if (facts_.kindIs(f.first, model::Kind::Definition)) {
      definitions.push_back(f.first);
      stratum_[f.first] = 0;
    }
  }
  const int ceiling = static_cast<int>(definitions.size());
  for (bool changed = true; changed;) {
    changed = false;
    for (const auto& d : definitions) {
      for (const auto& c : facts_.formulas().at(d).cases)
        for (const auto& l : c) {
          if (l.form != Form::Call) continue;
          const std::string callee = facts_.definitionOf(d, l.name);
          if (callee.empty()) continue;
          if (invalid.count(callee) && !invalid.count(d)) {
            invalid.insert(d);  // what rests on a broken definition is not evaluated either
            changed = true;
          }
          const int need = stratum_[callee] + (l.negated ? 1 : 0);
          if (stratum_[d] < need && stratum_[d] <= ceiling) {
            stratum_[d] = need;
            changed = true;
          }
        }
    }
  }
  std::vector<std::string> unstratified;
  for (const auto& d : definitions)
    if (stratum_[d] > ceiling) {
      unstratified.push_back(d);
      invalid.insert(d);
    }
  // a rule applying an invalid definition is not evaluated (the definition carries the finding)
  for (const auto& f : facts_.formulas()) {
    const auto rests = [&](const std::vector<model::Literal>& body) {
      for (const auto& l : body)
        if (l.form == Form::Call && invalid.count(facts_.definitionOf(f.first, l.name))) return true;
      return false;
    };
    bool broken = rests(f.second.where);
    for (const auto& c : f.second.claims) broken = broken || rests(c);
    if (broken) invalid.insert(f.first);
  }
  return unstratified;
}

std::map<int, std::vector<std::string>> Strata::levels(const std::set<std::string>& invalid) const {
  std::map<int, std::vector<std::string>> out;
  for (const auto& s : stratum_)
    if (!invalid.count(s.first)) out[s.second].push_back(s.first);
  return out;
}

std::vector<std::string> Strata::dependencyOrder(const std::vector<std::string>& definitions) const {
  const std::set<std::string> level(definitions.begin(), definitions.end());
  std::map<std::string, std::vector<std::string>> applies;  // what each definition applies within the stratum
  for (const auto& d : definitions)
    for (const auto& c : facts_.formulas().at(d).cases)
      for (const auto& l : c)
        if (l.form == Form::Call) {
          const std::string callee = facts_.definitionOf(d, l.name);
          if (level.count(callee)) applies[d].push_back(callee);
        }
  std::vector<std::string> order;
  std::map<std::string, int> state;  // 1 while its callees are visited, 2 once placed
  bool cycle = false;
  std::function<void(const std::string&)> visit = [&](const std::string& d) {
    if (state[d] == 2 || cycle) return;
    if (state[d] == 1) {
      cycle = true;  // it reaches itself: recursion
      return;
    }
    state[d] = 1;
    for (const auto& callee : applies[d]) visit(callee);
    state[d] = 2;
    order.push_back(d);
  };
  for (const auto& d : definitions) visit(d);
  return cycle ? std::vector<std::string>{} : order;
}

}  // namespace check
}  // namespace ds
