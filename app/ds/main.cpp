// ds — the DriftSpec command line: `ds check`, `ds export` and `ds render <workspace> [diagram]`.
#include <algorithm>
#include <iostream>
#include <string>

#include "driftspec/bind/Binder.hpp"
#include "driftspec/check/Checker.hpp"
#include "driftspec/export/JsonExport.hpp"
#include "driftspec/model/Model.hpp"
#include "driftspec/parse/Parser.hpp"
#include "driftspec/render/IrExport.hpp"
#include "driftspec/render/Svg.hpp"
#include "driftspec/render/Text.hpp"

#include <unistd.h>

#include <fstream>
#include <sstream>

namespace {

int usage() {
  std::cerr << "usage: ds check <workspace-dir>\n       ds export <workspace-dir>\n       ds render <workspace-dir> [diagram] [--svg|--text]   (render-IR JSON, or the SVG / terminal drawing by ds itself)\n       ds draw <render-ir.json>                      (SVG from any render-IR document)\n";
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
  if (argc < 3 || argc > 5) return usage();
  const std::string verb = argv[1];
  const std::string dir = argv[2];
  if (verb == "draw") {
    std::ifstream in(dir);
    if (!in) {
      std::cerr << "cannot read " << dir << "\n";
      return 1;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::cout << driftspec::render::Svg().render(driftspec::render::IrDocument::parse(buffer.str()));
    return 0;
  }
  const driftspec::model::Model model = load(dir);
  if (verb == "render") {
    driftspec::render::IrExport ir(model);
    if (argc == 3) {
      for (const auto& d : ir.diagrams()) std::cout << d.substr(std::string("diagram:").size()) << "\n";
      return 0;
    }
    const std::string name = argv[3];
    const bool svg = argc == 5 && std::string(argv[4]) == "--svg";
    const bool text = argc == 5 && std::string(argv[4]) == "--text";
    const std::string doc = ir.render(name.rfind("diagram:", 0) == 0 ? name : "diagram:" + name, !svg && !text);
    if (doc.empty()) {
      std::cerr << "no diagram named " << name << "\n";
      return 1;
    }
    if (svg) {
      std::cout << driftspec::render::Svg().render(driftspec::render::IrDocument::parse(doc));
      return 0;
    }
    if (text) {
      std::cout << driftspec::render::Text().render(driftspec::render::IrDocument::parse(doc), isatty(1) != 0);
      return 0;
    }
    std::cout << doc << "\n";
    return 0;
  }
  if (argc != 3) return usage();
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
