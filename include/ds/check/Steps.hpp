#ifndef DRIFTSPEC_CHECK_STEPS_HPP
#define DRIFTSPEC_CHECK_STEPS_HPP

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ds/bind/Notes.hpp"
#include "ds/check/Below.hpp"
#include "ds/check/Context.hpp"
#include "ds/check/Crossing.hpp"
#include "ds/check/Resolver.hpp"
#include "ds/check/Selection.hpp"
#include "ds/check/Step.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace check {

/// A step that only checks (DS-138): its run hands check the context to read, so the model it is given is
/// const and no check can change it; it gives back what it found. The one abstract class here, justified
/// by that guarantee (GUIDE-cpp-design).
class ACheck : public IStep {
 public:
  std::vector<model::Finding> run() final;

 private:
  virtual std::vector<model::Finding> check() = 0;
};

/// References become relations, behaviours take their subject's identity.
class Linker : public IStep {
 public:
  /// Given what it reads once, when it is made (DS-425).
  explicit Linker(Context& context);
  std::string name() const override;
  std::vector<std::string> needs() const override;
  std::vector<model::Finding> run() override;

 private:
  void resolveReference(const bind::Reference& reference);
  /// A bare name two classes of its package share: said once for its place, related to nothing (DS-325).
  bool reportAmbiguous(const bind::Reference& reference);
  bool kindAllowed(const bind::Reference& reference, const std::string& target);
  void checkTemplateBound(const bind::Reference& reference);
  /// A behaviour's identity is its resolved subject plus its scenario, wherever it was written.
  void unifyBehaviours();
  /// A class written with annotations only that nothing else defines is the definition, not an extension.
  void defineLoneExtensions();
  Context* context_ = nullptr;
  model::Model* model_ = nullptr;
  const model::Metamodel* metamodel_ = nullptr;
  const Resolver* resolver_ = nullptr;
};

/// Messages typed against the static context: calls, types, expansions.
class BehaviourTyping : public IStep {
 public:
  /// Given what it reads once, when it is made (DS-425).
  explicit BehaviourTyping(Context& context);
  std::string name() const override;
  std::vector<std::string> needs() const override;
  std::vector<model::Finding> run() override;

 private:
  /// The class a behaviour is about, and whether its subject is a method.
  const model::Entity* subjectClass(const model::Entity& sequence, const model::Entity** subject) const;
  void couple(const model::Entity& sequence, const model::Entity& type, bool transient);
  /// A message that reached its operation: the call, its arity, and its expansion (B5).
  void call(const model::Entity& msg, const model::Entity& op, const bind::Message& message);
  /// A call two overloads of its name could be a call of, by its arguments (DS-455): W-NAME-AMBIGUOUS, and true.
  bool reportOverloads(const model::Entity& seq, const std::string& typeId, const bind::Message& message);
  void expand(const model::Entity& msg, const model::Entity& op, const bind::Message& message);
  void checkReturn(const bind::Message& message);
  void checkMessage(const bind::Message& message);
  /// A message with no participant that names a class's operation by its qualified name: a call on the
  /// class itself, which the operation must be static to take (DS-60). False when it names none.
  bool callStatic(const model::Entity& seq, const model::Entity& msg, const bind::Message& message, const std::string& home);
  /// The box, interface or class a participant is named as (DS-232); empty when the name is none of them.
  std::string namedEntity(const std::string& name, const std::string& home) const;
  /// Types who sends a message, the participant written before its arrow; a name nobody designs is text.
  void typeSender(const model::Entity& seq, const model::Entity* cls, const model::Entity& subject, const model::Entity& msg,
                  const std::string& home);
  /// The operation a box offers under a name: a function of a module, an operation of one interface of a package.
  const model::Entity* offered(const model::Entity& box, const std::string& op) const;
  /// The type a name has in a behaviour: attribute, parameter, local or an earlier result (B3). A value is
  /// a pair: the type as written and the scope that resolves it; an empty type is an unknown one.
  bool typeOfName(const model::Entity& sequence, const model::Entity* cls, const model::Entity& subject, const std::string& name,
                  std::pair<std::string, std::string>* value, bool* transient) const;
  /// Whether a value may be passed where `parameter` is expected; `literal` for a literal, whose type ds does not know.
  bool compatible(const std::pair<std::string, std::string>& argument, const std::pair<std::string, std::string>& parameter,
                  bool literal = false) const;
  void checkArguments(const model::Entity& msg, const model::Entity& op, const bind::Message& message);
  void rememberResult(const bind::Message& message);
  void checkLocals();
  void checkEvent(const bind::Event& event);
  Context* context_ = nullptr;
  model::Model* model_ = nullptr;
  const Resolver* resolver_ = nullptr;
  std::map<std::string, std::map<std::string, std::pair<std::string, std::string>>> results_;  ///< per behaviour, the results so far
};

