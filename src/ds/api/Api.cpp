#include "ds/api/Api.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/export/JsonExport.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/render/IrExport.hpp"
#include "ds/render/Svg.hpp"
#include "ds/render/Text.hpp"
#include "ds/scene/Scene.hpp"

namespace ds {
namespace api {

namespace {

// the workspace loaded as the command line loads a directory: parsed, bound, checked, findings in order
model::Model load(const Files& files) {
  model::Model model;
  parse::Parser parser;
  bind::Binder binder(model);
  for (const auto& file : files) {
    const parse::ParsedFile parsed = parser.parseText(file.first, file.second);
    if (parsed.hasSyntaxErrors) {
      const std::string detail = parsed.errorDetail.empty() ? "" : ": " + parsed.errorDetail;
      model.report({"E-SYNTAX", "error", "the file does not parse" + detail, {parsed.path, parsed.errorLine, parsed.errorColumn}});
    }
    binder.bind(parsed);
  }
  check::Checker(model).run(binder);
  auto& findings = model.findings();
  std::stable_sort(findings.begin(), findings.end(), [](const model::Finding& a, const model::Finding& b) {
    return a.location.file != b.location.file ? a.location.file < b.location.file : a.location.line < b.location.line;
  });
  return model;
}

}  // namespace

std::string Api::check(const Files& files) {
  const model::Model model = load(files);
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
  return nlohmann::json{{"findings", findings},
                        {"counts",
                         {{"entities", model.entities().size()},
                          {"expected", expected},
                          {"relations", model.relations().size()},
                          {"findings", model.findings().size()},
                          {"errors", errors}}}}
      .dump();
}

std::string Api::exportModel(const Files& files) { return exporter::JsonExport().render(load(files), false); }

std::string Api::render(const Files& files, const std::string& diagram, const std::string& format, int width) {
  const model::Model model = load(files);
  const std::string id = diagram.rfind("diagram:", 0) == 0 ? diagram : "diagram:" + diagram;
  const std::string ir = render::IrExport(model).render(id, false);
  if (ir.empty() || format == "ir") return ir;
  const render::IrDocument doc = render::IrDocument::parse(ir);
  return format == "svg" ? render::Svg().render(doc, width) : render::Text().render(doc, false, width);
}

std::string Api::page(const Files& files, const std::string& title, const std::vector<std::string>& focus) {
  const model::Model model = load(files);
  return scene::Scene(model).page(title, "", focus);
}

std::string Api::expectedAt(const std::string& text, int line, int column) {
  parse::Parser parser;
  return nlohmann::json(parser.expectedAt(text, line, column)).dump();
}

}  // namespace api
}  // namespace ds
