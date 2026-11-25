#ifndef DRIFTSPEC_FORMAT_DESCRIPTIONS_HPP
#define DRIFTSPEC_FORMAT_DESCRIPTIONS_HPP

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
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
class SceneDescription : public ADescription {
 public:
  explicit SceneDescription(const scene::SceneDocument& document);
  std::string name() const override;

 private:
  void say(emit::ISink& out_sink) const override;
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

/// A check's report, said once in both halves (DS-399). As structure, one record of its findings and counts, the
/// one shape every program reads (ds check --json, the api, and the browser's answer, which carries it as one of
/// its fields). As presentation, what ds check prints: a table of the findings (where, the code, the message),
/// then the counts as a paragraph and, when coverage is open, where it is listed as another.
class ReportDescription : public ADescription {
 public:
  /// `workspace` is the workspace as it was given, named where the open coverage is listed; "<workspace>" when empty.
  ReportDescription(const check::Report& report, const std::string& workspace);
  std::string name() const override;

 private:
  void say(emit::ISink& out_sink) const override;
  /// Findings or coverage: code, location, message and the severity's name in the report's table.
  void findings(const std::vector<model::Finding>& findings, emit::ISink& out_sink) const;
  /// Where a finding is, as a reader finds it: file:line:column.
  static std::string where(const model::Finding& finding);
  /// The counts of the model as a sentence: one is said in the singular (1 finding, DS-435), and the open coverage
  /// ends it (DS-431).
  std::string counts() const;
  /// Where the open coverage is listed: the query that lists it, over the workspace (DS-431).
  std::string listing() const;
  /// A count and what it counts, in the singular for one.
  static std::string count(std::size_t n, std::string_view one, std::string_view many);

  const check::Report& report_;
  const std::string workspace_;  ///< the workspace as it was given; empty when the door gives none
};

/// A query's answers, said once in both halves (DS-399): as structure, a list of records of the same three fields,
/// kind, identity and detail (ds query --json), and the ends of a relation when it has them (DS-445); as
/// presentation, what ds query prints, a table of the kind, the identity and the detail when there is one.
class AnswersDescription : public ADescription {
 public:
  explicit AnswersDescription(const std::vector<query::Answer>& answers);
  std::string name() const override;

 private:
  void say(emit::ISink& out_sink) const override;

  const std::vector<query::Answer>& answers_;
};

/// Where each node and route of a diagram goes, in cells: the portable form for hosts that draw themselves
/// (-f layout), measured as the theme measures text in a terminal (DS-393).
class LayoutDescription : public ADescription {
 public:
  /// `width` wraps the drawing, in columns; 0 lets it be.
  LayoutDescription(const render::IrText& ir, const theme::Theme& theme, int width);
  std::string name() const override;

 private:
  void say(emit::ISink& out_sink) const override;
  /// The placed rectangles and routes of one drawing, in cells, as one record; of a drawing made of parts, those
  /// of each part; `part`, when given, is the name of the part it is.
  void placed(const render::IrDocument& document, const std::string* part, emit::ISink& out_sink) const;

  const render::IrText& ir_;
  const render::Metrics metrics_;
  const int width_;
};

/// A diagram as the terminal's coloured cells (-f cells), drawn in the theme (DS-393).
class CellsDescription : public ADescription {
 public:
  /// `width` wraps the drawing, in columns; 0 lets it be.
  CellsDescription(const render::IrText& ir, const theme::Theme& theme, int width);
  std::string name() const override;

 private:
  void say(emit::ISink& out_sink) const override;
  const render::IrText& ir_;
  const render::Text text_;
  const int width_;
};

}  // namespace format
}  // namespace ds

#endif  // DRIFTSPEC_FORMAT_DESCRIPTIONS_HPP
