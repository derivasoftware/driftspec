#include <string>
#include <vector>

#include "ds/check/Steps.hpp"
#include "ds/machine/Machine.hpp"

namespace ds {
namespace check {

std::string StateRules::name() const { return "states"; }

std::vector<std::string> StateRules::needs() const { return {"linker"}; }

std::vector<model::Finding> StateRules::check(const Context& context) {
  std::vector<model::Finding> found;
  for (const auto& pair : context.model().entities()) {
    const model::Entity& e = pair.second;
    if (e.kind != model::Kind::State || e.field("rules").empty()) continue;
    for (const model::Finding& fault : machine::Machine(context.model(), e).faults(context.metamodel())) found.push_back(fault);
  }
  return found;
}

}  // namespace check
}  // namespace ds
