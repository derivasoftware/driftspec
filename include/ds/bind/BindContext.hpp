#ifndef DRIFTSPEC_BIND_BINDCONTEXT_HPP
#define DRIFTSPEC_BIND_BINDCONTEXT_HPP

#include <string>

#include "ds/bind/Dispatch.hpp"
#include "ds/bind/Notes.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/TreeReader.hpp"

namespace ds {
namespace bind {

/// What binding one node is given (DS-354, DS-355): the model it declares into and the notes it leaves for the checker,
/// the only two things it writes; the reader of the file's tree; the language's metamodel; the table that binds the
/// node's children; and where the node is written, its scope and the entity that holds it. A binder writes through
/// this alone, so a file could later bind into a batch of its own. It holds no binding of its own (DS-356).
struct BindContext {
  model::Model& model;
  Notes& notes;
  const parse::TreeReader& reader;
  const model::Metamodel& metamodel;
  const Dispatch& dispatch;
  std::string scope;
  std::string owner;

  /// The same context for a node written in `inner` and held by `holder`.
  BindContext within(const std::string& inner, const std::string& holder) const {
    return {model, notes, reader, metamodel, dispatch, inner, holder};
  }
  /// Where a node is written, as the model keeps it.
  model::Location locationOf(TSNode node) const {
    const parse::Span s = reader.spanOf(node);
    return {reader.file().path, s.line, s.column, s.endLine, s.endColumn};  // the path copied once, as before DS-354
  }
  /// Binds the named children of `node` here, each by its binder.
  void children(TSNode node) const { dispatch.bindChildren(node, *this); }

  const model::Entity& declare(const model::Entity& entity) const { return model.declare(entity); }
  void relate(const model::Relation& relation) const { model.relate(relation); }
  void report(const model::Finding& finding) const { model.report(finding); }
  /// An entity already declared; null when there is none.
  const model::Entity* find(const std::string& id) const { return model.find(id); }
  /// A field of an entity the model keeps (DS-346).
  void annotate(const std::string& id, const std::string& key, const std::string& value) const { model.annotate(id, key, value); }
  /// What a later declaration says of an entity the model keeps (DS-346).
  void refine(const model::Entity& later) const { model.refine(later); }
  /// What a rule, a definition or a table states (DS-94, DS-268).
  void state(const std::string& id, const model::Formula& formula) const { model.state(id, formula); }
  /// A name written, for the checker to resolve.
  void refer(const Reference& reference) const { notes.references.push_back(reference); }
  /// A message of a behaviour, for the checker to type.
  void noteMessage(const Message& message) const { notes.messages.push_back(message); }
  /// An event of a state machine, for the checker to find on its entity.
  void noteEvent(const Event& event) const { notes.events.push_back(event); }
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_BINDCONTEXT_HPP
