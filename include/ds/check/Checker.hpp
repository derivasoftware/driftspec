#ifndef DRIFTSPEC_CHECK_CHECKER_HPP
#define DRIFTSPEC_CHECK_CHECKER_HPP

#include <map>
#include <set>
#include <string>

#include "ds/bind/Binder.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace check {

/// Resolves the references the Binder collected (R5, R6), checks the static
/// model against its own rules (R14–R17) and the dynamic specifications
/// against the static ones.
class Checker {
 public:
  explicit Checker(model::Model& model) : model_(model) {}
  void run(const bind::Binder& binder);

  /// Resolve `name` as written in `scope`: innermost scope first, then outward, then global.
  /// Returns the identity, or "" when nothing declares it.
  std::string resolve(const std::string& name, const std::string& scope) const;
  /// The extern entities of a simple name, declared in any extern module (the catalogue, R11).
  std::vector<std::string> externsNamed(const std::string& name) const;

  /// Whether `derived` is `base` or inherits from it, transitively.
  bool derives(const std::string& derived, const std::string& base) const;

 private:
  void resolveReference(const bind::Reference& reference);
  void checkMessage(const bind::Message& message);
  void checkEvent(const bind::Event& event);
  void checkLocals();
  void checkArtefacts();
  /// A behaviour's identity is its resolved subject plus its scenario, wherever it was written.
  void unifyBehaviours();
  std::string current(const std::string& id) const;  ///< an identity after the renames

  std::map<std::string, std::string> renamed_;
  /// The identity of a type written as text (`list<Scope>` → the class Scope), or "".
  std::string resolveTypeText(const std::string& text, const std::string& scope) const;
  /// The class a behaviour is about, and whether its subject is a method.
  const model::Entity* subjectClass(const model::Entity& sequence, const model::Entity** subject) const;
  void couple(const model::Entity& sequence, const model::Entity& type, bool transient);
  void checkTemplateBound(const bind::Reference& reference);
  void checkOwnershipCycles();
  void classify();

  model::Model& model_;
  std::set<std::string> reported_;
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_CHECKER_HPP
