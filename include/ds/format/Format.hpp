#ifndef DRIFTSPEC_FORMAT_FORMAT_HPP
#define DRIFTSPEC_FORMAT_FORMAT_HPP

#include <string>
#include <vector>

#include "ds/emit/Sink.hpp"

namespace ds {
namespace format {

/// What a writer is given besides its document (DS-126): what the viewer's page is titled, styled, framed and embedded as.
struct FormatOptions {
  std::string title;                    ///< what a page or a frame is titled after
  std::string css = {};                 ///< a reader's stylesheet, applied after the viewer's
  std::vector<std::string> focus = {};  ///< the identities a page opens framed on
  bool embed = false;                   ///< the page as an iframe that carries it whole
  int height = 640;                     ///< the height of that frame, in pixels
};

/// A writer of one kind of document that is no description yet (DS-126, DS-398): the viewer's page, which is the
/// scene in JSON inside its template (the drawings in svg and text are see-design's since DS-474, the IR a description
/// since DS-418). The writers of a document are found by name in the map FormatFactory makes; a writer never reads the
/// model. Whoever asks for the output lends the sink (DS-397): the page writes the scene into it.
template <typename D>
class IFormat {
 public:
  virtual ~IFormat() = default;
  virtual std::string name() const = 0;
  virtual std::string write(const D& document, const FormatOptions& options, emit::ISink& out_sink) const = 0;
};

/// What the writers share: it knows no description and no writer, and never reads the model.
namespace common {

/// Text deflated (zlib format) and written in base64: how the scene and the viewer travel inside a page, which a
/// browser inflates with its own DecompressionStream.
class Packing {
 public:
  static std::string pack(const std::string& text);
  /// The text a pack holds; empty when it holds none.
  static std::string unpack(const std::string& packed);
};

}  // namespace common

}  // namespace format
}  // namespace ds

#endif  // DRIFTSPEC_FORMAT_FORMAT_HPP
