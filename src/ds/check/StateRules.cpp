#include <string>
#include <vector>

#include "ds/check/Steps.hpp"
#include "ds/machine/Machine.hpp"

namespace ds {
namespace check {

std::string StateRules::name() const { return "states"; }

std::vector<std::string> StateRules::needs() const { return {"linker"}; }

StateRules::StateRules(const Context& context) : context_(&context) {}

std::vector<model::Finding> StateRules::check() {
  std::vector<model::Finding> found;
  for (const auto& pair : context_->model().entities()) {
    const model::Entity& e = pair.second;
    if (e.kind != model::Kind::State || e.field("rules").empty()) continue;
    for (const model::Finding& fault : machine::Machine(context_->model(), e).faults(context_->metamodel())) found.push_back(fault);
  }
  return found;
}

}  // namespace check
}  // namespace ds
