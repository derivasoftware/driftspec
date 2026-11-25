#ifndef DRIFTSPEC_CHECK_SELECTION_HPP
#define DRIFTSPEC_CHECK_SELECTION_HPP

#include <set>
#include <vector>

namespace ds {
namespace check {

/// Each step of the check, by what it does (DS-376): what a selection is made of. StepFactory::of pairs each with the step
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
  Order,
  Walkthroughs  ///< what each walkthrough's steps show and light (DS-469)
};

/// The steps of the check a load runs (DS-376): chosen by the one that knows what the load is for, the Api, and read
/// by StepFactory::of, which makes them and every step they need. The checker runs what it is given and knows nothing of it.
class Selection {
 public:
  /// Every step: what the findings, the export, a query and the viewer read.
  static Selection every();
  /// The steps named, and no other.
  static Selection of(const std::vector<Step>& steps);
  /// The same steps and one more.
  Selection with(Step step) const;
  /// The steps of both (DS-408): what a workspace has run for every use case it was asked.
  Selection joined(const Selection& other) const;
  bool has(Step step) const;
  /// Whether every step of `other` is among these: a load that ran these needs no other for it.
  bool covers(const Selection& other) const;

 private:
  std::set<Step> steps_;
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_SELECTION_HPP
