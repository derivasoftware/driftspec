#include "ds/check/Selection.hpp"

namespace ds {
namespace check {

Selection Selection::every() {
  return of({Step::Linker, Step::Behaviour, Step::Derivation, Step::Failures, Step::Contracts, Step::Structure, Step::Dependencies,
             Step::Rules, Step::Levels, Step::States, Step::Order});
}

Selection Selection::of(const std::vector<Step>& steps) {
  Selection out;
  out.steps_.insert(steps.begin(), steps.end());
  return out;
}

Selection Selection::with(Step step) const {
  Selection out = *this;
  out.steps_.insert(step);
  return out;
}

bool Selection::has(Step step) const { return steps_.count(step) != 0; }

}  // namespace check
}  // namespace ds
