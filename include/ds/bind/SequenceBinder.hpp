#ifndef DRIFTSPEC_BIND_SEQUENCEBINDER_HPP
#define DRIFTSPEC_BIND_SEQUENCEBINDER_HPP

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

/// A sequence (DS-356): the behaviour of an operation, its steps and the frames that hold them, an alt in two
/// branches; its messages are kept for the checker to type.
class SequenceBinder : public INodeBinder {
 public:
  SequenceBinder(const FieldBinder& fields, const AnnotationBinder& annotations, const TypeBinder& types)
      : fields_(fields), annotations_(annotations), types_(types) {}
  std::vector<std::string> kinds() const override;
  void bind(TSNode node, const BindContext& ctx) const override;

 private:
  // every method binds through its context: the model and the notes are written there and nowhere else (DS-354)
  void bindSteps(const BindContext& ctx, TSNode node, const model::Entity& sequence, const std::string& holder, int& out_order,
                 int& out_frames) const;
  /// One side of an alt (DS-172): a frame of kind branch the alt contains, `side` 0 for the first, 1 for the else.
  std::string branch(const BindContext& ctx, TSNode at, const std::string& sequence, const std::string& alt, int side,
                     const std::string& scenario) const;
  const FieldBinder& fields_;
  const AnnotationBinder& annotations_;
  const TypeBinder& types_;
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_SEQUENCEBINDER_HPP
