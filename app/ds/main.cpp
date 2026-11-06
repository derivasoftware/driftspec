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
#include <nlohmann/json.hpp>
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

std::string readFile(const std::string& path) {
  std::ifstream in(path);
  std::stringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

/// Writes to the file -o names, or to stdout.
int emit(const std::string& output, const std::string& text) {
  if (output.empty()) {
    std::cout << text;
    return 0;
  }
  std::ofstream out(output);
  if (!out) {
    std::cerr << "cannot write " << output << "\n";
    return 1;
  }
  out << text;
  return 0;
}

std::string locationJson(const ds::model::Location& l) {
  return nlohmann::json{{"file", l.file}, {"line", l.line}, {"column", l.column}}.dump();
}

int check(const std::string& dir, bool json) {
  ds::model::Model model;
  if (!loadWorkspace(dir, &model)) return 2;  // no workspace to check: the command line is wrong, not the design
  int errors = 0;
  const auto& entities = model.entities();
  const long expected = std::count_if(entities.begin(), entities.end(), [](const auto& p) { return p.second.expected; });
  nlohmann::json findings = nlohmann::json::array();
  for (const auto& f : model.findings()) {
    if (f.severity == "error") ++errors;
    if (json) {
      findings.push_back({{"code", f.code},
                          {"severity", f.severity},
                          {"message", f.message},
                          {"location", nlohmann::json::parse(locationJson(f.location))}});
    } else {
      std::cout << f.location.file << ":" << f.location.line << ":" << f.location.column << "  " << f.code << "  " << f.message << "\n";
    }
  }
  const long declared = static_cast<long>(entities.size()) - expected;
  if (json) {
    std::cout << nlohmann::json{{"findings", findings},
                                {"entities", declared},
                                {"expected", expected},
                                {"relations", model.relations().size()}}
                     .dump(2)
              << "\n";
  } else {
    std::cout << declared << " entities · " << expected << " expected · " << model.relations().size() << " relations · "
              << model.findings().size() << " findings\n";
  }
  return errors ? 1 : 0;
}

/// A static drawing of a render-IR document: svg, text for a terminal, or cells.
std::string drawing(const ds::render::IrDocument& doc, const std::string& format, int width) {
  if (format == "text") return ds::render::Text().render(doc, isatty(1) != 0, width);
  if (format == "cells") return ds::render::Text().cells(doc, width) + "\n";
  return ds::render::Svg().render(doc, width);
}

std::string layoutJson(const std::string& doc, int width) {
  // the placed rectangles and routes, in cells: the portable form for hosts that draw themselves
  const ds::render::IrDocument parsed = ds::render::IrDocument::parse(doc).bundled();
  const ds::render::LayoutResult lay = ds::render::layout(parsed, ds::render::Metrics::text(), width);
  nlohmann::json out = {{"nodes", nlohmann::json::array()}, {"routes", nlohmann::json::array()}};
  for (const auto& p : lay.placed) {
    out["nodes"].push_back(
        {{"id", p.node->id}, {"kind", p.node->kind}, {"x", p.rect.x}, {"y", p.rect.y}, {"w", p.rect.w}, {"h", p.rect.h}});
  }
  for (const auto& r : lay.routes) {
    const ds::render::IrEdge& e = parsed.edges[r.first];
    nlohmann::json points = nlohmann::json::array();
    for (const auto& pt : r.second.points) points.push_back({pt.x, pt.y});
    out["routes"].push_back({{"from", e.from}, {"to", e.to}, {"kind", e.kind}, {"points", points}});
  }
  return out.dump() + "\n";
}

std::string diagramId(const std::string& name) { return name.rfind("diagram:", 0) == 0 ? name : "diagram:" + name; }

/// One way to look at the design: the whole model in the viewer (html, or its data as json), or one
/// diagram drawn still (svg, text, cells) or as its render-IR (ir, layout).
struct Render {
  std::string input, diagram, format = "html", output, css;
  int width = 0, height = 640;
  bool embed = false;

  int run() const {
    if (!isDirectory(input)) {  // a render-IR document someone else produced: drawn like a diagram of a workspace
      std::ifstream probe(input);
      if (!probe) {
        std::cerr << "cannot read " << input << "\n";
        return 1;
      }
      const std::string doc = readFile(input);
      if (format == "html" || format == "json") {
        std::cerr << "a render-IR document draws as svg, text, cells or layout; the viewer needs a workspace\n";
        return 2;
      }
      if (format == "ir") return emit(output, doc);
      if (format == "layout") return emit(output, layoutJson(doc, width));
      return emit(output, drawing(ds::render::IrDocument::parse(doc), format, width));
    }
    ds::model::Model model;
    if (!loadWorkspace(input, &model)) return 1;
    if (format == "html" || format == "json") {
      if (format == "json") return emit(output, ds::scene::Scene(model).render() + "\n");
      std::vector<std::string> focus;
      if (!diagram.empty()) {
        const auto shown = ds::query::Query(model).ask("shows", diagram);
        if (!shown.empty() && shown.front().kind == "error") {
          std::cerr << "no diagram named " << diagram << "\n";
          return 1;
        }
        for (const auto& a : shown) focus.push_back(a.id);
      }
      const std::string title = titleOf(input);
      const std::string page = ds::scene::Scene(model).page(title, css.empty() ? "" : readFile(css), focus);
      return emit(output, embed ? ds::scene::Scene::frame(page, title, height) : page);
    }
    if (diagram.empty()) {
      std::cerr << "which diagram? ds query " << input << " diagrams lists them\n";
      return 2;
    }
    const std::string doc = ds::render::IrExport(model).render(diagramId(diagram), format == "ir");
    if (doc.empty()) {
      std::cerr << "no diagram named " << diagram << "\n";
      return 1;
    }
    if (format == "ir") return emit(output, doc + "\n");
    if (format == "layout") return emit(output, layoutJson(doc, width));
    return emit(output, drawing(ds::render::IrDocument::parse(doc), format, width));
  }
};

}  // namespace

