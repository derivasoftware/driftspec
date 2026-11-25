#ifndef DRIFTSPEC_RENDER_BOXESSVGCOMPOSER_HPP
#define DRIFTSPEC_RENDER_BOXESSVGCOMPOSER_HPP

#include "ds/render/Figure.hpp"
#include "ds/render/IComposer.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/Layout.hpp"

namespace ds {
namespace render {

/// A box diagram composed for SVG (DS-400): laid out in pixels, its edges routed as its kind is drawn (DS-443).
class BoxesSvgComposer : public IComposer {
 public:
  explicit BoxesSvgComposer(const Metrics& metrics) : metrics_(metrics) {}
  Figure compose(const IrDocument& doc, int width) const override;

 private:
  const Metrics metrics_;
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_BOXESSVGCOMPOSER_HPP
