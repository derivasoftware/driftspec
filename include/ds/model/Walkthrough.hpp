#ifndef DRIFTSPEC_MODEL_WALKTHROUGH_HPP
#define DRIFTSPEC_MODEL_WALKTHROUGH_HPP

#include <string>
#include <vector>

namespace ds {
namespace model {

/// One step of a walkthrough (DS-471): the diagram it shows and the scenario of it, what it lights and where its callout
/// points, and what it says.
struct WalkStep {
  std::string id;  ///< diagram:WALK_X::<n>
  int n = 0;       ///< its place in the walk, from 1
  std::string title;
  std::string show;                    ///< as written: the shown diagram's name
  std::string diagram;                 ///< the shown diagram's identity; "" when show names none
  std::string scenario;                ///< "" for the diagram as it is
  std::vector<std::string> focus;      ///< the identities it focuses, those that resolve, in the order written
  std::vector<std::string> highlight;  ///< the identities it highlights
  std::string at;                      ///< the identity its callout points at; "" for none
  std::string text;
};

/// A walkthrough as written, its steps resolved (DS-471): built once by Walkthroughs::of and written by each document
/// in its own convention, the export by identity and the scene by index.
struct Walkthrough {
  std::string id;  ///< diagram:WALK_X
  std::string name;
  std::string title;
  std::vector<WalkStep> steps;  ///< in the order written
};

}  // namespace model
}  // namespace ds

#endif  // DRIFTSPEC_MODEL_WALKTHROUGH_HPP
