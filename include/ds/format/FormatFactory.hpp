#ifndef DRIFTSPEC_FORMAT_FORMATFACTORY_HPP
#define DRIFTSPEC_FORMAT_FORMATFACTORY_HPP

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ds/check/Report.hpp"
#include "ds/format/Format.hpp"
#include "ds/query/Query.hpp"
#include "ds/render/Ir.hpp"
#include "ds/scene/Document.hpp"
#include "ds/theme/Theme.hpp"

namespace ds {
namespace format {

/// The one place that names the writers that are no description yet (DS-126, DS-398), keyed by name: the viewer's
/// page, a diagram's IR and its drawings in svg and text, the answers and the report as text. Each goes when its
/// document becomes a description and its technology a sink (DS-399, DS-400, DS-418).
class FormatFactory {
 public:
  /// html: the viewer as one page, the scene inside.
  static std::map<std::string, std::unique_ptr<IFormat<scene::SceneDocument>>> scene();
  /// ir, svg and text; the drawings in `theme` (DS-393).
  static std::map<std::string, std::unique_ptr<IFormat<render::IrText>>> diagram(const theme::Theme& theme);
  /// text: one answer per line.
  static std::map<std::string, std::unique_ptr<IFormat<std::vector<query::Answer>>>> answers();
  /// text: each finding where it is, then the counts.
  static std::map<std::string, std::unique_ptr<IFormat<check::Report>>> report();
};

}  // namespace format
}  // namespace ds

#endif  // DRIFTSPEC_FORMAT_FORMATFACTORY_HPP
