#ifndef DRIFTSPEC_BIND_TYPEBINDER_HPP
#define DRIFTSPEC_BIND_TYPEBINDER_HPP

#include <string>
#include <vector>

#include "ds/bind/BindContext.hpp"

namespace ds {
namespace bind {

/// A type as the code writes it (DS-18, DS-356): every name in it the design declares is a reference from the holder, with
/// the holder's relation; a template parameter is a name of the holder's, never a class.
class TypeBinder {
 public:
  /// Relates `from` to every name of `type` the design declares; the type's text, trimmed.
  std::string bind(TSNode type, const BindContext& ctx, const std::string& scope, const std::string& from, const std::string& relation,
                   const std::string& multiplicity, const std::vector<std::string>& params, const model::Location& location) const;
  /// The names of a template parameter list, <F, G: Bound>.
  static std::vector<std::string> params(const parse::TreeReader& reader, TSNode node);

 private:
  /// Whether a relation holds what it names: owns, refs or shares.
  static bool isOwnership(const std::string& relation);
  static bool contains(const std::vector<std::string>& items, const std::string& item);
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_TYPEBINDER_HPP
