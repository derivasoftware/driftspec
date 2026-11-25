#ifndef DRIFTSPEC_FORMAT_VIEWERPAGE_HPP
#define DRIFTSPEC_FORMAT_VIEWERPAGE_HPP

#include <string>
#include <vector>

#include "ds/emit/Sink.hpp"
#include "ds/format/Format.hpp"
#include "ds/scene/Document.hpp"

namespace ds {
namespace format {

/// The viewer as one self-contained page with the scene inside, or that page as an iframe when options.embed
/// asks (ds render -f html). It is no technology (DS-398): it is the scene in JSON inside its template. The scene
/// is described into the sink the page is lent, which is compact, and so are the identities it opens framed on,
/// one after the other; FormatFactory names it.
class ViewerPage : public IFormat<scene::SceneDocument> {
 public:
  std::string name() const override;
  std::string write(const scene::SceneDocument& document, const FormatOptions& options, emit::ISink& out_sink) const override;
  /// The page ds carries inside: viewer/scene.html, compiled into ds by CMake, with the places the scene fills.
  static const std::string& viewerTemplate();

 private:
  /// The viewer as one self-contained page with the scene inside, titled after the workspace.
  static std::string page(const scene::SceneDocument& document, const std::string& title, const std::string& css,
                          const std::vector<std::string>& focus, emit::ISink& out_sink);
  /// A page as an iframe that carries it whole (srcdoc), to paste into any HTML document.
  static std::string frame(const std::string& page, const std::string& title, int height);
};

}  // namespace format
}  // namespace ds

#endif  // DRIFTSPEC_FORMAT_VIEWERPAGE_HPP
