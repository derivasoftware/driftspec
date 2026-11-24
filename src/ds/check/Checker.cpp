#include "ds/check/Checker.hpp"

#include <set>
#include <stdexcept>

#include "ds/check/Context.hpp"
#include "ds/model/Metamodel.hpp"

namespace ds {
namespace check {

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

void Checker::run(const bind::Notes& notes) {
  context_.reset(notes);  // the steps read the language from it (DS-351); nothing of a run before stays (DS-425)
  model::Model& model = context_.model();
  for (const auto& key : order())
    for (const auto& f : steps_.at(key)->run()) model.report(f);  // in the order the steps run
  model.cover();  // what is not yet written is no finding (Owner, 2026-11-15; DS-216): the model keeps it apart
}

std::unique_ptr<IChecker> CheckerFactory::make(model::Model& model, const model::Metamodel& metamodel, const Selection& selection) {
  return std::make_unique<SelectedChecker>(model, metamodel, selection);
}

}  // namespace check
}  // namespace ds
