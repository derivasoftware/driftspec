#ifndef DRIFTSPEC_MODEL_MODEL_HPP
#define DRIFTSPEC_MODEL_MODEL_HPP

#include <map>
#include <set>
#include <string>
#include <vector>

#include "ds/model/Code.hpp"
#include "ds/model/Identity.hpp"
#include "ds/model/Index.hpp"
#include "ds/model/Severity.hpp"

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

class Metamodel;

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
      flags;  ///< modifiers with semantics (R19): abstract, static, const, owns, refs, shares, many, optional, dataclass, constructor
  std::vector<Param> params;      ///< read once from the syntax: an operation's parameters, a template's parameters
  std::vector<std::string> args;  ///< read once from the syntax: a message's arguments, as written
  Location location;
  bool expected = false;  ///< referenced somewhere but declared nowhere

  bool has(const std::string& flag) const;
  /// The value of a field; "" when it does not carry it.
  const std::string& field(const std::string& key) const;
  /// Whether a field is written as a whole number an int holds (`depth: 3`), read into out_number: the one
  /// reading of a number field. False, out_number untouched, when it is not written, holds anything but the
  /// digits 0 to 9 or is too large for an int; then it is no number, and never an abort (DS-337).
  bool number(const std::string& key, int& out_number) const;
};

/// A typed relation between two identities.
struct Relation {
  std::string kind;  ///< contains, refines, verifies, realises, inherits, uses, owns, refs, shares, instantiates, ...
  std::string from;
  std::string to;
  Location location;
  std::string multiplicity;  ///< for owns/refs/shares: "1", "0..1" or "*" (R16); empty otherwise
};

/// One binding that breaks a rule (DS-108): its ranges and where as bound, and for a never the answer
/// that breaks it; where the finding sits.
struct Breach {
  std::vector<std::pair<std::string, std::string>> bindings;
  std::vector<std::pair<std::string, std::string>> witness;
  Location location;
};

/// One diagnostic of the engine.
/// A finding: made only by Metamodel::finding from its code, which gives the printed code and the severity (DS-379).
struct Finding {
  std::string code;
  Severity severity;  ///< what the catalogue says of the code; the metamodel's severity table says what it means
  std::string message;
  Location location;
  std::string subject = {};  ///< the identity of the entity it is about (DS-426); "" for a file or a word, or not yet said
};

/// One term of a literal: a variable (a bare name), an entity (a qualified name) or a value (a number
/// or a string), as written.
struct Term {
  std::string text;
  bool variable = false;
  bool value = false;
  std::string field = {};     ///< the field read of what the variable is bound to (`r.status`); "" for the entity itself
  std::string property = {};  ///< what the entity bound is, read as a function: name, id or kind (DS-403); "" for none
};

/// One literal of a rule or a definition (DS-94): a relation of the metamodel, a kind, containment,
/// (in)equality or a definition applied to terms; negated when written after `not`.
/// How a literal reads: a relation, a kind, containment, a comparison, a definition applied or the
/// literal argument a call passes; a value that matches a pattern, and a member's flag (DS-403).
enum class LiteralForm { Relation, Kind, In, Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual, Call, Arg, Matches, Flag };

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

/// One column of a table (DS-268): a field of the row, what is related to the row, or a yes/no.
struct Column {
  std::string name;
  std::string form;      ///< "field", "property", "related" or "holds"
  std::string variable;  ///< the row's variable (field, property) or the one shown (related)
  std::string field;     ///< the field read
  std::string property;  ///< what is read of the entity, as a rule reads it: name, id or kind (DS-437)
  std::vector<Literal> body;
  Location location;
};

/// A table as the model answers it: the headings and, per row, a cell under each.
struct Tabulated {
  std::vector<std::string> columns;
  std::vector<std::vector<std::string>> rows;
  std::vector<std::vector<std::string>> kinds;  ///< how each cell reads: "" plain, "against" a no, "none" empty
};

/// What a rule or a definition says, read from its syntax and never from prose.
struct Formula {
  std::vector<Range> ranges;                 ///< a rule's for
  std::vector<Literal> where;                ///< a rule's where
  std::vector<std::vector<Literal>> claims;  ///< a rule's holds or never lines (one is expected)
  bool never = false;                        ///< the claim is a never
  std::vector<std::string> parameters;       ///< a definition's parameters
  std::vector<std::vector<Literal>> cases;   ///< a definition's if lines
  std::vector<Column> columns;               ///< a table's columns
};

