#include "ds/check/Steps.hpp"

namespace ds {
namespace check {

std::vector<model::Finding> ACheck::run(Context& out_context) { return check(out_context); }  // read only

// the one place that names each step: it makes them and keys them by name; the checker runs them
// through IStep, knowing none of them
std::map<std::string, std::unique_ptr<IStep>> steps() {
  std::vector<std::unique_ptr<IStep>> all;
  all.push_back(std::make_unique<Linker>());
  all.push_back(std::make_unique<BehaviourTyping>());
  all.push_back(std::make_unique<ContractCheck>());
  all.push_back(std::make_unique<Derivation>());
  all.push_back(std::make_unique<StructureCheck>());
  all.push_back(std::make_unique<DependencyCheck>());
  all.push_back(std::make_unique<RuleCheck>());
  all.push_back(std::make_unique<FailureFlow>());
  all.push_back(std::make_unique<LevelCoherence>());
  all.push_back(std::make_unique<StateRules>());
  std::map<std::string, std::unique_ptr<IStep>> out;
  for (auto& s : all) {
    const std::string key = s->name();
    out.emplace(key, std::move(s));
  }
  return out;
}

}  // namespace check
}  // namespace ds
