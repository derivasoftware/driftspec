#include "ds/check/Selection.hpp"

#include <algorithm>

namespace ds {
namespace check {

Selection Selection::every() {
  return of({Step::Linker, Step::Behaviour, Step::Derivation, Step::Failures, Step::Interfaces, Step::UseCases, Step::Diagrams,
             Step::Scenarios, Step::FailureChecks, Step::Structure, Step::Dependencies, Step::Rules, Step::Levels, Step::States,
             Step::Order, Step::Walkthroughs});
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

Selection Selection::joined(const Selection& other) const {
  Selection out = *this;
  out.steps_.insert(other.steps_.begin(), other.steps_.end());
  return out;
}

bool Selection::covers(const Selection& other) const {
  return std::includes(steps_.begin(), steps_.end(), other.steps_.begin(), other.steps_.end());
}

}  // namespace check
}  // namespace ds
