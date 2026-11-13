#ifndef DRIFTSPEC_QUERY_WALK_HPP
#define DRIFTSPEC_QUERY_WALK_HPP

#include <set>
#include <string>
#include <vector>

#include "ds/model/Model.hpp"

namespace ds {
namespace query {

/// One message a behaviour takes, and where it goes on: the behaviour its call enters and the
/// scenario it follows there (both empty when the call enters nothing).
struct Taken {
  std::string message;
  std::string next;
  std::string nextPath;
};

/// One step of a behaviour walked into what it calls: a message, at the depth it was reached.
struct Walked {
  std::string message;
  int depth = 1;
};

/// Every branch of a behaviour, as written: the path a sequence diagram draws whole.
extern const char* const kEveryBranch;

/// The ways through the behaviours of one model (DS-185).
class Walker {
 public:
  explicit Walker(const model::Model& model) : model_(model) {}

  /// The messages a behaviour takes along a scenario, in the order written (DS-130): every branch
  /// for kEveryBranch, the default path for "", the named one otherwise; out_kept, when given,
  /// receives the frames that scenario cannot tell apart. The one place that says where each call
  /// goes on.
  std::vector<Taken> steps(const std::string& sequence, const std::string& path, std::set<std::string>* out_kept = nullptr) const;

  /// A behaviour walked into what its calls enter, down to a depth: each message where it is taken,
  /// a behaviour entered once on each way down, so a cycle ends.
  std::vector<Walked> walk(const std::string& sequence, const std::string& path, int depth) const;

 private:
  const model::Model& model_;
};

}  // namespace query
}  // namespace ds

#endif  // DRIFTSPEC_QUERY_WALK_HPP
