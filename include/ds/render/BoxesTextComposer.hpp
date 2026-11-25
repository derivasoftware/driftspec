#ifndef DRIFTSPEC_RENDER_BOXESTEXTCOMPOSER_HPP
#define DRIFTSPEC_RENDER_BOXESTEXTCOMPOSER_HPP

#include "ds/render/Figure.hpp"
#include "ds/render/IComposer.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/Layout.hpp"

namespace ds {
namespace render {

/// A box diagram composed for a terminal (DS-400): laid out in cells, its edges orthogonal, as a terminal draws\n/// every kind.
class BoxesTextComposer : public IComposer {
 public:
  explicit BoxesTextComposer(const Metrics& metrics) : metrics_(metrics) {}
  Figure compose(const IrDocument& doc, int width) const override;

 private:
  const Metrics metrics_;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_BOXESTEXTCOMPOSER_HPP
