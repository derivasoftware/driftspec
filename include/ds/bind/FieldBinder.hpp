#ifndef DRIFTSPEC_BIND_FIELDBINDER_HPP
#define DRIFTSPEC_BIND_FIELDBINDER_HPP

#include <string>
#include <vector>

#include "ds/bind/BindContext.hpp"

namespace ds {
namespace bind {

/// The `key: value` fields every construct may write (DS-356): read into the entity, a repeated key accumulating.
class FieldBinder {
 public:
  /// The fields written in `node`, into `out_entity`.
  void bind(TSNode node, const BindContext& ctx, model::Entity& out_entity) const;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_FIELDBINDER_HPP
