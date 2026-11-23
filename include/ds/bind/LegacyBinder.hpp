#ifndef DRIFTSPEC_BIND_LEGACYBINDER_HPP
#define DRIFTSPEC_BIND_LEGACYBINDER_HPP

#include <set>
#include <string>
#include <vector>

#include "ds/bind/BindContext.hpp"
#include "ds/bind/INodeBinder.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace bind {

/// The constructs with no binder of their own yet (DS-355), bound by the walk the binder had: DS-356 takes them out one
/// group at a time, and DS-357 removes what is left.
class LegacyBinder : public INodeBinder {
 public:
  std::vector<std::string> kinds() const override;
  void bind(TSNode node, const BindContext& ctx) const override;

 private:
  // every method binds through its context: the model and the notes are written there and nowhere else (DS-354)
  /// What a rule or a definition says, read from its clauses into the model's formulas (DS-94).
  void bindFormula(const BindContext& ctx, TSNode node, model::Entity& out_entity) const;
  void bindUseCase(const BindContext& ctx, TSNode node) const;
  void bindClass(const BindContext& ctx, TSNode node) const;
  void bindMember(const BindContext& ctx, TSNode node, const model::Entity& cls, const std::vector<std::string>& params,
                  const std::set<std::string>& operators) const;
  /// A function of a module: a method's signature and body with the module as its scope.
  void bindFunction(const BindContext& ctx, TSNode node) const;
  /// The parameters and the return of a method or a function: their text, and the use they give.
  void bindSignature(const BindContext& ctx, TSNode node, model::Entity& out_entity, const std::string& scope,
                     const std::vector<std::string>& params) const;
  void bindMemberBody(const BindContext& ctx, TSNode member, model::Entity& out_entity, const std::string& scope) const;
  void bindSequence(const BindContext& ctx, TSNode node) const;
  void bindSteps(const BindContext& ctx, TSNode node, const model::Entity& sequence, const std::string& holder, int& out_order,
                 int& out_frames) const;
  /// One side of an alt (DS-172): a frame of kind branch the alt contains, `side` 0 for the first, 1 for the else.
  std::string branch(const BindContext& ctx, TSNode at, const std::string& sequence, const std::string& alt, int side,
                     const std::string& scenario) const;
  /// Relate `from` to every name of a type that the design declares; the type's text, trimmed (DS-18).
  std::string bindType(const BindContext& ctx, TSNode type, const std::string& scope, const std::string& from, const std::string& relation,
                       const std::string& multiplicity, const std::vector<std::string>& params, const model::Location& location) const;
  void declareOperator(const BindContext& ctx, const model::Entity& cls, const std::string& symbol, const model::Location& location) const;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_LEGACYBINDER_HPP
