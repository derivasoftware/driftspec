#ifndef DRIFTSPEC_DIFF_DIFF_HPP
#define DRIFTSPEC_DIFF_DIFF_HPP

#include <map>
#include <string>
#include <vector>

#include "driftspec/model/Model.hpp"

namespace driftspec {
namespace diff {

/// One difference between two models, by identity.
struct Change {
  std::string kind;    ///< added, removed, changed, renamed
  std::string what;    ///< entity or relation
  std::string id;      ///< the identity (for a relation: "kind from -> to")
  std::string entity;  ///< the entity's kind name, for the report
  std::string scope;   ///< module, to group the report
  std::string detail;  ///< what changed: "params: a -> b; flags: owns -> refs"
  std::string from;    ///< for a rename: the old identity
};

/// The semantic difference between two workspaces: entities and relations by
/// identity, never lines. What did not change is not reported; a moved file
/// or a reordered member is not a change.
struct DiffResult {
  std::vector<Change> changes;
  std::vector<model::Finding> findings;  ///< process findings the diff raises (W-REVISION-STALE)

  /// Added, removed, changed counts.
  int added() const;
  int removed() const;
  int changed() const;
  bool empty() const { return changes.empty(); }
};

class Diff {
 public:
  /// Compare the before (`a`) and after (`b`) models.
  static DiffResult compare(const model::Model& a, const model::Model& b);

  /// The after model with every entity and relation marked by the diff
  /// (field `change` = added | removed | changed | renamed; removed ones
  /// brought over from the before model), so a diagram can draw the change.
  static model::Model marked(const model::Model& a, const model::Model& b, const DiffResult& diff);

  /// The report, grouped by module.
  static std::string report(const DiffResult& diff);
};

}  // namespace diff
}  // namespace driftspec

#endif  // DRIFTSPEC_DIFF_DIFF_HPP
