#ifndef DRIFTSPEC_BIND_EXTENSIONBINDER_HPP
#define DRIFTSPEC_BIND_EXTENSIONBINDER_HPP

#include <string>
#include <vector>

#include "ds/bind/AnnotationBinder.hpp"
#include "ds/bind/BindContext.hpp"

namespace ds {
namespace bind {

/// An extension block (DS-356): the same id written again with annotations only adds them to what is defined elsewhere,
/// and expects it when nothing has defined it yet (R6). Requirements, verifications, constraints and classes extend.
class ExtensionBinder {
 public:
  explicit ExtensionBinder(const AnnotationBinder& annotations) : annotations_(annotations) {}
  /// Whether `body` holds annotations only, its id `idNode` aside: the block extends what is defined elsewhere.
  static bool extends(TSNode body, TSNode idNode = TSNode{});
  /// The annotations of `node` added to `id`, which is expected when nothing has defined it yet.
  void bind(TSNode node, const BindContext& ctx, const std::string& id, model::Kind kind, const std::string& name, const std::string& scope,
            const model::Location& loc) const;

 private:
  const AnnotationBinder& annotations_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_EXTENSIONBINDER_HPP
