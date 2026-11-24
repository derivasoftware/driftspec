#ifndef DRIFTSPEC_BIND_MEMBERBODYBINDER_HPP
#define DRIFTSPEC_BIND_MEMBERBODYBINDER_HPP

#include <string>
#include <vector>

#include "ds/bind/AnnotationBinder.hpp"
#include "ds/bind/BindContext.hpp"
#include "ds/bind/FieldBinder.hpp"

namespace ds {
namespace bind {

/// What a member or a function writes in its body (DS-356): its fields, its annotations, and the rules and
/// artefacts nested in it, which are about it (R12). Only the declaration the model kept takes the fields.
class MemberBodyBinder {
 public:
  MemberBodyBinder(const FieldBinder& fields, const AnnotationBinder& annotations) : fields_(fields), annotations_(annotations) {}
  /// The body of `member`, written for `out_entity` and read in `scope`.
  void bind(TSNode member, const BindContext& ctx, model::Entity& out_entity, const std::string& scope) const;

 private:
  const FieldBinder& fields_;
  const AnnotationBinder& annotations_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_MEMBERBODYBINDER_HPP
