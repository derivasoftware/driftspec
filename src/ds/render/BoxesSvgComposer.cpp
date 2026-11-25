#include "ds/render/BoxesSvgComposer.hpp"

namespace ds {
namespace render {

Figure BoxesSvgComposer::compose(const IrDocument& doc, int width) const {
  Figure figure;
  figure.document = doc.bundled();
  figure.layout = Layout(metrics_).place(figure.document, width, true);
  return figure;
}

}  // namespace render
}  // namespace ds
