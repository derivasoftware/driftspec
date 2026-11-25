#ifndef DRIFTSPEC_FORMAT_DESCRIPTIONFACTORY_HPP
#define DRIFTSPEC_FORMAT_DESCRIPTIONFACTORY_HPP

#include <memory>
#include <string>
#include <vector>

#include "ds/check/Report.hpp"
#include "ds/format/Description.hpp"
#include "ds/query/Query.hpp"
#include "ds/render/Ir.hpp"
#include "ds/scene/Document.hpp"
#include "ds/theme/Theme.hpp"

namespace ds {
namespace format {

/// The document axis (DS-398): the one place that names the descriptions, one maker per document. A door asks it
/// for the description of its document, asks emit::SinkFactory for the sink of its technology, and pairs them:
/// description.describe(sink). A description refers to the document it is given, so the document outlives it.
class DescriptionFactory {
 public:
  /// The scene, for the viewer.
  static std::unique_ptr<ADescription> scene(const scene::SceneDocument& document);
  /// A check's report: its findings, its coverage and its counts; `workspace` is the workspace as it was given,
  /// which the report names where it says its open coverage is listed ("<workspace>" when empty).
  static std::unique_ptr<ADescription> report(const check::Report& report, const std::string& workspace = "");
  /// A query's answers.
  static std::unique_ptr<ADescription> answers(const std::vector<query::Answer>& answers);
  /// A diagram's render-IR (DS-418): the document IrExport made, as plantuml-render's contract writes it.
  static std::unique_ptr<ADescription> ir(const render::IrDocument& document);
  /// Where each node and route of a diagram goes, in cells measured as the theme measures a terminal's text.
  static std::unique_ptr<ADescription> layout(const render::IrText& ir, const theme::Theme& theme, int width);
  /// A diagram as the terminal's coloured cells, drawn in the theme.
  static std::unique_ptr<ADescription> cells(const render::IrText& ir, const theme::Theme& theme, int width);
};

}  // namespace format
}  // namespace ds

#endif  // DRIFTSPEC_FORMAT_DESCRIPTIONFACTORY_HPP
