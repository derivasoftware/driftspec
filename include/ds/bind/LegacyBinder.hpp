#ifndef DRIFTSPEC_BIND_LEGACYBINDER_HPP
#define DRIFTSPEC_BIND_LEGACYBINDER_HPP

#include <set>
#include <string>
#include <vector>

#include "ds/bind/AnnotationBinder.hpp"
#include "ds/bind/BindContext.hpp"
#include "ds/bind/ExtensionBinder.hpp"
#include "ds/bind/FieldBinder.hpp"
#include "ds/bind/INodeBinder.hpp"
#include "ds/bind/SignatureBinder.hpp"
#include "ds/bind/TypeBinder.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace bind {

/// The constructs with no binder of their own yet (DS-355), bound by the walk the binder had: DS-356 takes them out one
/// group at a time, and DS-357 removes what is left.
class LegacyBinder : public INodeBinder {
 public:
  LegacyBinder(const FieldBinder& fields, const AnnotationBinder& annotations, const ExtensionBinder& extensions, const TypeBinder& types,
               const SignatureBinder& signatures)
      : fields_(fields), annotations_(annotations), extensions_(extensions), types_(types), signatures_(signatures) {}
  std::vector<std::string> kinds() const override;
  void bind(TSNode node, const BindContext& ctx) const override;

 private:
  // every method binds through its context: the model and the notes are written there and nowhere else (DS-354)
  void bindClass(const BindContext& ctx, TSNode node) const;
  void bindMember(const BindContext& ctx, TSNode node, const model::Entity& cls, const std::vector<std::string>& params,
                  const std::set<std::string>& operators) const;
  /// A function of a module: a method's signature and body with the module as its scope.
  void bindFunction(const BindContext& ctx, TSNode node) const;
  void bindMemberBody(const BindContext& ctx, TSNode member, model::Entity& out_entity, const std::string& scope) const;
  void bindSequence(const BindContext& ctx, TSNode node) const;
  void bindSteps(const BindContext& ctx, TSNode node, const model::Entity& sequence, const std::string& holder, int& out_order,
                 int& out_frames) const;
  /// One side of an alt (DS-172): a frame of kind branch the alt contains, `side` 0 for the first, 1 for the else.
  std::string branch(const BindContext& ctx, TSNode at, const std::string& sequence, const std::string& alt, int side,
                     const std::string& scenario) const;
  void declareOperator(const BindContext& ctx, const model::Entity& cls, const std::string& symbol, const model::Location& location) const;
  const FieldBinder& fields_;
  const AnnotationBinder& annotations_;
  const ExtensionBinder& extensions_;
  const TypeBinder& types_;
  const SignatureBinder& signatures_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_LEGACYBINDER_HPP
