#ifndef DRIFTSPEC_BIND_BINDER_HPP
#define DRIFTSPEC_BIND_BINDER_HPP

#include <set>
#include <string>
#include <vector>

#include "ds/bind/BindContext.hpp"
#include "ds/bind/Notes.hpp"
#include "ds/bind/TypeTokens.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace ds {
namespace bind {

/// Walks the syntax trees of a workspace and declares its entities (R1-R5, R9).
class Binder {
 public:
  /// Binds into `model`, reading the language from `metamodel` (DS-341): what an annotation means, which ones it knows.
  Binder(model::Model& model, const model::Metamodel& metamodel) : model_(model), metamodel_(metamodel) {}
  void bind(const parse::ParsedFile& file);

  /// What binding leaves for the checker (DS-354): the checker reads this, never the binder.
  const Notes& notes() const { return notes_; }

 private:
  // every method binds through its context: the model and the notes are written there and nowhere else (DS-354)
  void visit(const BindContext& ctx, TSNode node);
  void bindFields(const BindContext& ctx, TSNode node, model::Entity& out_entity);
  void bindAnnotations(const BindContext& ctx, TSNode node, const std::string& from, const std::string& scope);
  /// What a rule or a definition says, read from its clauses into the model's formulas (DS-94).
  void bindFormula(const BindContext& ctx, TSNode node, model::Entity& out_entity);
  void bindArtefact(const BindContext& ctx, TSNode node);
  void bindUseCase(const BindContext& ctx, TSNode node);
  void bindClass(const BindContext& ctx, TSNode node);
  /// An extension block (annotations only) adds relations to an entity defined elsewhere, or expects it.
  void extend(const BindContext& ctx, TSNode node, const std::string& id, model::Kind kind, const std::string& name,
              const std::string& scope, const model::Location& loc);
  void bindMember(const BindContext& ctx, TSNode node, model::Entity& out_cls, const std::vector<std::string>& params,
                  const std::set<std::string>& operators);
  /// A function of a module: a method's signature and body with the module as its scope.
  void bindFunction(const BindContext& ctx, TSNode node);
  /// The parameters and the return of a method or a function: their text, and the use they give.
  void bindSignature(const BindContext& ctx, TSNode node, model::Entity& out_entity, const std::string& scope,
                     const std::vector<std::string>& params);
  void bindMemberBody(const BindContext& ctx, TSNode member, model::Entity& out_entity, const std::string& scope);
  void bindSequence(const BindContext& ctx, TSNode node);
  void bindSteps(const BindContext& ctx, TSNode node, model::Entity& out_sequence, const std::string& holder, int& out_order,
                 int& out_frames);
  /// One side of an alt (DS-172): a frame of kind branch the alt contains, `side` 0 for the first, 1 for the else.
  std::string branch(const BindContext& ctx, TSNode at, const std::string& sequence, const std::string& alt, int side,
                     const std::string& scenario);
  /// Relate `from` to every name of a type that the design declares; the type's text, trimmed (DS-18).
  std::string bindType(const BindContext& ctx, TSNode type, const std::string& scope, const std::string& from, const std::string& relation,
                       const std::string& multiplicity, const std::vector<std::string>& params, const model::Location& location);
  void declareOperator(const BindContext& ctx, model::Entity& out_cls, const std::string& symbol, const model::Location& location);

  model::Model& model_;
  const model::Metamodel& metamodel_;
  Notes notes_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_BINDER_HPP
