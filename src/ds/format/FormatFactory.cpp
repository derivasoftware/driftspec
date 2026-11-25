#include "ds/format/FormatFactory.hpp"

#include "ds/format/ViewerPage.hpp"

namespace ds {
namespace format {

std::map<std::string, std::unique_ptr<IFormat<scene::SceneDocument>>> FormatFactory::scene() {
  std::map<std::string, std::unique_ptr<IFormat<scene::SceneDocument>>> out;
  out["html"] = std::make_unique<ViewerPage>();
  return out;
}

}  // namespace format
}  // namespace ds
