#ifndef DRIFTSPEC_MACHINE_JUMP_HPP
#define DRIFTSPEC_MACHINE_JUMP_HPP

#include <string>
#include <vector>

#include "ds/model/Model.hpp"

namespace ds {
namespace machine {

/// A jump of a state machine as its author wrote it (DS-250): from a state to another, on an event; with a guard it
/// may also not happen.
struct Jump {
  std::string from;
  std::string to;
  std::string event;
  std::string guard;  ///< as written; "" when it has none

  /// The jumps of a machine, in the order written.
  static std::vector<Jump> of(const model::Entity& machine);
};

}  // namespace machine
}  // namespace ds

#endif  // DRIFTSPEC_MACHINE_JUMP_HPP
