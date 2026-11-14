#ifndef DRIFTSPEC_CHECK_STEP_HPP
#define DRIFTSPEC_CHECK_STEP_HPP

#include <string>
#include <vector>

#include "ds/model/Model.hpp"

namespace ds {
namespace check {

class Context;

/// One step of the check (DS-138): its key, the steps it needs done before it, and what it does to the
/// model it is given. The checker orders the steps by what they need and runs them, knowing none.
class IStep {
 public:
  virtual ~IStep() = default;
  virtual std::string name() const = 0;
  virtual std::vector<std::string> needs() const = 0;
  /// Run over the context and give back what was found; the checker adds it to the model, in order.
  virtual std::vector<model::Finding> run(Context& out_context) = 0;
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_STEP_HPP
