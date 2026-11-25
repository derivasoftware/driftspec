#ifndef DRIFTSPEC_RENDER_ICOMPOSER_HPP
#define DRIFTSPEC_RENDER_ICOMPOSER_HPP

#include "ds/render/Figure.hpp"
#include "ds/render/Ir.hpp"

namespace ds {
namespace render {

/// What composes one kind of diagram for one technology (DS-400): a document in, its figure out, placed with the
/// technology's metrics within `width` (in its units; 0 = unbounded). ComposerFactory names them.
class IComposer {
 public:
  virtual ~IComposer() = default;
  /// The figure of `doc`, within `width`.
  virtual Figure compose(const IrDocument& doc, int width) const = 0;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_ICOMPOSER_HPP
