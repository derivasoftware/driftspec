#ifndef DRIFTSPEC_BIND_SIGNATUREBINDER_HPP
#define DRIFTSPEC_BIND_SIGNATUREBINDER_HPP

#include <string>
#include <string_view>
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
  /// The identity an operation written in `node` takes (DS-455): `bare` (owner::name), or, when another operation of
  /// its owner is written with the same name, `bare(T1, T2)`, its parameter types as written, whitespace normalised.
  /// Decided where it is bound, from what is written beside it, so nothing is renamed after.
  static std::string identityOf(const parse::TreeReader& reader, TSNode node, const std::string& bare);
  /// A type as an overload's identity writes it (DS-455): trimmed, each run of whitespace one space.
  static std::string normalised(std::string_view type);

 private:
  const TypeBinder& types_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_SIGNATUREBINDER_HPP
