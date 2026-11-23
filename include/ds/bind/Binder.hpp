#ifndef DRIFTSPEC_BIND_BINDER_HPP
#define DRIFTSPEC_BIND_BINDER_HPP

#include <string>
#include <vector>

#include "ds/bind/ArtefactBinder.hpp"
#include "ds/bind/BindContext.hpp"
#include "ds/bind/Dispatch.hpp"
#include "ds/bind/LegacyBinder.hpp"
#include "ds/bind/Notes.hpp"
#include "ds/bind/TypeTokens.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace ds {
namespace bind {

/// Walks the syntax trees of a workspace and declares its entities (R1-R5, R9). Each construct of the language goes to
/// its binder by the dispatch table (DS-353), the binders built once with the binder.
class Binder {
 public:
  /// Binds into `model`, reading the language from `metamodel` (DS-341): what an annotation means, which ones it knows.
  Binder(model::Model& model, const model::Metamodel& metamodel);
  Binder(const Binder&) = delete;  // the table points at the binders it holds
  void bind(const parse::ParsedFile& file);

  /// What binding leaves for the checker (DS-354): the checker reads this, never the binder.
  const Notes& notes() const { return notes_; }

 private:
  model::Model& model_;
  const model::Metamodel& metamodel_;
  Notes notes_;
  ArtefactBinder artefacts_;
  LegacyBinder legacy_;
  Dispatch dispatch_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_BINDER_HPP
