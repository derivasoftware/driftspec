#ifndef DRIFTSPEC_FORMAT_SCENEFORMATS_HPP
#define DRIFTSPEC_FORMAT_SCENEFORMATS_HPP

#include <map>
#include <memory>
#include <string>

#include "ds/format/Format.hpp"
#include "ds/scene/Scene.hpp"

namespace ds {
namespace format {

/// The scene as compact JSON (ds render -f json).
class SceneJson : public IFormat<scene::SceneDocument> {
 public:
  std::string name() const override;
  std::string write(const scene::SceneDocument& document, const FormatOptions& options) const override;
};

/// The viewer as one self-contained page with the scene inside, or that page as an iframe when
/// options.embed asks (ds render -f html).
class SceneHtml : public IFormat<scene::SceneDocument> {
 public:
  std::string name() const override;
  std::string write(const scene::SceneDocument& document, const FormatOptions& options) const override;
};

/// The one place that names the formats of the scene, keyed by name.
std::map<std::string, std::unique_ptr<IFormat<scene::SceneDocument>>> sceneFormats();

}  // namespace format
}  // namespace ds

#endif  // DRIFTSPEC_FORMAT_SCENEFORMATS_HPP
