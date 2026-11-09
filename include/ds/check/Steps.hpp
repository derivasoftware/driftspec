#ifndef DRIFTSPEC_CHECK_STEPS_HPP
#define DRIFTSPEC_CHECK_STEPS_HPP

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Context.hpp"
#include "ds/check/Resolver.hpp"
#include "ds/check/Step.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace check {

/// References become relations, behaviours take their subject's identity.
class Linker : public IStep {
 public:
  std::string name() const override;
  std::vector<std::string> needs() const override;
  void run(Context& context) override;

 private:
  void resolveReference(const bind::Reference& reference);
  bool kindAllowed(const bind::Reference& reference, const std::string& target);
  void checkTemplateBound(const bind::Reference& reference);
  /// A behaviour's identity is its resolved subject plus its scenario, wherever it was written.
  void unifyBehaviours();
  Context* context_ = nullptr;
  model::Model* model_ = nullptr;
  const Resolver* resolver_ = nullptr;
};

/// Messages typed against the static context: calls, types, expansions.
class BehaviourTyping : public IStep {
 public:
  std::string name() const override;
  std::vector<std::string> needs() const override;
  void run(Context& context) override;

 private:
  /// The class a behaviour is about, and whether its subject is a method.
  const model::Entity* subjectClass(const model::Entity& sequence, const model::Entity** subject) const;
  void couple(const model::Entity& sequence, const model::Entity& type, bool transient);
  /// A message that reached its operation: the call, its arity, and its expansion (B5).
  void call(model::Entity& msg, const model::Entity& op, const bind::Message& message);
  void expand(model::Entity& msg, const model::Entity& op, const bind::Message& message);
  void checkReturn(const bind::Message& message);
  void checkMessage(const bind::Message& message);
  /// The type a name has in a behaviour: attribute, parameter, local or an earlier result (B3). A value is
  /// a pair: the type as written and the scope that resolves it; an empty type is an unknown one.
  bool typeOfName(const model::Entity& sequence, const model::Entity* cls, const model::Entity& subject, const std::string& name,
                  std::pair<std::string, std::string>* value, bool* transient) const;
  bool compatible(const std::pair<std::string, std::string>& argument, const std::pair<std::string, std::string>& parameter) const;
  void checkArguments(const model::Entity& msg, const model::Entity& op, const bind::Message& message);
  void rememberResult(const bind::Message& message);
  void checkLocals();
  void checkEvent(const bind::Event& event);
  Context* context_ = nullptr;
  model::Model* model_ = nullptr;
  const Resolver* resolver_ = nullptr;
  std::map<std::string, std::map<std::string, std::pair<std::string, std::string>>> results_;  ///< per behaviour, the results so far
};

/// Interfaces, use cases, scenarios, outcomes and diagrams.
class ContractCheck : public IStep {
 public:
  std::string name() const override;
  std::vector<std::string> needs() const override;
  void run(Context& context) override;

 private:
  /// The literal a behaviour ends in, following the scenarios its calls enter; empty when it cannot be told.
  std::string outcomeOf(const std::string& sequence, const std::string& path, std::set<std::string>* seen) const;
  void checkUseCases();  // a use case is a contract its operations realise, outcomes included (DS-77)
  void checkInterfaces();
  void checkViews(const model::Entity& state);
  void checkScenarios();
  void checkDiagrams();
  Context* context_ = nullptr;
  model::Model* model_ = nullptr;
  const Resolver* resolver_ = nullptr;
};

/// What the model says without writing it: the stereotypes.
class Derivation : public IStep {
 public:
  std::string name() const override;
  std::vector<std::string> needs() const override;
  void run(Context& context) override;

 private:
  void classify();
  Context* context_ = nullptr;
  model::Model* model_ = nullptr;
  const Resolver* resolver_ = nullptr;
};

/// Ownership, artefacts, extensions, fields and languages.
class StructureCheck : public IStep {
 public:
  std::string name() const override;
  std::vector<std::string> needs() const override;
  void run(Context& context) override;

 private:
  void checkOwnershipCycles();
  void checkArtefacts();
  void checkExtensions();
  void checkFields();
  void checkLanguages();  // what each package and module is written in, held down by containment (DS-86)
  Context* context_ = nullptr;
  model::Model* model_ = nullptr;
  const Resolver* resolver_ = nullptr;
};

/// What a box declares it uses against what its behaviours call.
class DependencyCheck : public IStep {
 public:
  std::string name() const override;
  std::vector<std::string> needs() const override;
  void run(Context& context) override;

 private:
  void checkDependencies();  // what a box declares it uses against what its behaviours call (DS-96)
  Context* context_ = nullptr;
  model::Model* model_ = nullptr;
  const Resolver* resolver_ = nullptr;
};

/// Rules and definitions, decided by the evaluator.
class RuleCheck : public IStep {
 public:
  std::string name() const override;
  std::vector<std::string> needs() const override;
  void run(Context& context) override;

 private:
  void checkRules();  // a rule answers to a requirement and says what holds; a definition what it ranges over (DS-94)
  Context* context_ = nullptr;
  model::Model* model_ = nullptr;
  const Resolver* resolver_ = nullptr;
};

/// Every step of the check, made in one place, by key (DS-138).
std::map<std::string, std::unique_ptr<IStep>> steps();

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_STEPS_HPP
