#ifndef DRIFTSPEC_FORMAT_SCENEFORMATS_HPP
#define DRIFTSPEC_FORMAT_SCENEFORMATS_HPP

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ds/emit/Sink.hpp"
#include "ds/format/Format.hpp"
#include "ds/scene/Scene.hpp"

namespace ds {
namespace format {

/// The scene as compact JSON (ds render -f json).
class SceneJson : public IFormat<scene::SceneDocument> {
 public:
  std::string name() const override;
  std::string write(const scene::SceneDocument& document, const FormatOptions& options, emit::ISink& out_sink) const override;
};

/// The viewer as one self-contained page with the scene inside, or that page as an iframe when
/// options.embed asks (ds render -f html). The scene and the identities it opens framed on travel inside the
/// page as JSON, written one after the other into the sink it is lent, which is compact.
class SceneHtml : public IFormat<scene::SceneDocument> {
 public:
  std::string name() const override;
  std::string write(const scene::SceneDocument& document, const FormatOptions& options, emit::ISink& out_sink) const override;

 private:
  /// The viewer as one self-contained page with the scene inside, titled after the workspace.
  static std::string page(const scene::SceneDocument& document, const std::string& title, const std::string& css,
                          const std::vector<std::string>& focus, emit::ISink& out_sink);
  /// A page as an iframe that carries it whole (srcdoc), to paste into any HTML document.
  static std::string frame(const std::string& page, const std::string& title, int height);
};

/// The one place that names the formats of the scene, keyed by name.
std::map<std::string, std::unique_ptr<IFormat<scene::SceneDocument>>> sceneFormats();

}  // namespace format
}  // namespace ds

#endif  // DRIFTSPEC_FORMAT_SCENEFORMATS_HPP
