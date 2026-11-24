#include "ds/check/Steps.hpp"

namespace ds {
namespace check {

std::vector<model::Finding> ACheck::run(Context& out_context) { return check(out_context); }  // read only

// the one place that names each step: it pairs each with what it does, makes the ones a selection names and the
// ones they need, and keys them by name; the checker runs them through IStep, knowing none of them
std::map<std::string, std::unique_ptr<IStep>> StepFactory::of(const Selection& selection) {
  std::vector<std::pair<Step, std::unique_ptr<IStep>>> all;
  all.emplace_back(Step::Linker, std::make_unique<Linker>());
  all.emplace_back(Step::Behaviour, std::make_unique<BehaviourTyping>());
  all.emplace_back(Step::Interfaces, std::make_unique<InterfaceContracts>());
  all.emplace_back(Step::UseCases, std::make_unique<UseCaseContracts>());
  all.emplace_back(Step::Diagrams, std::make_unique<DiagramCheck>());
  all.emplace_back(Step::Scenarios, std::make_unique<ScenarioCheck>());
  all.emplace_back(Step::FailureChecks, std::make_unique<FailureCheck>());
  all.emplace_back(Step::Derivation, std::make_unique<Derivation>());
  all.emplace_back(Step::Structure, std::make_unique<StructureCheck>());
  all.emplace_back(Step::Dependencies, std::make_unique<DependencyCheck>());
  all.emplace_back(Step::Rules, std::make_unique<RuleCheck>());
  all.emplace_back(Step::Failures, std::make_unique<FailureFlow>());
  all.emplace_back(Step::Levels, std::make_unique<LevelCoherence>());
  all.emplace_back(Step::States, std::make_unique<StateRules>());
  all.emplace_back(Step::Order, std::make_unique<CallOrder>());
  std::map<std::string, std::unique_ptr<IStep>> made;
  std::vector<std::string> wanted;
  for (auto& s : all) {
    const std::string key = s.second->name();
    if (selection.has(s.first)) wanted.push_back(key);
    made.emplace(key, std::move(s.second));
  }
  // a step chosen brings every step it needs, so none runs without what it reads
  std::map<std::string, std::unique_ptr<IStep>> out;
  while (!wanted.empty()) {
    const std::string key = wanted.back();
    wanted.pop_back();
    const auto it = made.find(key);
    if (it == made.end()) continue;  // taken already
    for (const auto& need : it->second->needs()) wanted.push_back(need);
    out.emplace(key, std::move(it->second));
    made.erase(it);
  }
  return out;
}

}  // namespace check
}  // namespace ds
