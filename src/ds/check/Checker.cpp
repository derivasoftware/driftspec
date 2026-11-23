#include "ds/check/Checker.hpp"

#include <set>
#include <stdexcept>

#include "ds/check/Context.hpp"
#include "ds/model/Metamodel.hpp"

namespace ds {
namespace check {

Checker::Checker(model::Model& model, const model::Metamodel& metamodel, const std::map<std::string, std::unique_ptr<IStep>>& steps)
    : model_(model), metamodel_(metamodel), resolver_(model), steps_(steps) {}

// each step after every step it needs; among the steps ready at once, by key, so the order is one
std::vector<std::string> Checker::order() const {
  std::vector<std::string> out;
  std::set<std::string> done;
  while (out.size() < steps_.size()) {
    bool progressed = false;
    for (const auto& s : steps_) {
      if (done.count(s.first)) continue;
      bool ready = true;
      for (const auto& need : s.second->needs()) ready = ready && (done.count(need) || !steps_.count(need));
      if (!ready) continue;
      out.push_back(s.first);
      done.insert(s.first);
      progressed = true;
      break;  // the first ready step by key, then look again from the start
    }
    if (!progressed) throw std::logic_error("the steps of the check need each other in a cycle");
  }
  return out;
}

void Checker::run(const bind::Binder& binder) {
  Context context(model_, resolver_, binder);
  for (const auto& key : order())
    for (const auto& f : steps_.at(key)->run(context)) model_.report(f);  // in the order the steps run
  // what is not yet written is no finding (Owner, 2026-11-15; DS-216): it is kept apart, as coverage
  std::vector<model::Finding> findings;
  for (auto& f : model_.findings()) {
    const model::CodeSpec* spec = metamodel_.codeOf(f.code);
    if (spec && spec->severity == "coverage") {
      f.severity = "coverage";
      model_.coverage().push_back(f);
    } else {
      findings.push_back(f);
    }
  }
  model_.findings().swap(findings);
}

}  // namespace check
}  // namespace ds
