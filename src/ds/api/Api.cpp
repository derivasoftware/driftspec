#include "ds/api/Api.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Report.hpp"
#include "ds/check/Steps.hpp"
#include "ds/format/TextFormats.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace ds {
namespace api {

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
  format::FormatOptions options;  // the one report of a check (DS-120), as JSON (DS-126)
  options.pretty = pretty;
  return format::ReportJson().write(check::Report::of(model), options);
}

// the text functions of the command line's contract: a workspace loaded for one question (DS-173)
std::string Api::check(const Files& files) { return Workspace(files).report(); }

std::string Api::exportModel(const Files& files) { return Workspace(files).exportModel(); }

std::string Api::render(const Files& files, const std::string& diagram, const std::string& format, int width) {
  // svg, the IR itself, or text for anything else
  return Workspace(files).render(diagram, format == "svg" || format == "ir" ? format : "text", width);
}

std::string Api::page(const Files& files, const std::string& title, const std::vector<std::string>& focus) {
  return Workspace(files).page(title, focus);
}

std::string Api::expectedAt(const std::string& text, int line, int column) {
  parse::Parser parser;
  return nlohmann::json(parser.expectedAt(text, line, column)).dump();
}

}  // namespace api
}  // namespace ds
