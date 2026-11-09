#include "ds/check/Checker.hpp"

#include <set>
#include <stdexcept>

#include "ds/check/Context.hpp"

namespace ds {
namespace check {

Checker::Checker(model::Model& model, const std::map<std::string, std::unique_ptr<IStep>>& steps)
    : model_(model), resolver_(model), steps_(steps) {}

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
}

}  // namespace check
}  // namespace ds
