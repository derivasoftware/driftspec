#include "ds/render/BoxesTextComposer.hpp"

namespace ds {
namespace render {

Figure BoxesTextComposer::compose(const IrDocument& doc, int width) const {
  Figure figure;
  figure.owned = doc.bundled();
  figure.layout = Layout(metrics_).place(figure.owned, width);
  return figure;
}

}  // namespace render
}  // namespace ds
