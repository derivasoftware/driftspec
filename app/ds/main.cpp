// ds — the DriftSpec command line: `ds check <workspace>` and `ds export <workspace>`.
#include <algorithm>
#include <iostream>
#include <string>

#include "driftspec/bind/Binder.hpp"
#include "driftspec/check/Checker.hpp"
#include "driftspec/export/JsonExport.hpp"
#include "driftspec/model/Model.hpp"
#include "driftspec/parse/Parser.hpp"

namespace {

int usage() {
  std::cerr << "usage: ds check <workspace-dir>\n       ds export <workspace-dir>\n";
  return 2;
}

driftspec::model::Model load(const std::string& dir) {
  driftspec::model::Model model;
  driftspec::parse::Parser parser;
  driftspec::bind::Binder binder(model);
  for (const auto& file : parser.parseDirectory(dir)) {
    if (file.hasSyntaxErrors) model.report({"E-SYNTAX", "error", "the file does not parse", {file.path, 1, 1}});
    binder.bind(file);
  }
  driftspec::check::Checker(model).run(binder);
  auto& findings = model.findings();
  std::stable_sort(findings.begin(), findings.end(), [](const auto& a, const auto& b) {
    return a.location.file != b.location.file ? a.location.file < b.location.file : a.location.line < b.location.line;
  });
  return model;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) return usage();
  const std::string verb = argv[1];
  const std::string dir = argv[2];
  const driftspec::model::Model model = load(dir);
  if (verb == "export") {
    std::cout << driftspec::exporter::JsonExport().render(model) << "\n";
    return 0;
  }
  if (verb != "check") return usage();
  int errors = 0;
  for (const auto& f : model.findings()) {
    std::cout << f.location.file << ":" << f.location.line << ":" << f.location.column << "  " << f.code << "  " << f.message << "\n";
    if (f.severity == "error") ++errors;
  }
  const auto& entities = model.entities();
  const long expected = std::count_if(entities.begin(), entities.end(), [](const auto& p) { return p.second.expected; });
  std::cout << entities.size() - expected << " entities · " << expected << " expected · " << model.relations().size() << " relations · "
            << model.findings().size() << " findings\n";
  return errors ? 1 : 0;
}
