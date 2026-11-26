#ifndef DRIFTSPEC_CHECK_COMPILEDBODY_HPP
#define DRIFTSPEC_CHECK_COMPILEDBODY_HPP

#include <string>
#include <vector>

namespace ds {
namespace check {

/// A body of a formula as the solver reads it (DS-507): each term's slot, as Variables compiled it, and what its
/// constants stand for, resolved in the facts the first time the body is solved. The facts record every constant before
/// any solve, so a name is looked up in the facts once per body, never at every step.
struct CompiledBody {
  const std::vector<std::vector<int>>* slots = nullptr;  ///< per literal and term: its slot, as Variables compiled it
  /// per literal and term: what a constant stands for ("" when it names nothing); null for a variable or a value
  std::vector<std::vector<const std::string*>> constants;
  std::vector<const std::string*> calls;  ///< per literal: the definition a call applies; null for none
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_COMPILEDBODY_HPP
