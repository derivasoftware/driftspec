#ifndef DRIFTSPEC_CHECK_RESOLVER_HPP
#define DRIFTSPEC_CHECK_RESOLVER_HPP

#include <string>
#include <vector>

#include "ds/model/Model.hpp"

namespace ds {
namespace check {

/// Read-only questions about a model: what a name means where it is written (R5, R10, R11), what a
/// type inherits and holds, what is private on a way down (DS-125). The checker and the evaluator of
/// rules ask it; neither needs the other to know what a name is.
class Resolver {
 public:
  explicit Resolver(const model::Model& model) : model_(model) {}

  /// Resolve `name` as written in `scope`: innermost scope first, then outward, then global.
  /// Returns the identity, or "" when nothing declares it.
  std::string resolve(const std::string& name, const std::string& scope) const;
  /// The extern entities of a simple name, declared in any extern module (the catalogue, R11).
  std::vector<std::string> externsNamed(const std::string& name) const;
  /// Whether `derived` is `base` or inherits from it, transitively.
  bool derives(const std::string& derived, const std::string& base) const;
  /// The member `name` of a type: its own, else the nearest one it inherits; nullptr when none declares it.
  const model::Entity* member(const std::string& type, const std::string& name) const;
  /// The identity of a type written as text (`list<Scope>` → the class Scope), or "".
  std::string resolveTypeText(const std::string& text, const std::string& scope) const;
  /// What is private on the way from `from` down to `target`; empty when everything crossed is public.
  std::string privateOnTheWay(const std::string& from, const std::string& target) const;

 private:
  const model::Model& model_;
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_RESOLVER_HPP
