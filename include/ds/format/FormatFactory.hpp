#ifndef DRIFTSPEC_FORMAT_FORMATFACTORY_HPP
#define DRIFTSPEC_FORMAT_FORMATFACTORY_HPP

#include <map>
#include <memory>
#include <string>

#include "ds/format/Format.hpp"
#include "ds/scene/Document.hpp"

namespace ds {
namespace format {

/// The one place that names the writers that are no description yet (DS-126, DS-398), keyed by name: the viewer's
/// page, which is the scene in JSON inside its template. The IR went when it became a description (DS-418), the
/// drawings in svg and text when see-design drew them (DS-474), the answers and the report as text when the text sink
/// wrote them (DS-399).
class FormatFactory {
 public:
  /// html: the viewer as one page, the scene inside.
  static std::map<std::string, std::unique_ptr<IFormat<scene::SceneDocument>>> scene();
};

}  // namespace format
}  // namespace ds

#endif  // DRIFTSPEC_FORMAT_FORMATFACTORY_HPP
