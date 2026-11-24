#ifndef DRIFTSPEC_CHECK_SELECTION_HPP
#define DRIFTSPEC_CHECK_SELECTION_HPP

#include <set>
#include <vector>

namespace ds {
namespace check {

/// Each step of the check, by what it does (DS-376): what a selection is made of. Steps::of pairs each with the step
/// it makes, so the names the steps are keyed by stay theirs.
enum class Step {
  Linker,
  Behaviour,
  Derivation,
  Failures,
  Interfaces,
  UseCases,
  Diagrams,
  Scenarios,
  FailureChecks,
  Structure,
  Dependencies,
  Rules,
  Levels,
  States,
  Order
};

/// The steps of the check a load runs (DS-376): chosen by the one that knows what the load is for, the Api, and read
/// by Steps::of, which makes them and every step they need. The checker runs what it is given and knows nothing of it.
class Selection {
 public:
  /// Every step: what the findings, the export, a query and the viewer read.
  static Selection every();
  /// The steps named, and no other.
  static Selection of(const std::vector<Step>& steps);
  /// The same steps and one more.
  Selection with(Step step) const;
  bool has(Step step) const;

 private:
  std::set<Step> steps_;
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_SELECTION_HPP
