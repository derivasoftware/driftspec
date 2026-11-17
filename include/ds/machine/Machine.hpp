#ifndef DRIFTSPEC_MACHINE_MACHINE_HPP
#define DRIFTSPEC_MACHINE_MACHINE_HPP

#include <string>
#include <vector>

#include "ds/model/Model.hpp"

namespace ds {
namespace machine {

/// A state machine as its author wrote it (DS-249), read once for whoever needs it: the check that holds its
/// rules to each other and to the structure, and the drawing. A state is computed by a rule over the context
/// of the machine's subject: what its attributes are worth. The rules of a machine exclude each other, so
/// that whatever the subject holds, it is in one state.
class Machine {
 public:
  Machine(const model::Model& model, const model::Entity& machine);

  /// The states, in the order written: those a rule computes, then those only a jump or the start names.
  std::vector<std::string> states() const;
  /// The rules that compute a state, as written; a state may have several, each one way of being in it.
  std::vector<std::string> rules(const std::string& state) const;
  /// The state the subject starts in: the one written, else the one its defaults are in; empty when the
  /// defaults are in no state or do not tell.
  std::string initial() const;
  /// What the rules say that cannot be: an attribute the subject does not hold, a value its type does not
  /// have, a rule that excludes itself, and two states that can hold at once, with a context in which they do.
  std::vector<model::Finding> faults() const;

 private:
  const model::Model& model_;
  const model::Entity& machine_;
  std::string subject_;
};

}  // namespace machine
}  // namespace ds

#endif  // DRIFTSPEC_MACHINE_MACHINE_HPP
