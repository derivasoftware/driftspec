#ifndef DRIFTSPEC_MODEL_MODEL_HPP
#define DRIFTSPEC_MODEL_MODEL_HPP

#include <map>
#include <string>
#include <vector>

namespace ds {
namespace model {

/// Where something was read from: file path and 1-based line and column.
struct Location {
  std::string file;
  int line = 0;
  int column = 0;
};

/// The kinds of entity the language declares.
enum class Kind {
  Package,
  Module,
  Requirement,
  Verification,
  Boundary,
  Operation,
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
  Diagram
};

const char* kindName(Kind kind);

/// Whether `scope` is the inside of an outermost package (`app@`): what is declared there is of the system.
bool isSystemScope(const std::string& scope);

/// The simple name at the end of an identity: app@core@model::Entity → Entity, app@core → core.
std::string lastName(const std::string& id);

/// One declared (or expected, R6) entity of the workspace.
struct Entity {
  Kind kind = Kind::Package;
  std::string id;     ///< identity: qualified name, or the artefact id
  std::string name;   ///< the simple name as written
  std::string scope;  ///< qualified name of the enclosing scope ("" at top)
  std::map<std::string, std::string> fields;
  std::vector<std::string>
      flags;  ///< modifiers with semantics (R19): abstract, static, const, owns, refs, shares, extern, dataclass, constructor
  Location location;
  bool expected = false;  ///< referenced somewhere but declared nowhere

  bool has(const std::string& flag) const;
};

/// A typed relation between two identities.
struct Relation {
  std::string kind;  ///< contains, refines, verifies, realises, inherits, uses, owns, refs, shares, instantiates, ...
  std::string from;
  std::string to;
  Location location;
  std::string multiplicity;  ///< for owns/refs/shares: "1", "0..1" or "*" (R16); empty otherwise
};

/// One diagnostic of the engine.
struct Finding {
  std::string code;
  std::string severity;  ///< "error" or "warning"
  std::string message;
  Location location;
};

/// The resolved workspace: entities by identity, relations and findings.
class Model {
 public:
  Entity& declare(const Entity& entity);
  const Entity* find(const std::string& id) const;
  Entity* find(const std::string& id);
  void relate(const Relation& relation);
  void report(const Finding& finding);
  /// Give an entity a new identity, carrying its members (scope) and relations along.
  /// Returns false, reporting E-DUPLICATE-IDENTITY, when the new identity is already declared.
  bool rename(const std::string& from, const std::string& to);

  const std::map<std::string, Entity>& entities() const { return entities_; }
  const std::vector<Relation>& relations() const { return relations_; }
  const std::vector<Finding>& findings() const { return findings_; }
  std::vector<Finding>& findings() { return findings_; }

 private:
  std::map<std::string, Entity> entities_;
  std::vector<Relation> relations_;
  std::vector<Finding> findings_;
};

}  // namespace model
}  // namespace ds

#endif  // DRIFTSPEC_MODEL_MODEL_HPP
