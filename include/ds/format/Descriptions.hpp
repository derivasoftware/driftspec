#ifndef DRIFTSPEC_FORMAT_DESCRIPTIONS_HPP
#define DRIFTSPEC_FORMAT_DESCRIPTIONS_HPP

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ds/check/Report.hpp"
#include "ds/emit/Sink.hpp"
#include "ds/format/Description.hpp"
#include "ds/layout/Box.hpp"
#include "ds/query/Query.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/Layout.hpp"
#include "ds/render/Text.hpp"
#include "ds/scene/Document.hpp"
#include "ds/scene/Map.hpp"
#include "ds/theme/Theme.hpp"

namespace ds {
namespace format {

/// Each description refers to the document it is given, which outlives it: DescriptionFactory makes them, and
/// nobody else names one (DS-398).

/// The scene, for the viewer (ds render -f json, and the scene inside the viewer's page): every record an array
/// whose columns "fields" names, and a node, a behaviour or a link named by its index, so the document says how
/// to read itself. The keys of the one record with keys, the document itself, are written in byte order.
class SceneDescription : public IDescription {
 public:
  explicit SceneDescription(const scene::SceneDocument& document);
  void describe(emit::ISink& out_sink) const override;

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

/// A check's report as one record of its findings and counts, the one shape every caller reads (ds check --json,
/// the api, and the browser's answer, which carries it as one of its fields).
class ReportDescription : public IDescription {
 public:
  explicit ReportDescription(const check::Report& report);
  void describe(emit::ISink& out_sink) const override;

 private:
  /// Findings or coverage: code, location, message and the severity's name in the report's table.
  void findings(const std::vector<model::Finding>& findings, emit::ISink& out_sink) const;

  const check::Report& report_;
};

/// A query's answers as a list of records of the same three fields: kind, identity, detail (ds query --json).
class AnswersDescription : public IDescription {
 public:
  explicit AnswersDescription(const std::vector<query::Answer>& answers);
  void describe(emit::ISink& out_sink) const override;

 private:
  const std::vector<query::Answer>& answers_;
};

/// Where each node and route of a diagram goes, in cells: the portable form for hosts that draw themselves
/// (-f layout), measured as the theme measures text in a terminal (DS-393).
class LayoutDescription : public IDescription {
 public:
  /// `width` wraps the drawing, in columns; 0 lets it be.
  LayoutDescription(const render::IrText& ir, const theme::Theme& theme, int width);
  void describe(emit::ISink& out_sink) const override;

 private:
  /// The placed rectangles and routes of one drawing, in cells, as one record; of a drawing made of parts, those
  /// of each part; `part`, when given, is the name of the part it is.
  void placed(const render::IrDocument& document, const std::string* part, emit::ISink& out_sink) const;

  const render::IrText& ir_;
  const render::Metrics metrics_;
  const int width_;
};

/// A diagram as the terminal's coloured cells (-f cells), drawn in the theme (DS-393).
class CellsDescription : public IDescription {
 public:
  /// `width` wraps the drawing, in columns; 0 lets it be.
  CellsDescription(const render::IrText& ir, const theme::Theme& theme, int width);
  void describe(emit::ISink& out_sink) const override;

 private:
  const render::IrText& ir_;
  const render::Text text_;
  const int width_;
};

}  // namespace format
}  // namespace ds

#endif  // DRIFTSPEC_FORMAT_DESCRIPTIONS_HPP
