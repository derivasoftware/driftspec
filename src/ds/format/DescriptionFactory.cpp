#include "ds/format/DescriptionFactory.hpp"

#include "ds/format/Descriptions.hpp"

namespace ds {
namespace format {

std::unique_ptr<ADescription> DescriptionFactory::scene(const scene::SceneDocument& document) {
  return std::make_unique<SceneDescription>(document);
}

std::unique_ptr<ADescription> DescriptionFactory::report(const check::Report& report, const std::string& workspace) {
  return std::make_unique<ReportDescription>(report, workspace);
}

std::unique_ptr<ADescription> DescriptionFactory::answers(const std::vector<query::Answer>& answers) {
  return std::make_unique<AnswersDescription>(answers);
}

std::unique_ptr<ADescription> DescriptionFactory::ir(const render::IrDocument& document) {
  return std::make_unique<IrDescription>(document);
}

std::unique_ptr<ADescription> DescriptionFactory::layout(const render::IrText& ir, const theme::Theme& theme, int width) {
  return std::make_unique<LayoutDescription>(ir, theme, width);
}

std::unique_ptr<ADescription> DescriptionFactory::cells(const render::IrText& ir, const theme::Theme& theme, int width) {
  return std::make_unique<CellsDescription>(ir, theme, width);
}

}  // namespace format
}  // namespace ds
