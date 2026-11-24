#ifndef DRIFTSPEC_BIND_BINDER_HPP
#define DRIFTSPEC_BIND_BINDER_HPP

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ds/bind/AnnotationBinder.hpp"
#include "ds/bind/BindContext.hpp"
#include "ds/bind/Dispatch.hpp"
#include "ds/bind/ExtensionBinder.hpp"
#include "ds/bind/FieldBinder.hpp"
#include "ds/bind/FormulaBinder.hpp"
#include "ds/bind/INodeBinder.hpp"
#include "ds/bind/MemberBinder.hpp"
#include "ds/bind/MemberBodyBinder.hpp"
#include "ds/bind/Notes.hpp"
#include "ds/bind/SignatureBinder.hpp"
#include "ds/bind/TypeBinder.hpp"
#include "ds/bind/TypeTokens.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace ds {
namespace bind {

/// Walks the syntax trees of a workspace and declares its entities (R1-R5, R9). Each construct of the language goes to
/// its binder by the dispatch table (DS-353), the binders built once with the binder and the table with the first file
/// it binds, from the grammar that file was parsed with (DS-374).
class Binder {
 public:
  /// Binds into `model`, reading the language from `metamodel` (DS-341): what an annotation means, which ones it knows.
  Binder(model::Model& model, const model::Metamodel& metamodel);
  Binder(const Binder&) = delete;  // the binders hold the parts it owns, and the table points at the binders
  void bind(const parse::ParsedFile& file);

  /// What binding leaves for the checker (DS-354): the checker reads this, never the binder.
  const Notes& notes() const { return notes_; }

 private:
  model::Model& model_;
  const model::Metamodel& metamodel_;
  Notes notes_;
  // the parts every construct shares, each given to the node binders that use it (DS-356)
  FieldBinder fields_;
  AnnotationBinder annotations_;
  ExtensionBinder extensions_{annotations_};
  TypeBinder types_;
  SignatureBinder signatures_{types_};
  FormulaBinder formulas_;
  MemberBodyBinder bodies_{fields_, annotations_};
  MemberBinder members_{types_, signatures_, bodies_};
  // the node binders, held as what they are to the binder: each binds the kinds it takes; only the constructor
  // names the concrete ones (DS-365)
  std::vector<std::unique_ptr<INodeBinder>> binders_;
  std::optional<Dispatch> dispatch_;  // built from the grammar of the first file: a workspace is read by one grammar
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_BINDER_HPP
