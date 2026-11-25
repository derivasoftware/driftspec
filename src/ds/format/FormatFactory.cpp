#include "ds/format/FormatFactory.hpp"

#include "ds/format/DiagramFormats.hpp"
#include "ds/format/ViewerPage.hpp"

namespace ds {
namespace format {

std::map<std::string, std::unique_ptr<IFormat<scene::SceneDocument>>> FormatFactory::scene() {
  std::map<std::string, std::unique_ptr<IFormat<scene::SceneDocument>>> out;
  out["html"] = std::make_unique<ViewerPage>();
  return out;
}

std::map<std::string, std::unique_ptr<IFormat<render::IrDocument>>> FormatFactory::diagram(const theme::Theme& theme) {
  std::map<std::string, std::unique_ptr<IFormat<render::IrDocument>>> out;
  out["svg"] = std::make_unique<DiagramSvg>(theme);
  out["text"] = std::make_unique<DiagramText>(theme);
  return out;
}

}  // namespace format
}  // namespace ds
