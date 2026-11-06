// ds — the DriftSpec command line: check, export, render, draw, coupling, diff and query over a workspace.
#include <sys/stat.h>
#include <unistd.h>

#include <CLI/CLI.hpp>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/diff/Diff.hpp"
#include "ds/export/JsonExport.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Query.hpp"
#include "ds/render/IrExport.hpp"
#include "ds/render/Layout.hpp"
#include "ds/render/Svg.hpp"
#include "ds/render/Text.hpp"
#include "ds/scene/Scene.hpp"

namespace {

bool isDirectory(const std::string& path) {
  struct stat st;
  return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string firstLine(const std::string& command) {
  std::string out;
  if (FILE* p = ::popen(command.c_str(), "r")) {
    char buffer[4096];
    if (std::fgets(buffer, sizeof buffer, p)) out = buffer;
    ::pclose(p);
  }
  while (!out.empty() && (out.back() == '\n' || out.back() == '/')) out.pop_back();
  return out;
}

/// The .ds files of `path` (relative to the current directory, or absolute inside a
/// repository) at git revision `rev`, extracted to a fresh directory ("" on failure).
std::string checkout(const std::string& rev, const std::string& path) {
  char tmpl[] = "/tmp/ds-diff-XXXXXX";
  const char* dir = ::mkdtemp(tmpl);
  if (!dir) return "";
  std::string root = ".", relative = path;
  if (!path.empty() && path[0] == '/') {
    root = firstLine("git -C '" + path + "' rev-parse --show-toplevel 2>/dev/null");
    relative = firstLine("git -C '" + path + "' rev-parse --show-prefix 2>/dev/null");
    if (root.empty()) return "";
    if (relative.empty()) relative = ".";
  }
  const std::string command =
      "git -C '" + root + "' archive '" + rev + "' -- '" + relative + "' 2>/dev/null | tar -x -C '" + std::string(dir) + "' 2>/dev/null";
  if (std::system(command.c_str()) != 0) return "";
  return std::string(dir) + "/" + relative;
}

ds::model::Model load(const std::string& dir) {
  ds::model::Model model;
  ds::parse::Parser parser;
  ds::bind::Binder binder(model);
  for (const auto& file : parser.parseDirectory(dir)) {
    if (file.hasSyntaxErrors) {
      const std::string detail = file.errorDetail.empty() ? "" : ": " + file.errorDetail;
      model.report({"E-SYNTAX", "error", "the file does not parse" + detail, {file.path, file.errorLine, file.errorColumn}});
    }
    binder.bind(file);
  }
  ds::check::Checker(model).run(binder);
  auto& findings = model.findings();
  std::stable_sort(findings.begin(), findings.end(), [](const auto& a, const auto& b) {
    return a.location.file != b.location.file ? a.location.file < b.location.file : a.location.line < b.location.line;
  });
  return model;
}

/// The workspace loaded, or nothing when the path is not a directory (reported on stderr).
bool loadWorkspace(const std::string& dir, ds::model::Model* model) {
  if (!isDirectory(dir)) {
    std::cerr << "cannot read " << dir << ": not a directory\n";
    return false;
  }
  *model = load(dir);
  return true;
}

std::string titleOf(std::string dir) {
  while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
  const auto slash = dir.find_last_of('/');
  return slash == std::string::npos ? dir : dir.substr(slash + 1);
}

int check(const std::string& dir) {
  ds::model::Model model;
  if (!loadWorkspace(dir, &model)) return 1;
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

int coupling(const ds::model::Model& model) {
  // derived from behaviour: a local or an attribute typed in another module is coupling to it
  std::map<std::string, std::vector<std::string>> byBehaviour;
  for (const auto& r : model.relations()) {
    if (r.kind != "couples-structural" && r.kind != "couples-transient") continue;
    byBehaviour[r.from].push_back(
        r.to + (r.kind == "couples-transient" ? "  (transient: created inside)" : "  (structural: an attribute or parameter)"));
  }
  if (byBehaviour.empty()) std::cout << "no behaviour couples to another module\n";
  for (const auto& b : byBehaviour) {
    const ds::model::Entity* seq = model.find(b.first);
    std::cout << (seq ? seq->name : b.first) << "  (module " << (seq ? seq->scope : "?") << ")\n";
    for (const auto& t : b.second) std::cout << "  -> " << t << "\n";
  }
  return 0;
}

/// A drawing of a render-IR document: svg, text for a terminal, or cells.
void drawDocument(const ds::render::IrDocument& doc, const std::string& how, int width) {
  if (how == "text")
    std::cout << ds::render::Text().render(doc, isatty(1) != 0, width);
  else if (how == "cells")
    std::cout << ds::render::Text().cells(doc, width) << "\n";
  else
    std::cout << ds::render::Svg().render(doc, width);
}

void printLayout(const std::string& doc, int width) {
  // the placed rectangles and routes, in cells: the portable form for hosts that draw themselves
  const ds::render::IrDocument parsed = ds::render::IrDocument::parse(doc).bundled();
  const ds::render::LayoutResult lay = ds::render::layout(parsed, ds::render::Metrics::text(), width);
  std::cout << "{\"nodes\":[";
  bool first = true;
  for (const auto& p : lay.placed) {
    std::cout << (first ? "" : ",") << "{\"id\":\"" << p.node->id << "\",\"kind\":\"" << p.node->kind << "\",\"x\":" << p.rect.x
              << ",\"y\":" << p.rect.y << ",\"w\":" << p.rect.w << ",\"h\":" << p.rect.h << "}";
    first = false;
  }
  std::cout << "],\"routes\":[";
  first = true;
  for (const auto& r : lay.routes) {
    const ds::render::IrEdge& e = parsed.edges[r.first];
    std::cout << (first ? "" : ",") << "{\"from\":\"" << e.from << "\",\"to\":\"" << e.to << "\",\"kind\":\"" << e.kind
              << "\",\"points\":[";
    for (size_t k = 0; k < r.second.points.size(); ++k)
      std::cout << (k ? "," : "") << "[" << r.second.points[k].x << "," << r.second.points[k].y << "]";
    std::cout << "]}";
    first = false;
  }
  std::cout << "]}\n";
}

std::string diagramId(const std::string& name) { return name.rfind("diagram:", 0) == 0 ? name : "diagram:" + name; }

/// Adds the mutually exclusive drawing flags of a command; the one given is left in `how`.
void drawingFlags(CLI::App* cmd, std::string* how, bool withLayout) {
  auto* svg = cmd->add_flag_callback("--svg", [how] { *how = "svg"; }, "draw it as SVG");
  auto* text = cmd->add_flag_callback("--text", [how] { *how = "text"; }, "draw it for a terminal, in box-drawing text");
  auto* cells = cmd->add_flag_callback("--cells", [how] { *how = "cells"; }, "the drawing as a grid of cells, for tests and hosts");
  svg->excludes(text)->excludes(cells);
  text->excludes(cells);
  if (withLayout) {
    auto* layout = cmd->add_flag_callback("--layout", [how] { *how = "layout"; }, "only the placed rectangles and routes, as JSON");
    layout->excludes(svg)->excludes(text)->excludes(cells);
  }
}

}  // namespace

int main(int argc, char** argv) {
  CLI::App app{"ds: a software design as text, checked, drawn, compared and asked", "ds"};
  app.require_subcommand(1);
  app.footer("Each command takes --help. A workspace is a directory of .ds files read as one.");

  std::string dir, diagram, document, before, after, path = ".", question, subject, css, how;
  int width = 0, height = 640;
  bool json = false, embed = false;
  const auto widthOption = [&width](CLI::App* cmd) {
    cmd->add_option("--width", width, "wrap the drawing: columns for text and cells, pixels for SVG")->check(CLI::PositiveNumber);
  };

  auto* checkCmd = app.add_subcommand("check", "check a workspace: one line per finding, then the counts of the model");
  checkCmd->add_option("workspace", dir, "a directory of .ds files")->required();

  auto* exportCmd = app.add_subcommand("export", "the model as JSON: entities, relations, findings and the metamodel");
  exportCmd->add_option("workspace", dir, "a directory of .ds files")->required();

  auto* renderCmd = app.add_subcommand("render", "a diagram of the workspace: render-IR JSON, or drawn; with no diagram, the list");
  renderCmd->add_option("workspace", dir, "a directory of .ds files")->required();
  renderCmd->add_option("diagram", diagram, "the diagram to draw");
  drawingFlags(renderCmd, &how, true);
  widthOption(renderCmd);

  auto* drawCmd = app.add_subcommand("draw", "draw any render-IR document like a workspace diagram (SVG unless asked)");
  drawCmd->add_option("document", document, "a render-IR JSON file")->required();
  drawingFlags(drawCmd, &how, false);
  widthOption(drawCmd);

  auto* couplingCmd = app.add_subcommand("coupling", "what every behaviour touches outside its module");
  couplingCmd->add_option("workspace", dir, "a directory of .ds files")->required();

  auto* diffCmd = app.add_subcommand("diff", "the changes between two versions in the terms of the model, or a diagram with them marked");
  diffCmd->add_option("before", before, "a directory, or a git revision")->required();
  diffCmd->add_option("after", after, "a directory, or a git revision")->required();
  diffCmd->add_option("path", path, "with revisions: the workspace inside the repository");
  // a diagram with the changes marked: the format flag names the diagram, as in ds diff A B path --svg CL_X
  auto* diffSvg = diffCmd->add_option_function<std::string>(
      "--svg", [&](const std::string& d) { how = "svg", diagram = d; }, "draw this diagram as SVG with the changes marked");
  auto* diffText = diffCmd->add_option_function<std::string>(
      "--text", [&](const std::string& d) { how = "text", diagram = d; }, "draw this diagram for a terminal with the changes marked");
  auto* diffCells = diffCmd->add_option_function<std::string>(
      "--cells", [&](const std::string& d) { how = "cells", diagram = d; }, "this diagram as cells with the changes marked");
  diffSvg->excludes(diffText)->excludes(diffCells);
  diffText->excludes(diffCells);
  widthOption(diffCmd);

  auto* queryCmd =
      app.add_subcommand("query", "ask the model by identity: diagrams · entities · relations · artefacts · behaviours · shows · findings");
  queryCmd->add_option("workspace", dir, "a directory of .ds files")->required();
  queryCmd->add_option("question", question, "what to ask")->required();
  queryCmd->add_option("subject", subject, "what it is asked about");
  queryCmd->add_flag("--json", json, "answer as JSON");

  auto* sceneCmd = app.add_subcommand("scene", "the whole model laid out once, nested, as JSON for a viewer");
  sceneCmd->add_option("workspace", dir, "a directory of .ds files")->required();

  auto* viewCmd = app.add_subcommand("view", "the viewer: one page with the whole model inside, to open in a browser");
  viewCmd->add_option("workspace", dir, "a directory of .ds files")->required();
  viewCmd->add_flag("--embed", embed, "an <iframe> carrying the page, to paste into any HTML document");
  viewCmd->add_option("--height", height, "the height of the embedded frame, in pixels")->check(CLI::PositiveNumber)->capture_default_str();
  viewCmd->add_option("--css", css, "a stylesheet applied over the viewer's; it may redefine its colour and font variables")
      ->check(CLI::ExistingFile);

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    const int code = app.exit(e);
    return code == 0 ? 0 : 2;  // help is not a failure; a wrong command line is bad usage, as before
  }

  if (drawCmd->parsed()) {
    std::ifstream in(document);
    if (!in) {
      std::cerr << "cannot read " << document << "\n";
      return 1;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    // any render-IR producer (design-render over argos, plantuml-render) reaches the same drawings as a workspace
    drawDocument(ds::render::IrDocument::parse(buffer.str()), how.empty() ? "svg" : how, width);
    return 0;
  }
  if (diffCmd->parsed()) {
    std::string a = before, b = after;
    if (!isDirectory(a) || !isDirectory(b)) {
      a = checkout(a, path);
      b = checkout(b, path);
      if (a.empty() || b.empty()) {
        std::cerr << "cannot read both revisions (give two directories, or two git revisions and a path)\n";
        return 1;
      }
    }
    const ds::model::Model first = load(a);
    const ds::model::Model second = load(b);
    const ds::diff::DiffResult result = ds::diff::Diff::compare(first, second);
    if (!diagram.empty()) {
      const ds::model::Model marked = ds::diff::Diff::marked(first, second, result);
      const std::string doc = ds::render::IrExport(marked).render(diagramId(diagram), false);
      if (doc.empty()) {
        std::cerr << "no diagram named " << diagram << "\n";
        return 1;
      }
      drawDocument(ds::render::IrDocument::parse(doc), how.empty() ? "text" : how, width);
      return 0;
    }
    std::cout << ds::diff::Diff::report(result);
    return result.findings.empty() ? 0 : 1;
  }
  if (checkCmd->parsed()) return check(dir);

  ds::model::Model model;
  if (!loadWorkspace(dir, &model)) return 1;
  if (exportCmd->parsed()) {
    std::cout << ds::exporter::JsonExport().render(model) << "\n";
    return 0;
  }
  if (couplingCmd->parsed()) return coupling(model);
  if (sceneCmd->parsed()) {
    std::cout << ds::scene::Scene(model).render() << "\n";
    return 0;
  }
  if (viewCmd->parsed()) {
    std::string style;
    if (!css.empty()) {
      std::ifstream in(css);
      std::stringstream buffer;
      buffer << in.rdbuf();
      style = buffer.str();
    }
    const std::string title = titleOf(dir);
    const std::string page = ds::scene::Scene(model).page(title, style);
    std::cout << (embed ? ds::scene::Scene::frame(page, title, height) : page);
    return 0;
  }
  if (queryCmd->parsed()) {
    const ds::query::Query query(model);
    const std::vector<ds::query::Answer> answers = query.ask(question, subject);
    if (!answers.empty() && answers.front().kind == "error") {
      std::cerr << answers.front().id << ": " << answers.front().detail << "\n";
      if (answers.front().detail.find("not a question") != std::string::npos) {
        std::cerr << "questions:";
        for (const auto& q : ds::query::Query::questions()) std::cerr << " " << q.first << (q.second.empty() ? "" : " " + q.second) << " ·";
        std::cerr << "\n";
      }
      return 1;
    }
    std::cout << (json ? ds::query::Query::json(answers) + "\n" : ds::query::Query::text(answers));
    return 0;
  }
  // render
  ds::render::IrExport ir(model);
  if (diagram.empty()) {
    for (const auto& d : ir.diagrams()) std::cout << d.substr(std::string("diagram:").size()) << "\n";
    return 0;
  }
  const std::string doc = ir.render(diagramId(diagram), how.empty());
  if (doc.empty()) {
    std::cerr << "no diagram named " << diagram << "\n";
    return 1;
  }
  if (how == "layout")
    printLayout(doc, width);
  else if (how.empty())
    std::cout << doc << "\n";
  else
    drawDocument(ds::render::IrDocument::parse(doc), how, width);
  return 0;
}
