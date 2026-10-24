#ifndef DRIFTSPEC_BIND_BINDER_HPP
#define DRIFTSPEC_BIND_BINDER_HPP

#include <string>
#include <vector>

#include "driftspec/model/Model.hpp"
#include "driftspec/parse/Parser.hpp"

namespace driftspec {
namespace bind {

/// A reference found while binding, resolved later by the Checker.
struct Reference {
  std::string from;        ///< identity of the referring entity
  std::string target;      ///< the name as written
  std::string scope;       ///< qualified scope where it was written (R4)
  std::string relation;    ///< inherits, uses, verifies, refines, realises, exercises, refers
  model::Kind expectedKind;  ///< what to create when nobody declares it (R6)
  model::Location location;
};

/// A message of a sequence: `from -> to.op(args)`, checked against the static model.
struct Message {
  std::string sequence;
  std::string participantType;  ///< as written, resolved later
  std::string scope;
  std::string op;
  model::Location location;
};

/// A transition event of a state machine, checked against the entity's operations.
struct Event {
  std::string state;
  std::string entity;  ///< as written
  std::string scope;
  std::string event;
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
  void bindFields(const parse::ParsedFile& file, TSNode node, model::Entity& entity);
  void bindAnnotations(const parse::ParsedFile& file, TSNode node, const std::string& from, const std::string& scope);
  model::Location locationOf(const parse::ParsedFile& file, TSNode node) const;
  std::string qualify(const std::string& scope, const std::string& name) const;

  model::Model& model_;
  std::vector<Reference> references_;
  std::vector<Message> messages_;
  std::vector<Event> events_;
};

}  // namespace bind
}  // namespace driftspec

#endif  // DRIFTSPEC_BIND_BINDER_HPP
