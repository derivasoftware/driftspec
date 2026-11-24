#ifndef DRIFTSPEC_BIND_DISPATCH_HPP
#define DRIFTSPEC_BIND_DISPATCH_HPP

#include <vector>

#include "ds/bind/INodeBinder.hpp"
#include "tree_sitter/api.h"

namespace ds {
namespace bind {

/// Which binder binds a node (DS-355): a table indexed by the grammar's symbol for the node's kind, built once from the
/// binders' kinds. Finding the binder of a node reads its symbol and one slot; no name is compared or built. A node
/// no binder takes (a name, a field, an annotation, a comment) binds nothing by itself: its construct reads it.
class Dispatch {
 public:
  /// The table of `language` for `binders`. Throws when a kind names no node of the grammar or two binders take it.
  Dispatch(const TSLanguage& language, const std::vector<const INodeBinder*>& binders);

  /// The binder of a node: the one its kind is given to; null when no binder takes it.
  const INodeBinder* of(TSNode node) const {
    const TSSymbol symbol = ts_node_symbol(node);
    return symbol < bySymbol_.size() ? bySymbol_[symbol] : nullptr;
  }
  /// Binds the named children of `node` in the order they are written, each by its binder.
  void bindChildren(TSNode node, const BindContext& ctx) const;

 private:
  std::vector<const INodeBinder*> bySymbol_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_DISPATCH_HPP