/// An interface's operations against their behaviours and outcomes, its machine as navigation, a class machine's
/// reachable states, and a method against the operation it answers (DS-382: the interfaces of ContractCheck).
class InterfaceContracts : public ACheck {
 public:
  /// Given what it reads once, when it is made (DS-425).
  explicit InterfaceContracts(const Context& context);
  std::string name() const override;
  std::vector<std::string> needs() const override;

 private:
  std::vector<model::Finding> check() override;
  /// The literal a behaviour ends in, following the scenarios its calls enter; empty when it cannot be told.
  std::string outcomeOf(const std::string& sequence, const std::string& path, std::set<std::string>* seen) const;
  void checkInterfaces();
  /// A class's method against the operation of the interface it realises, when the class declares it.
  void checkRealisations();
  void checkViews(const model::Entity& state);
  /// A state of a class's machine that no transition leads to from the initial one.
  void checkStates(const model::Entity& state);
  const Context* context_ = nullptr;
  const model::Model* model_ = nullptr;
  const model::Metamodel* metamodel_ = nullptr;
  std::vector<model::Finding> found_;
  const Resolver* resolver_ = nullptr;
};

/// A use case as a contract its operations realise, outcome by outcome (DS-77, DS-382).
class UseCaseContracts : public ACheck {
 public:
  /// Given what it reads once, when it is made (DS-425).
  explicit UseCaseContracts(const Context& context);
  std::string name() const override;
  std::vector<std::string> needs() const override;

 private:
  std::vector<model::Finding> check() override;
  void checkUseCases();  // a use case is a contract its operations realise, outcomes included (DS-77)
  const Context* context_ = nullptr;
  const model::Model* model_ = nullptr;
  const model::Metamodel* metamodel_ = nullptr;
  std::vector<model::Finding> found_;
  const Resolver* resolver_ = nullptr;
};

/// What a diagram names exists, and how deep it asks to be drawn (DS-382).
class DiagramCheck : public ACheck {
 public:
  /// Given what it reads once, when it is made (DS-425).
  explicit DiagramCheck(const Context& context);
  std::string name() const override;
  std::vector<std::string> needs() const override;

 private:
  std::vector<model::Finding> check() override;
  /// Whether a pattern a diagram writes names something the model declares: itself, a prefix ending in *, or a
  /// template of it; a scenario by its behaviour's subject.
  bool namesSomething(const std::string& written) const;
  void checkDiagrams();
  const Context* context_ = nullptr;
  const model::Model* model_ = nullptr;
  const model::Metamodel* metamodel_ = nullptr;
  std::vector<model::Finding> found_;
  const Resolver* resolver_ = nullptr;
};

/// A scenario written twice for one behaviour (REQ-00065-1, DS-382).
class ScenarioCheck : public ACheck {
 public:
  /// Given what it reads once, when it is made (DS-425).
  explicit ScenarioCheck(const Context& context);
  std::string name() const override;
  std::vector<std::string> needs() const override;

 private:
  std::vector<model::Finding> check() override;
  void checkScenarios();
  const Context* context_ = nullptr;
  const model::Model* model_ = nullptr;
  const model::Metamodel* metamodel_ = nullptr;
  std::vector<model::Finding> found_;
  const Resolver* resolver_ = nullptr;
};