int main(int argc, char** argv) {
  CLI::App app{"ds: a software design as text, checked, drawn, compared and asked", "ds"};
  app.require_subcommand(1);
  app.footer("A workspace is a directory of .ds files read as one. Each command takes --help.");
  const auto workspace = [](CLI::App* cmd, std::string* dir) {
    cmd->add_option("workspace", *dir, "a directory of .ds files")->required();
  };
  const auto width = [](CLI::App* cmd, int* w) {
    cmd->add_option("-w,--width", *w, "wrap the drawing: columns for text and cells, pixels for svg")->check(CLI::PositiveNumber);
  };

  std::string dir, question, subject;
  bool json = false;

  auto* checkCmd = app.add_subcommand("check", "the findings of a workspace, then the counts of its model");
  workspace(checkCmd, &dir);
  checkCmd->add_flag("--json", json, "the findings and the counts as JSON");

  auto* exportCmd = app.add_subcommand("export", "the model as JSON: entities, relations, findings and its metamodel");
  workspace(exportCmd, &dir);

  Render render;
  auto* renderCmd = app.add_subcommand("render", "look at the design: the whole model in the viewer, or one diagram drawn");
  renderCmd->add_option("input", render.input, "a workspace, or a render-IR document to draw")->required();
  renderCmd->add_option("diagram", render.diagram, "a diagram: the viewer opens framed on it, or it is drawn alone");
  renderCmd
      ->add_option("-f,--format", render.format,
                   "html: the viewer · json: its scene · svg, text, cells: a diagram drawn · ir, layout: its data")
      ->check(CLI::IsMember({"html", "json", "svg", "text", "cells", "ir", "layout"}))
      ->capture_default_str();
  width(renderCmd, &render.width);
  renderCmd->add_option("-o,--output", render.output, "write to this file instead of stdout");
  auto* embed = renderCmd->add_flag("--embed", render.embed, "html as an <iframe> carrying the page, to paste into any HTML document");
  renderCmd->add_option("--height", render.height, "the height of the embedded frame, in pixels")
      ->check(CLI::PositiveNumber)
      ->needs(embed)
      ->capture_default_str();
  renderCmd->add_option("--css", render.css, "a stylesheet over the viewer's; it may redefine its colour and font variables")
      ->check(CLI::ExistingFile);

  std::string before, after, path, diagram, format = "text";
  int diffWidth = 0;
  auto* diffCmd = app.add_subcommand("diff", "the changes between two versions in the model's terms, or a diagram with them marked");
  diffCmd->add_option("before", before, "a directory, or a git revision")->required();
  diffCmd->add_option("after", after, "a directory, or a git revision")->required();
  diffCmd->add_option("path", path, "with git revisions: the workspace inside the repository; with directories: the diagram");
  diffCmd->add_option("diagram", diagram, "with git revisions: the diagram to draw with the changes marked");
  diffCmd->add_option("-f,--format", format, "how to draw the diagram")
      ->check(CLI::IsMember({"text", "svg", "cells"}))
      ->capture_default_str();
  width(diffCmd, &diffWidth);

  auto* queryCmd =
      app.add_subcommand("query", "ask the model: diagrams · entities · relations · artefacts · behaviours · shows · findings · coupling");
  workspace(queryCmd, &dir);
  queryCmd->add_option("question", question, "what to ask")->required();
  queryCmd->add_option("subject", subject, "what it is asked about");
  queryCmd->add_flag("--json", json, "answer as JSON");

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    const int code = app.exit(e);
    return code == 0 ? 0 : 2;  // help is not a failure; a wrong command line is bad usage
  }

  if (checkCmd->parsed()) return check(dir, json);
  if (renderCmd->parsed()) return render.run();
  if (diffCmd->parsed()) {
    std::string a = before, b = after;
    if (isDirectory(a) && isDirectory(b)) {
      if (diagram.empty()) diagram = path;  // two directories need no path: the third argument is the diagram
    } else {
      a = checkout(a, path.empty() ? "." : path);
      b = checkout(b, path.empty() ? "." : path);
      if (a.empty() || b.empty()) {
        std::cerr << "cannot read both revisions (give two directories, or two git revisions and a path)\n";
        return 1;
      }
    }
    const ds::model::Model first = load(a);
    const ds::model::Model second = load(b);
    // a version that does not load is no deletion of the design: say why, and compare nothing
    bool unloadable = false;
    for (const auto* version : {&first, &second}) {
      const std::string& name = version == &first ? before : after;
      long declared = 0;
      for (const auto& p : version->entities()) declared += p.second.expected ? 0 : 1;
      if (declared == 0) {
        std::cerr << name << ": no .ds file declares anything there\n";
        unloadable = true;
      }
      for (const auto& f : version->findings()) {
        if (f.code != "E-SYNTAX") continue;
        std::cout << name << ": " << f.location.file << ":" << f.location.line << ":" << f.location.column << "  " << f.code << "  "
                  << f.message << "\n";
        unloadable = true;
      }
    }
    if (unloadable) return 1;
    const ds::diff::DiffResult result = ds::diff::Diff::compare(first, second);
    if (!diagram.empty()) {
      const ds::model::Model marked = ds::diff::Diff::marked(first, second, result);
      const std::string doc = ds::render::IrExport(marked).render(diagramId(diagram), false);
      if (doc.empty()) {
        std::cerr << "no diagram named " << diagram << "\n";
        return 1;
      }
      std::cout << drawing(ds::render::IrDocument::parse(doc), format, diffWidth);
      return 0;
    }
    std::cout << ds::diff::Diff::report(result);
    return result.findings.empty() ? 0 : 1;
  }

  ds::model::Model model;
  if (!loadWorkspace(dir, &model)) return 1;
  if (exportCmd->parsed()) {
    std::cout << ds::exporter::JsonExport().render(model) << "\n";
    return 0;
  }
  const ds::query::Query query(model);
  const std::vector<ds::query::Answer> answers = query.ask(question, subject);
  if (!answers.empty() && answers.front().kind == "error") {
    if (json) std::cout << ds::query::Query::json(answers) << "\n";  // asked for JSON, the error is an answer too
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
