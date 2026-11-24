#ifndef DRIFTSPEC_BIND_MEMBERBINDER_HPP
#define DRIFTSPEC_BIND_MEMBERBINDER_HPP

#include <set>
#include <string>
#include <vector>

#include "ds/bind/BindContext.hpp"
#include "ds/bind/MemberBodyBinder.hpp"
#include "ds/bind/SignatureBinder.hpp"
#include "ds/bind/TypeBinder.hpp"

namespace ds {
namespace bind {

/// A member of a class (DS-356): a method, an attribute or an operator, with its signature or type, its modifiers
/// and its body; an operator in the class's header set is refined by its body.
class MemberBinder {
 public:
  MemberBinder(const TypeBinder& types, const SignatureBinder& signatures, const MemberBodyBinder& bodies)
      : types_(types), signatures_(signatures), bodies_(bodies) {}
  /// The member `m` of `cls`, whose template parameters are `params` and header operators `operators`.
  void bind(TSNode m, const BindContext& ctx, const model::Entity& cls, const std::vector<std::string>& params,
            const std::set<std::string>& operators) const;

 private:
  const TypeBinder& types_;
  const SignatureBinder& signatures_;
  const MemberBodyBinder& bodies_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_MEMBERBINDER_HPP
