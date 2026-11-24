#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <vector>

#include "Shared.hpp"
#include "ds/check/Evaluator.hpp"
#include "ds/check/Steps.hpp"
#include "ds/machine/Machine.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/model/Paths.hpp"

namespace ds {
namespace check {

std::string ScenarioCheck::name() const { return "scenarios"; }

std::vector<std::string> ScenarioCheck::needs() const {
  // what ContractCheck needed: the expansions behaviour typing made, over the links the linker made, and what
  // escapes each operation
  return {"linker", "behaviour", "failures"};
}

std::vector<model::Finding> ScenarioCheck::check(const Context& context) {
  context_ = &context;
  model_ = &context.model();
  metamodel_ = &context.metamodel();
  resolver_ = &context.resolver();
  found_.clear();
  checkScenarios();
  std::vector<model::Finding> out;
  out.swap(found_);
  return out;
}

void ScenarioCheck::checkScenarios() {
  // a scenario has one name: a branch of the behaviour, or a behaviour of its own, never both nor twice
  std::map<std::string, int> named;
  for (const auto& pair : model_->entities()) {
    const model::Entity& f = pair.second;
    if (f.kind != model::Kind::Frame || f.field("kind") == "branch") continue;
    // a loop or an opt names its scenario itself; an alt's sides name theirs, where each is written
    std::vector<const model::Entity*> namers{&f};
    if (f.field("kind") == "alt")
      namers = {model_->find(model::Identity(f.id).branch(false).text()), model_->find(model::Identity(f.id).branch(true).text())};
    for (const model::Entity* side : namers) {
      if (!side) continue;
      const std::string name = side->field("scenario");
      if (name.empty()) continue;
      const std::string base =
          model::Identity(model::Identity(f.scope).subject().text()).sequence().text();  // the behaviour, whatever scenario holds the frame
      if (++named[model::Identity(base).withScenario(name).text()] == 2 || model_->find(model::Identity(base).withScenario(name).text())) {
        found_.push_back(context_->metamodel().finding(
            model::Code::W_SCENARIO_TWICE, base + " names the scenario '" + name + "' twice: a scenario is one path", side->location));
      }
    }
  }
}

}  // namespace check
}  // namespace ds
