#ifndef DRIFTSPEC_BIND_SIGNATUREBINDER_HPP
#define DRIFTSPEC_BIND_SIGNATUREBINDER_HPP

#include <string>
#include <vector>

#include "ds/bind/BindContext.hpp"
#include "ds/bind/TypeBinder.hpp"

namespace ds {
namespace bind {

/// The parameters and the return of an operation, a method or a function (DS-356): their text, and the use their types give.
class SignatureBinder {
 public:
  explicit SignatureBinder(const TypeBinder& types) : types_(types) {}
  /// The signature written in `node`, into `out_entity`, its types read in `scope`.
  void bind(TSNode node, const BindContext& ctx, model::Entity& out_entity, const std::string& scope,
            const std::vector<std::string>& params) const;

 private:
  const TypeBinder& types_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_SIGNATUREBINDER_HPP
