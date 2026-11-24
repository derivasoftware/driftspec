#ifndef DRIFTSPEC_MACHINE_CALLS_HPP
#define DRIFTSPEC_MACHINE_CALLS_HPP

#include <map>
#include <set>
#include <string>
#include <vector>

#include "ds/machine/Jump.hpp"
#include "ds/machine/Machine.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace machine {

/// One walk of the behaviours written, for the calls of the events of one machine (DS-250).
class Calls {
 public:
  Calls(const model::Model& model, const std::string& subject, const std::vector<Jump>& jumps, const std::vector<std::string>& states)
      : model_(model), subject_(subject), jumps_(jumps), states_(states) {}
  /// The calls a behaviour makes, in the order written, added to what was found.
  void of(const model::Entity& sequence, std::vector<Call>& found) const;

 private:
  /// The states a receiver may be in, by the name it has in a behaviour; a name that is not there may be in any.
  using May = std::map<std::string, std::set<std::string>>;
  /// What a behaviour or a frame holds, in the order written. A frame may not happen: what comes after it
  /// finds its receivers as the frame leaves them or as they were; the sides of an alt are each one way.
  void walk(const model::Entity& sequence, const std::string& holder, May& may, std::vector<Call>& found) const;
  /// One message: a call of an event of the machine moves its receiver by the jumps on that event.
  void call(const model::Entity& sequence, const model::Entity& m, May& may, std::vector<Call>& found) const;
  /// A behaviour as a reader calls it: its subject, and its scenario when it has one.
  static std::string named(const model::Entity& sequence);
  /// What two ways through a behaviour leave between them: a receiver either way leaves in any state is in any.
  static May either(const May& a, const May& b);

  const model::Model& model_;
  const std::string& subject_;
  const std::vector<Jump>& jumps_;
  const std::vector<std::string>& states_;
};

}  // namespace machine
}  // namespace ds

#endif  // DRIFTSPEC_MACHINE_CALLS_HPP