/// What each operation lets escape against what its signature promises, and handlers that handle nothing; it reads
/// what FailureFlow wrote and writes nothing (DS-189, DS-224, DS-382).
class FailureCheck : public ACheck {
 public:
  /// Given what it reads once, when it is made (DS-425).
  explicit FailureCheck(const Context& context);
  std::string name() const override;
  std::vector<std::string> needs() const override;

 private:
  std::vector<model::Finding> check() override;
  /// A handler of a failure no step of its attempt can fail with.
  void checkHandlers();
  /// What an operation lets escape against what its signature promises, and at a boundary (DS-189).
  void checkFailures(const model::Entity& operation);
  void checkScenario(const model::Entity& usecase);
  const Context* context_ = nullptr;
  const model::Model* model_ = nullptr;
  const model::Metamodel* metamodel_ = nullptr;
  std::vector<model::Finding> found_;
  const Resolver* resolver_ = nullptr;
};

/// What the model says without writing it: the stereotypes and the languages.
class Derivation : public IStep {
 public:
  /// Given what it reads once, when it is made (DS-425).
  explicit Derivation(Context& context);
  std::string name() const override;
  std::vector<std::string> needs() const override;
  std::vector<model::Finding> run() override;

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
  /// Given what it reads once, when it is made (DS-425).
  explicit FailureFlow(Context& context);
  std::string name() const override;
  std::vector<std::string> needs() const override;
  std::vector<model::Finding> run() override;

 private:
  /// The failures the attempts around a step handle: a step in an attempt's steps, not in its handlers.
  std::set<std::string> handledAround(const std::string& step) const;
  /// Where a failure starts: the scenario of the step that fails, X::sequence/s, or the behaviour itself.
  std::string originOf(const std::string& step, const std::string& sequence) const;
  /// Whether a behaviour is written for an operation; one with none fails as its signature says.
  bool hasBehaviour(const std::string& operation) const;
  /// The operation a message calls; "" when it calls none ds knows.
  std::string callee(const std::string& message) const;
  /// One pass over every operation; whether anything new escaped.
  bool pass(std::map<std::string, std::map<std::string, Escape>>& out_escapes) const;
  void write(const std::map<std::string, std::map<std::string, Escape>>& escapes);
  model::Model* model_ = nullptr;
  Context* context_ = nullptr;  ///< what it reads, given at construction (DS-425)
};

/// Two levels of one behaviour, both written, agree (DS-233). A scenario of a use case says what crosses between
/// its participants; the behaviour of the operation it is asked through, unfolded and lifted onto those
/// participants, has to cross the same way in the same order. The higher level may leave crossings out; it may
/// not add one or turn two around. What the level below does not say yet is coverage, never a finding.
class LevelCoherence : public ACheck {
 public:
  /// Given what it reads once, when it is made (DS-425).
  explicit LevelCoherence(const Context& context);
  std::string name() const override;
  std::vector<std::string> needs() const override;

 private:
  std::vector<model::Finding> check() override;
  /// The operation a message calls; "" when it calls none.
  static std::string calleeOf(const model::Model& model, const std::string& message);
  /// The behaviour a scenario's entry runs below it, and the scenario of it: the scenario's own, else the shared one
  /// that has it, else what the entry expands.
  static std::pair<std::string, std::string> behaviourBelow(const model::Model& model, const model::Entity& scenario,
                                                            const model::Entity& entry);
  /// Every crossing of the behaviour, entering the behaviours its calls expand, each participant named by `on`.
  template <typename On>
  static void collect(const model::Model& model, const std::string& sequence, const std::string& path, const On& on,
                      std::set<std::string>& stack, Below& out);
  /// Whether a crossing said is the one done: the same two participants, and the same operation when it names one.
  static bool same(const Crossing& said, const Crossing& done);
  /// Where each crossing said finds its place, in order, among those done: the longest such choice; -1 for one that
  /// finds none.
  static std::vector<int> placed(const std::vector<Crossing>& said, const std::vector<Crossing>& done);
  const Context* context_ = nullptr;  ///< what it reads, given at construction (DS-425)
};

