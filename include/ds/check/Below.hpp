#ifndef DRIFTSPEC_CHECK_BELOW_HPP
#define DRIFTSPEC_CHECK_BELOW_HPP

#include <set>
#include <string>
#include <vector>

#include "ds/check/Crossing.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace check {

/// What the level below a scenario does, seen from the scenario's participants.
struct Below {
  std::vector<Crossing> crossings;  ///< in the order they happen
  std::set<std::string> entered;    ///< the participants something is seen to happen in
  std::set<std::string> open;       ///< those in which a call is made whose behaviour is not written
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_BELOW_HPP
