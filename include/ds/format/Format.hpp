#ifndef DRIFTSPEC_FORMAT_FORMAT_HPP
#define DRIFTSPEC_FORMAT_FORMAT_HPP

#include <string>
#include <vector>

#include "ds/scene/Scene.hpp"

namespace ds {
namespace format {

/// What every format is given besides its document (DS-126).
struct FormatOptions {
  bool pretty = false;             ///< JSON indented for a reader
  std::string title;               ///< what a page or a frame is titled after
  std::string css;                 ///< a reader's stylesheet, applied after the viewer's
  std::vector<std::string> focus;  ///< the identities a page opens framed on
  bool embed = false;              ///< the page as an iframe that carries it whole
  int height = 640;                ///< the height of that frame, in pixels
};

/// A format writes one kind of document (DS-126): every output of ds is a document written by a
/// format, and the formats of a document are found by name. A format never reads the model. To
/// add one: a class implementing IFormat<that document>, and one line in the map of its document.
template <typename D>
class IFormat {
 public:
  virtual ~IFormat() = default;
  virtual std::string name() const = 0;
  virtual std::string write(const D& document, const FormatOptions& options) const = 0;
};

/// What two formats or more need: knows no format and never reads the model.
namespace common {

/// Text deflated (zlib format) and written in base64: how the scene travels inside a page, which a
/// browser inflates with its own DecompressionStream.
std::string pack(const std::string& text);
/// The text a pack holds; empty when it holds none.
std::string unpack(const std::string& packed);
/// The scene as compact JSON that says how to read itself: every record is an array whose columns
/// "fields" names, and a node, a behaviour or a link is named by its index.
std::string encode(const scene::SceneDocument& document, bool pretty);

}  // namespace common

}  // namespace format
}  // namespace ds

#endif  // DRIFTSPEC_FORMAT_FORMAT_HPP
