#include "ds/render/ComposerFactory.hpp"

#include <stdexcept>

#include "ds/render/BoxesSvgComposer.hpp"
#include "ds/render/BoxesTextComposer.hpp"
#include "ds/render/SequenceComposer.hpp"

namespace ds {
namespace render {

std::unique_ptr<IComposer> ComposerFactory::of(const std::string& kind, const std::string& technology, const Metrics& metrics) {
  if (kind == "boxes" && technology == "svg") return std::make_unique<BoxesSvgComposer>(metrics);
  if (kind == "boxes" && technology == "text") return std::make_unique<BoxesTextComposer>(metrics);
  if (kind == "sequence" && (technology == "svg" || technology == "text"))
    return std::make_unique<SequenceComposer>(metrics);  // one engine, the sink's metrics
  throw std::invalid_argument("no composer of " + kind + " drawn in " + technology);
}

}  // namespace render
}  // namespace ds
