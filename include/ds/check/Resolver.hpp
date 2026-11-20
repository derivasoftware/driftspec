#ifndef DRIFTSPEC_CHECK_RESOLVER_HPP
#define DRIFTSPEC_CHECK_RESOLVER_HPP

#include <string>
#include <vector>

#include "ds/model/Model.hpp"

namespace ds {
namespace check {

/// Read-only questions about a model: what a name means where it is written (R5, R10), what a
/// type inherits and holds, what is private on a way down (DS-125). The checker and the evaluator of
/// rules ask it; neither needs the other to know what a name is.
class Resolver {
 public:
  explicit Resolver(const model::Model& model) : model_(model) {}

  /// Resolve `name` as written in `scope`: innermost scope first, then outward, then global; a bare name
  /// found nowhere on the way is the one class of that name in the same package (DS-18).
  /// Returns the identity, or "" when nothing declares it, or when two classes of the package share the name.
  std::string resolve(const std::string& name, const std::string& scope) const;
  /// Whether `derived` is `base` or inherits from it, transitively.
  bool derives(const std::string& derived, const std::string& base) const;
  /// The member `name` of a type: its own, else the nearest one it inherits; nullptr when none declares it.
  const model::Entity* member(const std::string& type, const std::string& name) const;
  /// The element of a type written as the code writes it: the one entity of the design its text names
  /// (`list[Scope]`, `std::map<std::string, Scope*>` → the class Scope); "" for none or for two (DS-18).
  std::string resolveTypeText(const std::string& text, const std::string& scope) const;
  /// What is private on the way from `from` down to `target`; empty when everything crossed is public.
  std::string privateOnTheWay(const std::string& from, const std::string& target) const;

 private:
  /// The one class or enum named `name` (with its arity, Box<1>) in the package of `scope`; "" for none or two.
  std::string inPackage(const std::string& name, const std::string& scope) const;

  const model::Model& model_;
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_RESOLVER_HPP
