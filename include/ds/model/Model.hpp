#ifndef DRIFTSPEC_MODEL_MODEL_HPP
#define DRIFTSPEC_MODEL_MODEL_HPP

#include <map>
#include <set>
#include <string>
#include <vector>

#include "ds/model/Identity.hpp"
#include "ds/model/Index.hpp"

namespace ds {
namespace model {

/// Where something was read from: the file, and where it starts and ends, 1-based (DS-172). The end
/// is the place right after its last character; 0 when it is not known (a node nothing wrote). Columns
/// count bytes, as the parser does.
struct Location {
  std::string file;
  int line = 0;
  int column = 0;
  int endLine = 0;
  int endColumn = 0;
};

/// The kinds of entity the language declares.
enum class Kind {
  Package,
  Module,
  Requirement,
  Verification,
  Interface,
  Operation,
  View,
  Constraint,
  Class,
  Enum,
  Function,
  Method,
  Attribute,
  Operator,
  Sequence,
  Message,
  Frame,
  State,
  Diagram,
  UseCase,
  Outcome,
  Rule,
  Definition
};

const char* kindName(Kind kind);
/// Whether a kind is an artefact identified by its id with its revision: requirement, verification,
/// constraint and use case (R5, DS-116).
bool isArtefact(Kind kind);

/// One declared (or expected, R6) entity of the workspace.
/// One parameter as the binder read it: its name and its type as written (a template's: its name and bound).
struct Param {
  std::string name;
  std::string type;
};

struct Entity {
  Kind kind = Kind::Package;
  std::string id;     ///< identity: qualified name, or the artefact id
  std::string name;   ///< the simple name as written
  std::string scope;  ///< qualified name of the enclosing scope ("" at top)
  std::map<std::string, std::string> fields;
  std::vector<std::string>
      flags;  ///< modifiers with semantics (R19): abstract, static, const, owns, refs, shares, extern, dataclass, constructor
  std::vector<Param> params;      ///< read once from the syntax: an operation's parameters, a template's parameters
  std::vector<std::string> args;  ///< read once from the syntax: a message's arguments, as written
  Location location;
  bool expected = false;  ///< referenced somewhere but declared nowhere

  bool has(const std::string& flag) const;
  /// The value of a field; "" when it does not carry it.
  std::string field(const std::string& key) const;
};

/// A typed relation between two identities.
struct Relation {
  std::string kind;  ///< contains, refines, verifies, realises, inherits, uses, owns, refs, shares, instantiates, ...
  std::string from;
  std::string to;
  Location location;
  std::string multiplicity;  ///< for owns/refs/shares: "1", "0..1" or "*" (R16); empty otherwise
  std::string change = {};   ///< set by a diff on the model it marks: "added" or "removed"; empty otherwise
};

/// One binding that breaks a rule (DS-108): its ranges and where as bound, and for a never the answer
/// that breaks it; where the finding sits.
struct Breach {
  std::vector<std::pair<std::string, std::string>> bindings;
  std::vector<std::pair<std::string, std::string>> witness;
  Location location;
};

/// One diagnostic of the engine.
struct Finding {
  std::string code;
  std::string severity;  ///< "error" or "warning"
  std::string message;
  Location location;
};

/// One term of a literal: a variable (a bare name), an entity (a qualified name) or a value (a number
/// or a string), as written.
struct Term {
  std::string text;
  bool variable = false;
  bool value = false;
};

/// One literal of a rule or a definition (DS-94): a relation of the metamodel, a kind, containment,
/// (in)equality or a definition applied to terms; negated when written after `not`.
/// How a literal reads: a relation, a kind, containment, a comparison, a definition applied or the
/// literal argument a call passes.
enum class LiteralForm { Relation, Kind, In, Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual, Call, Arg };

struct Literal {
  LiteralForm form = LiteralForm::Relation;
  bool negated = false;
  std::string name;  ///< the relation, the kind, the definition or the parameter (arg), as written
  std::vector<Term> terms;
  Location location;
};

/// What a range of `for` binds: every entity of a kind, optionally inside a box.
struct Range {
  std::string kind;
  std::string variable;
  std::string scope;  ///< as written; "" for the whole model
  Location location;
};

/// What a rule or a definition says, read from its syntax and never from prose.
struct Formula {
  std::vector<Range> ranges;                 ///< a rule's for
  std::vector<Literal> where;                ///< a rule's where
  std::vector<std::vector<Literal>> claims;  ///< a rule's holds or never lines (one is expected)
  bool never = false;                        ///< the claim is a never
  std::vector<std::string> parameters;       ///< a definition's parameters
  std::vector<std::vector<Literal>> cases;   ///< a definition's if lines
};

/// The resolved workspace: entities by identity, relations and findings.
class Model {
 public:
  /// The relation as the language reads it: a class that inherits from an interface realises it (R14),
  /// the checker having derived the interface; any other relation reads as written. Every drawing asks
  /// here, so the viewer and the diagrams say the same (DS-118).
  std::string readAs(const Relation& relation) const;
  Entity& declare(const Entity& entity);
  const Entity* find(const std::string& id) const;
  Entity* find(const std::string& id);
  void relate(const Relation& relation);
  void report(const Finding& finding);
  /// Give an entity a new identity, carrying its members (scope) and relations along.
  /// Returns false, reporting E-DUPLICATE-IDENTITY, when the new identity is already declared.
  bool rename(const std::string& from, const std::string& to);

