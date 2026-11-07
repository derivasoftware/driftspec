#ifndef DRIFTSPEC_CHECK_CHECKER_HPP
#define DRIFTSPEC_CHECK_CHECKER_HPP

#include <map>
#include <set>
#include <string>
#include <utility>

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
  bool kindAllowed(const bind::Reference& reference, const std::string& target);
  void checkMessage(const bind::Message& message);
  void checkEvent(const bind::Event& event);
  void checkLocals();
  void checkArtefacts();
  void checkExtensions();
  void checkFields();
  void checkLanguages();     // what each package and module is written in, held down by containment (DS-86)
  void checkDependencies();  // what a box declares it uses against what its behaviours call (DS-96)
  void checkRules();         // a rule answers to a requirement and says what holds; a definition what it ranges over (DS-94)
  /// A behaviour's identity is its resolved subject plus its scenario, wherever it was written.
  void unifyBehaviours();
  std::string current(const std::string& id) const;  ///< an identity after the renames

  std::map<std::string, std::string> renamed_;
  /// The identity of a type written as text (`list<Scope>` → the class Scope), or "".
  std::string resolveTypeText(const std::string& text, const std::string& scope) const;
  /// The class a behaviour is about, and whether its subject is a method.
  const model::Entity* subjectClass(const model::Entity& sequence, const model::Entity** subject) const;
  void couple(const model::Entity& sequence, const model::Entity& type, bool transient);
  /// A message that reached its operation: the call, its arity, and its expansion (B5).
  void call(model::Entity& msg, const model::Entity& op, const bind::Message& message);
  /// The type a name has in a behaviour: attribute, parameter, local or an earlier result (B3). A value is
  /// a pair: the type as written and the scope that resolves it; an empty type is an unknown one.
  bool typeOfName(const model::Entity& sequence, const model::Entity* cls, const model::Entity& subject, const std::string& name,
                  std::pair<std::string, std::string>* value, bool* transient) const;
  void checkArguments(const model::Entity& msg, const model::Entity& op, const bind::Message& message);
  bool compatible(const std::pair<std::string, std::string>& argument, const std::pair<std::string, std::string>& parameter) const;
  void rememberResult(const bind::Message& message);
  void expand(model::Entity& msg, const model::Entity& op, const bind::Message& message);
  void checkReturn(const bind::Message& message);
  /// What is private on the way from `from` down to `target`; empty when everything crossed is public.
  std::string privateOnTheWay(const std::string& from, const std::string& target) const;
  void checkVisibility(const std::string& from, const std::string& target, const model::Location& location);
  void checkLeak(const std::string& from, const std::string& target, const model::Location& location);
  void checkInterfaces();
  void checkUseCases();  // a use case is a contract its operations realise, outcomes included (DS-77)
  void checkDiagrams();
  void checkViews(const model::Entity& state);
  /// The literal a behaviour ends in, following the scenarios its calls enter; empty when it cannot be told.
  std::string outcomeOf(const std::string& sequence, const std::string& path, std::set<std::string>* seen) const;
  void checkScenarios();
  void checkTemplateBound(const bind::Reference& reference);
  void checkOwnershipCycles();
  void classify();

  model::Model& model_;
  std::set<std::string> reported_;
  std::map<std::string, std::map<std::string, std::pair<std::string, std::string>>> results_;  ///< per behaviour, the results so far
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_CHECKER_HPP
