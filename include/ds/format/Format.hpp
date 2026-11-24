#ifndef DRIFTSPEC_FORMAT_FORMAT_HPP
#define DRIFTSPEC_FORMAT_FORMAT_HPP

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ds/check/Report.hpp"
#include "ds/emit/Sink.hpp"
#include "ds/model/Model.hpp"
#include "ds/scene/Map.hpp"
#include "ds/scene/Scene.hpp"

namespace ds {
namespace format {

/// What every format is given besides its document (DS-126).
struct FormatOptions {
  bool pretty = false;                  ///< the IR indented for a reader; a JSON format is indented as the sink it is lent
  std::string title;                    ///< what a page or a frame is titled after
  std::string css = {};                 ///< a reader's stylesheet, applied after the viewer's
  std::vector<std::string> focus = {};  ///< the identities a page opens framed on
  bool embed = false;                   ///< the page as an iframe that carries it whole
  int height = 640;                     ///< the height of that frame, in pixels
  int width = 0;                        ///< wrap a drawing: columns for text and cells, pixels for svg; 0 lets it be
  bool color = false;                   ///< text for a terminal that shows colour
};

/// A format writes one kind of document (DS-126): every output of ds is a document written by a
/// format, and the formats of a document are found by name. A format never reads the model. To
/// add one: a class implementing IFormat<that document>, and one line in the map of its document.
/// Whoever asks for the output lends the sink a JSON format writes into (DS-397): such a format
/// returns what the sink holds; one that writes text returns its text and leaves the sink alone.
template <typename D>
class IFormat {
 public:
  virtual ~IFormat() = default;
  virtual std::string name() const = 0;
  virtual std::string write(const D& document, const FormatOptions& options, emit::ISink& out_sink) const = 0;
};

/// What two formats or more need: knows no format and never reads the model.
namespace common {

/// Text deflated (zlib format) and written in base64: how the scene travels inside a page, which a
/// browser inflates with its own DecompressionStream.
std::string pack(const std::string& text);
/// The text a pack holds; empty when it holds none.
std::string unpack(const std::string& packed);

/// The scene as JSON that says how to read itself: every record is an array whose columns "fields" names, and
/// a node, a behaviour or a link is named by its index (DS-397: written into the sink it is lent).
class SceneEncoder {
 public:
  explicit SceneEncoder(const scene::SceneDocument& document);
  /// The scene as one value where the sink stands.
  void encode(emit::ISink& out_sink) const;

 private:
  /// The index of a node, of a requirement; -1 when there is none.
  int node(const std::string& id) const;
  int requirement(const std::string& id) const;
  /// The index of the behaviour a sequence is (X::sequence or X::sequence/scenario), landed where the map
  /// says; -1 when none.
  int behaviourOf(const std::string& sequence) const;
  /// A box as its four whole numbers: x, y, w, h.
  static void box(const layout::Box& b, emit::ISink& out_sink);
  static void texts(const std::vector<std::string>& texts, emit::ISink& out_sink);
  static void pairs(const std::vector<std::pair<std::string, std::string>>& pairs, emit::ISink& out_sink);

  const scene::SceneDocument& document_;
  std::map<std::string, int> nodeAt_;         ///< each node's index
  std::map<std::string, int> behaviourAt_;    ///< "subject / scenario" -> its index
  std::map<std::string, int> requirementAt_;  ///< each requirement's index
  scene::Map landing_;                        ///< where a link's sequence lands: the behaviour it is
};

/// A check's report as one record of its findings and counts, the one shape every caller reads (DS-397): what
/// ds check --json writes whole, and what the browser's answer carries as one of its fields. It belongs to no
/// family, so a door that carries the report names no format.
class ReportEncoder {
 public:
  explicit ReportEncoder(const check::Report& report);
  /// The report as one value where the sink stands.
  void encode(emit::ISink& out_sink) const;

 private:
  /// Findings or coverage: code, location, message and the severity's name in the report's table.
  void findings(const std::vector<model::Finding>& findings, emit::ISink& out_sink) const;

  const check::Report& report_;
};

}  // namespace common

}  // namespace format
}  // namespace ds

#endif  // DRIFTSPEC_FORMAT_FORMAT_HPP