  const std::map<std::string, Entity>& entities() const { return entities_; }
  const std::vector<Finding>& findings() const { return findings_; }
  std::vector<Finding>& findings() { return findings_; }
  /// What is not yet written, kept apart from the findings: absence is no finding (DS-216); ds query coverage asks it.
  const std::vector<Finding>& coverage() const { return coverage_; }
  std::vector<Finding>& coverage() { return coverage_; }
  /// What the rules and definitions say, by the identity of each.
  const std::map<std::string, Formula>& formulas() const { return formulas_; }
  void state(const std::string& id, const Formula& formula) { formulas_[id] = formula; }
  /// The bindings that break each rule, by the rule's identity, as the evaluator found them.
  const std::map<std::string, std::vector<Breach>>& breaches() const { return breaches_; }
  void breach(const std::string& rule, const Breach& breach) { breaches_[rule].push_back(breach); }
  /// The questions the model answers by identity, with no pass over it (DS-171).
  Index index() const { return Index(*this); }

 private:
  friend class Index;
  /// The relations as the model keeps them: read through index(), the one way in for a reader.
  const std::vector<Relation>& relations() const { return relations_; }
  /// An entity enters what is indexed, or leaves it.
  void indexEntity(const Entity& entity, bool enters);
  /// Everything indexed, built again from the entities and the relations (after a rename).
  void indexAgain();
  std::map<std::string, Entity> entities_;
  std::vector<Relation> relations_;
  std::vector<Finding> findings_;
  std::vector<Finding> coverage_;
  std::map<std::string, Formula> formulas_;
  std::map<std::string, std::vector<Breach>> breaches_;
  mutable Indexed indexed_;  ///< kept in step by declare, relate and rename; its tree is built when asked
};

/// Whether a branch of the behaviour `sequence` is named `scenario` (`opt ... / scenario {`).
bool hasScenario(const Model& model, const std::string& sequence, const std::string& scenario);

/// The messages of `sequence` that the path named `scenario` takes, in order, up to the return that ends it.
/// A named branch is taken when it is asked for and skipped otherwise; of an alt with one named side, the
/// other side is taken; a branch it cannot tell is kept, and its frame is added to `kept`. An empty
/// `scenario` is the default path: no named branch taken.
std::vector<const Entity*> pathOf(const Model& model, const std::string& sequence, const std::string& scenario,
                                  std::set<std::string>* kept);

}  // namespace model
}  // namespace ds

#endif  // DRIFTSPEC_MODEL_MODEL_HPP
