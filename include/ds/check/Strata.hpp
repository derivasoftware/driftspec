#ifndef DRIFTSPEC_CHECK_STRATA_HPP
#define DRIFTSPEC_CHECK_STRATA_HPP

#include <map>
#include <set>
#include <string>
#include <vector>

#include "ds/check/Facts.hpp"

namespace ds {
namespace check {

/// The order the definitions are computed in (REQ-00096-1, DS-380): a definition sits one stratum above every
/// definition it negates, and within a stratum after those it applies. It reads the facts.
class Strata {
 public:
  explicit Strata(const Facts& facts) : facts_(facts) {}

  /// Places every definition in its stratum. What rests on an invalid definition becomes invalid too, and so does a
  /// rule that applies one; a definition that negates itself through recursion has no stratum. Returns those, in
  /// the order of the definitions, for the evaluator to report.
  std::vector<std::string> stratify(std::set<std::string>& invalid);
  /// The valid definitions of each stratum, lowest first.
  std::map<int, std::vector<std::string>> levels(const std::set<std::string>& invalid) const;
  /// The definitions of one stratum, each after the ones of the stratum it applies; empty when one of them reaches
  /// itself through the others (recursion), which is fixed by passes to its fixpoint instead (DS-338).
  std::vector<std::string> dependencyOrder(const std::vector<std::string>& definitions) const;

 private:
  const Facts& facts_;
  std::map<std::string, int> stratum_;
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_STRATA_HPP