/// The resolved workspace: entities by identity, relations and findings, written in the language of the metamodel
/// it is given, which outlives it (DS-341).
class Model {
 public:
  explicit Model(const Metamodel& metamodel) : metamodel_(&metamodel) {}
  /// An entity enters the model, or meets the one it keeps under that identity (R6, DS-222): what the model
  /// keeps comes back to be read. Only annotate and refine change it afterwards (DS-346).
  const Entity& declare(const Entity& entity);
  const Entity* find(const std::string& id) const;
  /// Write a field of an entity the model keeps: the binder's from the syntax, the checker's derived. Only the
  /// field changes; what the entity is indexed by (its identity, scope, kind, name and expected) stays (DS-346).
  void annotate(const std::string& id, const std::string& key, const std::string& value);
  /// What a later declaration says of an entity the model keeps: the body of a class refining an operator its
  /// header set declared. Its fields and flags join, its parameters replace any, and it is found where the later
  /// one is written; nothing it is indexed by changes (DS-346).
  void refine(const Entity& later);
  void relate(const Relation& relation);
  void report(const Finding& finding);
  /// Give an entity a new identity, carrying its members (scope) and relations along.
  /// Returns false, reporting E-DUPLICATE-IDENTITY, when the new identity is already declared.
  bool rename(const std::string& from, const std::string& to);

  const std::map<std::string, Entity>& entities() const { return entities_; }
  const std::vector<Finding>& findings() const { return findings_; }
  /// What is not yet written, kept apart from the findings: absence is no finding (DS-216); ds query coverage asks it.
  const std::vector<Finding>& coverage() const { return coverage_; }
  /// Every finding whose code the metamodel calls coverage leaves the findings for the coverage, marked so, each
  /// list in the order reported (DS-216).
  void cover();
  /// The findings that only echo what files that do not parse, or are not UTF-8, leave out of the model go: everything
  /// else said in those files, and every name declared nowhere. What says the files are broken stays (DS-304, DS-411).
  void withdrawEchoes(const std::set<std::string>& broken);
  /// The findings, and the coverage, in the order of where they are written: by file, then by line, and on one line
  /// by code, then by message (DS-382). Neither depends on the order the steps of the check ran in.
  void sortFindings();
  /// What the rules and definitions say, by the identity of each.
  const std::map<std::string, Formula>& formulas() const { return formulas_; }
  void state(const std::string& id, const Formula& formula) { formulas_[id] = formula; }
  /// The bindings that break each rule, by the rule's identity, as the evaluator found them.
  const std::map<std::string, std::vector<Breach>>& breaches() const { return breaches_; }
  void breach(const std::string& rule, const Breach& breach) { breaches_[rule].push_back(breach); }
  /// The tables the checker answered, by diagram (DS-268).
  const std::map<std::string, Tabulated>& tables() const { return tables_; }
  /// The table the checker answered for a diagram, in place of any it answered before.
  void tabulate(const std::string& diagram, const Tabulated& table) { tables_[diagram] = table; }
  /// The questions the model answers by identity, with no pass over it (DS-171).
  Index index() const { return Index(*this); }

 private:
  friend class Index;
  /// A behaviour is named after its subject: X::state, X::run::sequence (DS-222).
  static bool behaviour(Kind kind);
  /// The entity kept under an identity, to change; a defect of ds when none is kept.
  Entity& held(const std::string& id);
  /// E-DUPLICATE-IDENTITY: `entity` would take `id`, which `first` already holds.
  void reportDuplicate(const Entity& entity, const std::string& id, const Entity& first);
  /// The relations as the model keeps them: read through index(), the one way in for a reader.
  const std::vector<Relation>& relations() const { return relations_; }
  /// An entity enters what is indexed, or leaves it.
  void indexEntity(const Entity& entity, bool enters);
  /// Everything indexed, built again from the entities and the relations (after a rename).
  void indexAgain();
  /// Never null: a model is always written in one. Its own, for what it declares, relates, renames and covers; a
  /// reader of the model is handed the metamodel by whoever holds it (DS-351).
  const Metamodel* metamodel_;
  std::map<std::string, Entity> entities_;
  std::vector<Relation> relations_;
  std::vector<Finding> findings_;
  std::vector<Finding> coverage_;
  std::map<std::string, Formula> formulas_;
  std::map<std::string, std::vector<Breach>> breaches_;
  std::map<std::string, Tabulated> tables_;
  mutable Indexed indexed_;  ///< kept in step by declare, relate and rename; its tree is built when asked
};

}  // namespace model
}  // namespace ds

#endif  // DRIFTSPEC_MODEL_MODEL_HPP
