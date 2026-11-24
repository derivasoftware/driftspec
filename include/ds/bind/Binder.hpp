#ifndef DRIFTSPEC_BIND_BINDER_HPP
#define DRIFTSPEC_BIND_BINDER_HPP

#include <string>
#include <vector>

#include "ds/bind/AnnotationBinder.hpp"
#include "ds/bind/ArtefactBinder.hpp"
#include "ds/bind/BindContext.hpp"
#include "ds/bind/ClassBinder.hpp"
#include "ds/bind/DiagramBinder.hpp"
#include "ds/bind/Dispatch.hpp"
#include "ds/bind/EnumBinder.hpp"
#include "ds/bind/ExtensionBinder.hpp"
#include "ds/bind/FieldBinder.hpp"
#include "ds/bind/FormulaBinder.hpp"
#include "ds/bind/FunctionBinder.hpp"
#include "ds/bind/InterfaceBinder.hpp"
#include "ds/bind/LegacyBinder.hpp"
#include "ds/bind/MemberBinder.hpp"
#include "ds/bind/MemberBodyBinder.hpp"
#include "ds/bind/ModuleBinder.hpp"
#include "ds/bind/Notes.hpp"
#include "ds/bind/PackageBinder.hpp"
#include "ds/bind/RuleBinder.hpp"
#include "ds/bind/SequenceBinder.hpp"
#include "ds/bind/SignatureBinder.hpp"
#include "ds/bind/StateBinder.hpp"
#include "ds/bind/TypeBinder.hpp"
#include "ds/bind/TypeTokens.hpp"
#include "ds/bind/UseCaseBinder.hpp"
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
  // the parts every construct shares, then the binders of the constructs, each given the parts it uses (DS-356)
  FieldBinder fields_;
  AnnotationBinder annotations_;
  ExtensionBinder extensions_{annotations_};
  TypeBinder types_;
  SignatureBinder signatures_{types_};
  FormulaBinder formulas_;
  MemberBodyBinder bodies_{fields_, annotations_};
  MemberBinder members_{types_, signatures_, bodies_};
  ArtefactBinder artefacts_{fields_, annotations_, extensions_};
  PackageBinder packages_{fields_, annotations_};
  ModuleBinder modules_{fields_, annotations_};
  InterfaceBinder interfaces_{fields_, annotations_, signatures_};
  EnumBinder enums_{annotations_};
  RuleBinder rules_{fields_, annotations_, formulas_};
  DiagramBinder diagrams_{fields_, formulas_};
  UseCaseBinder usecases_{fields_, annotations_, types_, signatures_};
  StateBinder states_{fields_, annotations_};
  ClassBinder classes_{fields_, annotations_, extensions_, members_};
  FunctionBinder functions_{signatures_, bodies_};
  SequenceBinder sequences_{fields_, annotations_, types_};
  LegacyBinder legacy_;
  Dispatch dispatch_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_BINDER_HPP
