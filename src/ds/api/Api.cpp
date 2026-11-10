#include "ds/api/Api.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Steps.hpp"
#include "ds/export/JsonExport.hpp"
#include "ds/format/SceneFormats.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/IrExport.hpp"
#include "ds/render/Svg.hpp"
#include "ds/render/Text.hpp"
#include "ds/scene/Scene.hpp"

namespace ds {
namespace api {

namespace {

// the texts parsed, then loaded as every caller loads (Api::load)
model::Model loadTexts(const Files& files) {
  parse::Parser parser;
  std::vector<parse::ParsedFile> parsed;
  for (const auto& file : files) parsed.push_back(parser.parseText(file.first, file.second));
  return Api::load(parsed);
}

}  // namespace

model::Model Api::load(const std::vector<parse::ParsedFile>& files) {
  model::Model model;
  bind::Binder binder(model);
  for (const auto& parsed : files) {
    if (parsed.hasSyntaxErrors) {
      const std::string detail = parsed.errorDetail.empty() ? "" : ": " + parsed.errorDetail;
      model.report({"E-SYNTAX", "error", "the file does not parse" + detail, {parsed.path, parsed.errorLine, parsed.errorColumn}});
    }
    binder.bind(parsed);
  }
  const auto steps = check::steps();
  check::Checker(model, steps).run(binder);
  auto& findings = model.findings();
  std::stable_sort(findings.begin(), findings.end(), [](const model::Finding& a, const model::Finding& b) {
    return a.location.file != b.location.file ? a.location.file < b.location.file : a.location.line < b.location.line;
  });
  return model;
}

std::string Api::report(const model::Model& model, bool pretty) {
  nlohmann::json findings = nlohmann::json::array();
  int errors = 0;
  for (const auto& f : model.findings()) {
    errors += f.severity == "error" ? 1 : 0;
    findings.push_back({{"code", f.code},
                        {"severity", f.severity},
                        {"message", f.message},
                        {"location", {{"file", f.location.file}, {"line", f.location.line}, {"column", f.location.column}}}});
  }
  long expected = 0;
  for (const auto& e : model.entities()) expected += e.second.expected ? 1 : 0;
  const nlohmann::json out{{"findings", findings},
                           {"entities", static_cast<long>(model.entities().size()) - expected},  // declared: what the design says
                           {"expected", expected},
                           {"relations", model.relations().size()},
                           {"errors", errors}};
  return pretty ? out.dump(2) : out.dump();
}

std::string Api::check(const Files& files) { return report(loadTexts(files)); }

std::string Api::exportModel(const Files& files) { return exporter::JsonExport().render(loadTexts(files), false); }

std::string Api::render(const Files& files, const std::string& diagram, const std::string& format, int width) {
  const model::Model model = loadTexts(files);
  const std::string id = model::diagramId(diagram);
  const std::string ir = render::IrExport(model).render(id, false);
  if (ir.empty() || format == "ir") return ir;
  const render::IrDocument doc = render::IrDocument::parse(ir);
  return format == "svg" ? render::Svg().render(doc, width) : render::Text().render(doc, false, width);
}

std::string Api::page(const Files& files, const std::string& title, const std::vector<std::string>& focus) {
  const model::Model model = loadTexts(files);
  format::FormatOptions options;  // the viewer page: the scene written by its html format (DS-126)
  options.title = title;
  options.focus = focus;
  return format::SceneHtml().write(scene::Scene(model).build(), options);
}

std::string Api::expectedAt(const std::string& text, int line, int column) {
  parse::Parser parser;
  return nlohmann::json(parser.expectedAt(text, line, column)).dump();
}

}  // namespace api
}  // namespace ds
