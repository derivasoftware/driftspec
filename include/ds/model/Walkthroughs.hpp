#ifndef DRIFTSPEC_MODEL_WALKTHROUGHS_HPP
#define DRIFTSPEC_MODEL_WALKTHROUGHS_HPP

#include <vector>

#include "ds/model/Model.hpp"
#include "ds/model/Walkthrough.hpp"

namespace ds {
namespace model {

/// The one place a walkthrough's steps are resolved (DS-471): from the steps the binder declared and the relations the
/// linker made of them. What does not resolve is left out; telling it is the check's (DS-469).
class Walkthroughs {
 public:
  /// Every walkthrough of the model, by identity, each with its steps in the order written.
  static std::vector<Walkthrough> of(const Model& model);
};

}  // namespace model
}  // namespace ds

#endif  // DRIFTSPEC_MODEL_WALKTHROUGHS_HPP
