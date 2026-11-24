#ifndef DRIFTSPEC_BIND_ANNOTATIONBINDER_HPP
#define DRIFTSPEC_BIND_ANNOTATIONBINDER_HPP

#include <string>
#include <vector>

#include "ds/bind/BindContext.hpp"

namespace ds {
namespace bind {

/// The annotations every construct may write (DS-356): each a reference from the entity for the checker to resolve, with
/// the relation the metamodel gives its word; a word the language does not know is kept as a link and said (DS-210).
class AnnotationBinder {
 public:
  /// The annotations written in `node`, noted from `from` and read in `scope`.
  void bind(TSNode node, const BindContext& ctx, const std::string& from, const std::string& scope) const;

 private:
  /// What an annotation means from an entity of `from`'s kind: its relation, or the one it has on a verification.
  static std::string relationOf(model::Kind from, const model::AnnotationSpec& spec);
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_ANNOTATIONBINDER_HPP
