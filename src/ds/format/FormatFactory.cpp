#include "ds/format/FormatFactory.hpp"

#include "ds/format/DiagramFormats.hpp"
#include "ds/format/TextFormats.hpp"
#include "ds/format/ViewerPage.hpp"

namespace ds {
namespace format {

std::map<std::string, std::unique_ptr<IFormat<scene::SceneDocument>>> FormatFactory::scene() {
  std::map<std::string, std::unique_ptr<IFormat<scene::SceneDocument>>> out;
  out["html"] = std::make_unique<ViewerPage>();
  return out;
}

std::map<std::string, std::unique_ptr<IFormat<render::IrText>>> FormatFactory::diagram(const theme::Theme& theme) {
  std::map<std::string, std::unique_ptr<IFormat<render::IrText>>> out;
  out["ir"] = std::make_unique<DiagramIr>();
  out["svg"] = std::make_unique<DiagramSvg>(theme);
  out["text"] = std::make_unique<DiagramText>(theme);
  return out;
}

std::map<std::string, std::unique_ptr<IFormat<std::vector<query::Answer>>>> FormatFactory::answers() {
  std::map<std::string, std::unique_ptr<IFormat<std::vector<query::Answer>>>> out;
  out["text"] = std::make_unique<AnswersText>();
  return out;
}

std::map<std::string, std::unique_ptr<IFormat<check::Report>>> FormatFactory::report() {
  std::map<std::string, std::unique_ptr<IFormat<check::Report>>> out;
  out["text"] = std::make_unique<ReportText>();
  return out;
}

}  // namespace format
}  // namespace ds
