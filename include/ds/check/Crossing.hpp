#ifndef DRIFTSPEC_CHECK_CROSSING_HPP
#define DRIFTSPEC_CHECK_CROSSING_HPP

#include <set>
#include <string>
#include <vector>

#include "ds/model/Model.hpp"

namespace ds {
namespace check {

/// One message that crosses from a participant of the scenario to another.
struct Crossing {
  std::string from;
  std::string to;
  std::string op;     ///< the operation it calls; empty for a label
  std::string label;  ///< the operation as written
  model::Location location;
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_CROSSING_HPP
