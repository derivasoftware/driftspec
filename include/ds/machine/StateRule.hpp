#ifndef DRIFTSPEC_MACHINE_STATERULE_HPP
#define DRIFTSPEC_MACHINE_STATERULE_HPP

#include <string>
#include <vector>

#include "ds/machine/Literal.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace machine {

/// One rule of a state machine (DS-249): the state it computes and the literals that all hold when the subject is in it.
struct StateRule {
  std::string state;
  std::vector<Literal> literals;
  std::string text;  ///< as written
  int line = 0;

  /// The rules of a machine, in the order written.
  static std::vector<StateRule> of(const model::Entity& machine);
};

}  // namespace machine
}  // namespace ds

#endif  // DRIFTSPEC_MACHINE_STATERULE_HPP