/// The states of a machine, written as rules over the context of its subject (DS-249), held to each other
/// and to the structure: two that can hold at once, a value a type does not have, an attribute the subject
/// does not hold, a rule that excludes itself.
class StateRules : public ACheck {
 public:
  /// Given what it reads once, when it is made (DS-425).
  explicit StateRules(const Context& context);
  std::string name() const override;
  std::vector<std::string> needs() const override;

 private:
  std::vector<model::Finding> check() override;
  const Context* context_ = nullptr;  ///< what it reads, given at construction (DS-425)
};

/// The calls of the behaviours written, held to the machine of what they are made on (DS-250): a call of an
/// event made when its receiver can be in no state the machine lets it happen from. The behaviours are
/// read as they are: nothing is asked of them.
class CallOrder : public ACheck {
 public:
  /// Given what it reads once, when it is made (DS-425).
  explicit CallOrder(const Context& context);
  std::string name() const override;
  std::vector<std::string> needs() const override;

 private:
  std::vector<model::Finding> check() override;
  /// The states a call could have come from, as the message says them: Open or Paid.
  static std::string either(const std::vector<std::string>& names);
  const Context* context_ = nullptr;  ///< what it reads, given at construction (DS-425)
};

/// Ownership, artefacts, extensions and fields.
class StructureCheck : public ACheck {
 public:
  /// Given what it reads once, when it is made (DS-425).
  explicit StructureCheck(const Context& context);
  std::string name() const override;
  std::vector<std::string> needs() const override;

 private:
  std::vector<model::Finding> check() override;
  void checkOwnershipCycles();
  /// A chain of one relation that comes back to where it starts (refines, inherits).
  void checkCycles(const std::string& relation, model::Code code, const std::string& verb);
  /// A requirement that refines one written inside its own box.
  void checkRefinesUpward();
  void checkSatisfiesHome();
  void checkArtefacts();
  /// Whether a requirement is realised (DS-429): something satisfies it, or it is refined and every requirement that
  /// refines it is realised; one met again on the way down (a cycle, a finding of its own) is not. `seen` holds that way.
  bool realised(const std::string& requirement, const std::set<std::string>& satisfied,
                const std::map<std::string, std::vector<std::string>>& refinedBy, std::set<std::string>& seen) const;
  void checkExtensions();
  void checkFields();
  const Context* context_ = nullptr;
  const model::Model* model_ = nullptr;
  const model::Metamodel* metamodel_ = nullptr;
  std::vector<model::Finding> found_;
  const Resolver* resolver_ = nullptr;
};

/// What a box declares it uses against what its behaviours call.
class DependencyCheck : public ACheck {
 public:
  /// Given what it reads once, when it is made (DS-425).
  explicit DependencyCheck(const Context& context);
  std::string name() const override;
  std::vector<std::string> needs() const override;

 private:
  std::vector<model::Finding> check() override;
  void checkDependencies();  // what a box declares it uses against what its behaviours call (DS-96)
  const Context* context_ = nullptr;
  const model::Model* model_ = nullptr;
  std::vector<model::Finding> found_;
  const Resolver* resolver_ = nullptr;
};

/// Rules and definitions, decided by the evaluator.
class RuleCheck : public IStep {
 public:
  /// Given what it reads once, when it is made (DS-425).
  explicit RuleCheck(Context& context);
  std::string name() const override;
  std::vector<std::string> needs() const override;
  std::vector<model::Finding> run() override;

 private:
  void checkRules();  // a rule answers to a requirement and says what holds; a definition what it ranges over (DS-94)
  Context* context_ = nullptr;
  model::Model* model_ = nullptr;
  const model::Metamodel* metamodel_ = nullptr;
  const Resolver* resolver_ = nullptr;
};

/// The one place that names the concrete steps (DS-138, DS-382): it builds what a Selection asks for (DS-376), the
/// steps it names and every step they need, keyed by name. The checker runs them through IStep, knowing none of them.
class StepFactory {
 public:
  /// The steps a selection names and every step they need, keyed by name; every step, unless a selection says
  /// otherwise. Each is made with the context it reads, once (DS-425).
  static std::map<std::string, std::unique_ptr<IStep>> of(Context& context, const Selection& selection = Selection::every());
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_STEPS_HPP
