#ifndef DRIFTSPEC_BIND_BINDER_HPP
#define DRIFTSPEC_BIND_BINDER_HPP

#include <set>
#include <string>
#include <vector>

#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace ds {
namespace bind {

/// A reference found while binding, resolved later by the Checker.
struct Reference {
  std::string from;          ///< identity of the referring entity
  std::string target;        ///< the name as written
  std::string scope;         ///< qualified scope where it was written (R4)
  std::string relation;      ///< inherits, uses, verifies, refines, realises, exercises, refers
  model::Kind expectedKind;  ///< what to create when nobody declares it (R6)
  model::Location location;
  std::string multiplicity;       ///< for owns/refs/shares (R16)
  std::vector<std::string> args;  ///< template arguments as written, for Repository<Finding> (R15)
  /// a name read out of a type, a base or a bound: it relates only to what the design declares, and is
  /// text otherwise, with no expected node and no finding (DS-18)
  bool soft = false;
};

/// One name a type's text holds (pkg@mod::Name), with the arguments of a bracket written right after it.
struct TypeToken {
  std::string name;
  std::vector<std::string> args;
};
/// The names of a type's text, left to right. The text is the code's: no name of any language is known,
/// so `std::map<std::string, Pool*>` holds std::map (two arguments), std::string and Pool (DS-18).
/// A dot qualifies as `::` does: `lsp.Location` is the name lsp::Location (DS-319).
std::vector<TypeToken> scanTypeTokens(const std::string& text);

/// A message of a behaviour: `from -> to.op(args) -> result`, checked against the static
/// context of the behaviour's subject (self, attributes, parameters, locals).
struct Message {
  std::string sequence;  ///< identity of the behaviour
  std::string message;   ///< identity of the message entity
  std::string to;        ///< participant name as written; "self" when omitted
  std::string op;
  int argc = 0;
  model::Location location;
  bool returns = false;  ///< a `return` of the behaviour, not a call
};

/// A transition event (or action) of a state machine, checked against the entity's operations.
struct Event {
  std::string state;
  std::string entity;  ///< as written
  std::string scope;
  std::string event;
  bool action = false;  ///< `/ action` rather than `on event`
  model::Location location;
};

/// Walks the syntax trees of a workspace and declares its entities (R1-R5, R9).
class Binder {
 public:
  explicit Binder(model::Model& model) : model_(model) {}
  void bind(const parse::ParsedFile& file);

  const std::vector<Reference>& references() const { return references_; }
  const std::vector<Message>& messages() const { return messages_; }
  const std::vector<Event>& events() const { return events_; }

 private:
  void visit(const parse::ParsedFile& file, TSNode node, const std::string& scope, const std::string& owner);
  void bindFields(const parse::ParsedFile& file, TSNode node, model::Entity& out_entity);
  void bindAnnotations(const parse::ParsedFile& file, TSNode node, const std::string& from, const std::string& scope);
  /// What a rule or a definition says, read from its clauses into the model's formulas (DS-94).
  void bindFormula(const parse::ParsedFile& file, TSNode node, model::Entity& out_entity);
  void bindArtefact(const parse::ParsedFile& file, TSNode node, const std::string& scope, const std::string& owner);
  void bindUseCase(const parse::ParsedFile& file, TSNode node, const std::string& scope, const std::string& owner);
  void bindClass(const parse::ParsedFile& file, TSNode node, const std::string& scope, const std::string& owner);
  /// An extension block (annotations only) adds relations to an entity defined elsewhere, or expects it.
  void extend(const parse::ParsedFile& file, TSNode node, const std::string& id, model::Kind kind, const std::string& name,
              const std::string& scope, const model::Location& loc);
  void bindMember(const parse::ParsedFile& file, TSNode node, model::Entity& out_cls, const std::vector<std::string>& params,
                  const std::set<std::string>& operators);
  /// A function of a module: a method's signature and body with the module as its scope.
  void bindFunction(const parse::ParsedFile& file, TSNode node, const std::string& scope, const std::string& owner);
  /// The parameters and the return of a method or a function: their text, and the use they give.
  void bindSignature(const parse::ParsedFile& file, TSNode node, model::Entity& out_entity, const std::string& scope,
                     const std::vector<std::string>& params);
  void bindMemberBody(const parse::ParsedFile& file, TSNode member, model::Entity& out_entity, const std::string& scope);
  void bindSequence(const parse::ParsedFile& file, TSNode node, const std::string& scope, const std::string& owner);
  void bindSteps(const parse::ParsedFile& file, TSNode node, model::Entity& out_sequence, const std::string& holder, int& out_order,
                 int& out_frames);
  /// One side of an alt (DS-172): a frame of kind branch the alt contains, `side` 0 for the first, 1 for the else.
  std::string branch(const parse::ParsedFile& file, TSNode at, const std::string& sequence, const std::string& alt, int side,
                     const std::string& scenario);
  /// Relate `from` to every name of a type that the design declares; the type's text, trimmed (DS-18).
  std::string bindType(const parse::ParsedFile& file, TSNode type, const std::string& scope, const std::string& from,
                       const std::string& relation, const std::string& multiplicity, const std::vector<std::string>& params,
                       const model::Location& location);
  void declareOperator(model::Entity& out_cls, const std::string& symbol, const model::Location& location);
  model::Location locationOf(const parse::ParsedFile& file, TSNode node) const;

  model::Model& model_;
  std::vector<Reference> references_;
  std::vector<Message> messages_;
  std::vector<Event> events_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_BINDER_HPP
