#include "ds/render/BoxesSvgComposer.hpp"

namespace ds {
namespace render {

Figure BoxesSvgComposer::compose(const IrDocument& doc, int width) const {
  Figure figure;
  figure.owned = doc.bundled();
  figure.layout = Layout(metrics_).place(figure.owned, width, true);
  return figure;
}

}  // namespace render
}  // namespace ds
