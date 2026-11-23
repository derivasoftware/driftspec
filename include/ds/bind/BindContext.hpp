#ifndef DRIFTSPEC_BIND_BINDCONTEXT_HPP
#define DRIFTSPEC_BIND_BINDCONTEXT_HPP

#include <string>

#include "ds/bind/Notes.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/TreeReader.hpp"

namespace ds {
namespace bind {

/// What binding one node is given (DS-354): the model it declares into and the notes it leaves for the checker, the
/// only two things it writes; the reader of the file's tree; and where the node is written, its scope and the entity
/// that holds it. Binding writes through this alone, so a file could later bind into a batch of its own.
struct BindContext {
  model::Model& model;
  Notes& notes;
  const parse::TreeReader& reader;
  std::string scope;
  std::string owner;

  /// The same context for a node written in `inner` and held by `holder`.
  BindContext within(const std::string& inner, const std::string& holder) const { return {model, notes, reader, inner, holder}; }
  /// Where a node is written, as the model keeps it.
  model::Location locationOf(TSNode node) const {
    const parse::Span s = reader.spanOf(node);
    return {s.file, s.line, s.column, s.endLine, s.endColumn};
  }
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_BINDCONTEXT_HPP
