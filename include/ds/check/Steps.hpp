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

/// A step that only checks (DS-138): its run hands check the context to read, so the model it is given is
/// const and no check can change it; it gives back what it found. The one abstract class here, justified
/// by that guarantee (GUIDE-cpp-design).
class ACheck : public IStep {
 public:
  std::vector<model::Finding> run(Context& context) final;

 private:
  virtual std::vector<model::Finding> check(const Context& context) = 0;
};

/// References become relations, behaviours take their subject's identity.
class Linker : public IStep {
 public:
  std::string name() const override;
  std::vector<std::string> needs() const override;
  std::vector<model::Finding> run(Context& context) override;

 private:
  void resolveReference(const bind::Reference& reference);
  bool kindAllowed(const bind::Reference& reference, const std::string& target);
  void checkTemplateBound(const bind::Reference& reference);
  /// A behaviour's identity is its resolved subject plus its scenario, wherever it was written.
  void unifyBehaviours();
  /// A class written with annotations only that nothing else defines is the definition, not an extension.
  void defineLoneExtensions();
  Context* context_ = nullptr;
  model::Model* model_ = nullptr;
  const Resolver* resolver_ = nullptr;
};

/// Messages typed against the static context: calls, types, expansions.
class BehaviourTyping : public IStep {
 public:
  std::string name() const override;
  std::vector<std::string> needs() const override;
  std::vector<model::Finding> run(Context& context) override;

 private:
  /// The class a behaviour is about, and whether its subject is a method.
  const model::Entity* subjectClass(const model::Entity& sequence, const model::Entity** subject) const;
  void couple(const model::Entity& sequence, const model::Entity& type, bool transient);
  /// A message that reached its operation: the call, its arity, and its expansion (B5).
  void call(model::Entity& msg, const model::Entity& op, const bind::Message& message);
  void expand(model::Entity& msg, const model::Entity& op, const bind::Message& message);
  void checkReturn(const bind::Message& message);
  void checkMessage(const bind::Message& message);
  /// A message with no participant that names a class's operation by its qualified name: a call on the
  /// class itself, which the operation must be static to take (DS-60). False when it names none.
  bool callStatic(const model::Entity& seq, model::Entity& msg, const bind::Message& message, const std::string& home);
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
class ContractCheck : public ACheck {
 public:
  std::string name() const override;
  std::vector<std::string> needs() const override;

 private:
  std::vector<model::Finding> check(const Context& context) override;
  /// The literal a behaviour ends in, following the scenarios its calls enter; empty when it cannot be told.
  std::string outcomeOf(const std::string& sequence, const std::string& path, std::set<std::string>* seen) const;
  void checkUseCases();  // a use case is a contract its operations realise, outcomes included (DS-77)
  void checkInterfaces();
  void checkViews(const model::Entity& state);
  /// A state of a class's machine that no transition leads to from the initial one.
  void checkStates(const model::Entity& state);
  void checkScenarios();
  void checkDiagrams();
  const Context* context_ = nullptr;
  const model::Model* model_ = nullptr;
  std::vector<model::Finding> found_;
  const Resolver* resolver_ = nullptr;
};

/// What the model says without writing it: the stereotypes and the languages.
class Derivation : public IStep {
 public:
  std::string name() const override;
  std::vector<std::string> needs() const override;
  std::vector<model::Finding> run(Context& context) override;

 private:
  void fillLanguages();  // what each package and module is written in, held down by containment (DS-86)
  void classify();
  Context* context_ = nullptr;
  model::Model* model_ = nullptr;
  const Resolver* resolver_ = nullptr;
};

/// One failure escaping an operation: the operations it comes up through, from that one down to where it
/// starts, and the behaviour (or the scenario of one) that starts it.
struct Escape {
  std::vector<std::string> through;
  std::string origin;
};

/// What failures escape each operation, and the path each comes by (DS-188): computed from where a
/// failure starts (fail) and where it is handled (attempt ... on), up through the calls, until nothing
/// changes; written on the operation as derived fields, escapes and escape.<failure>.
class FailureFlow : public IStep {
 public:
  std::string name() const override;
  std::vector<std::string> needs() const override;
  std::vector<model::Finding> run(Context& context) override;

 private:
  /// The failures the attempts around a step handle: a step in an attempt's steps, not in its handlers.
  std::set<std::string> handledAround(const std::string& step) const;
  /// Where a failure starts: the scenario of the step that fails, X::sequence/s, or the behaviour itself.
  std::string originOf(const std::string& step, const std::string& sequence) const;
  /// The operation a message calls; "" when it calls none ds knows.
  std::string callee(const std::string& message) const;
  /// One pass over every operation; whether anything new escaped.
  bool pass(std::map<std::string, std::map<std::string, Escape>>& out_escapes) const;
  void write(const std::map<std::string, std::map<std::string, Escape>>& escapes);
  model::Model* model_ = nullptr;
};

/// Ownership, artefacts, extensions and fields.
class StructureCheck : public ACheck {
 public:
  std::string name() const override;
  std::vector<std::string> needs() const override;

 private:
  std::vector<model::Finding> check(const Context& context) override;
  void checkOwnershipCycles();
  void checkArtefacts();
  void checkExtensions();
  void checkFields();
  const Context* context_ = nullptr;
  const model::Model* model_ = nullptr;
  std::vector<model::Finding> found_;
  const Resolver* resolver_ = nullptr;
};

/// What a box declares it uses against what its behaviours call.
class DependencyCheck : public ACheck {
 public:
  std::string name() const override;
  std::vector<std::string> needs() const override;

 private:
  std::vector<model::Finding> check(const Context& context) override;
  void checkDependencies();  // what a box declares it uses against what its behaviours call (DS-96)
  const Context* context_ = nullptr;
  const model::Model* model_ = nullptr;
  std::vector<model::Finding> found_;
  const Resolver* resolver_ = nullptr;
};

/// Rules and definitions, decided by the evaluator.
class RuleCheck : public IStep {
 public:
  std::string name() const override;
  std::vector<std::string> needs() const override;
  std::vector<model::Finding> run(Context& context) override;

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
