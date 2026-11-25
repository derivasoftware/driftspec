#ifndef DRIFTSPEC_RENDER_COMPOSERFACTORY_HPP
#define DRIFTSPEC_RENDER_COMPOSERFACTORY_HPP

#include <memory>
#include <string>

#include "ds/render/IComposer.hpp"
#include "ds/render/Layout.hpp"

namespace ds {
namespace render {

/// The one place that names the composers (DS-400), one per kind of diagram and technology: what composes a `kind`
/// ("boxes", "sequence") drawn in `technology` ("svg", "text"), with that technology's metrics.
class ComposerFactory {
 public:
  static std::unique_ptr<IComposer> of(const std::string& kind, const std::string& technology, const Metrics& metrics);
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_COMPOSERFACTORY_HPP
