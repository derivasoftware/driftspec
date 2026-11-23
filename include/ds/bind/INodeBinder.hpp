#ifndef DRIFTSPEC_BIND_INODEBINDER_HPP
#define DRIFTSPEC_BIND_INODEBINDER_HPP

#include <string>
#include <vector>

#include "tree_sitter/api.h"

namespace ds {
namespace bind {

struct BindContext;

/// What binds one construct of the language (DS-355): the kinds of node it takes, and how one of them is declared into
/// the model. A binder keeps nothing between nodes; it writes only through the context it is given.
class INodeBinder {
 public:
  virtual ~INodeBinder() = default;
  /// The grammar's names of the nodes it binds: "requirement", "class".
  virtual std::vector<std::string> kinds() const = 0;
  /// Binds one node of those kinds, in the place the context says.
  virtual void bind(TSNode node, const BindContext& ctx) const = 0;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_INODEBINDER_HPP
