// ds — the DriftSpec command line: `ds check`, `ds export` and `ds render <workspace> [diagram]`.
#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "driftspec/bind/Binder.hpp"
#include "driftspec/check/Checker.hpp"
#include "driftspec/export/JsonExport.hpp"
#include "driftspec/model/Model.hpp"
#include "driftspec/parse/Parser.hpp"
#include "driftspec/render/IrExport.hpp"
#include "driftspec/render/Svg.hpp"
#include "driftspec/render/Text.hpp"
#include "driftspec/diff/Diff.hpp"

#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>

#include <unistd.h>

#include <fstream>
#include <sstream>

namespace {

int usage() {
  std::cerr << "usage: ds check <workspace-dir>\n       ds export <workspace-dir>\n       ds render <workspace-dir> [diagram] [--svg|--text|--cells] [--width N]   (render-IR JSON, or the SVG / terminal drawing / cell grid by ds itself; N wraps the drawing)\n       ds draw <render-ir.json>                      (SVG from any render-IR document)\n       ds coupling <workspace-dir>                   (what every behaviour touches outside its module)\n       ds diff <A> <B> [path] [--svg|--text|--cells <diagram>]   (design diff: two directories, or two git revisions of <path>)\n";
  return 2;
}

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
  const std::string command = "git -C '" + root + "' archive '" + rev + "' -- '" + relative + "' 2>/dev/null | tar -x -C '" + std::string(dir) + "' 2>/dev/null";
  if (std::system(command.c_str()) != 0) return "";
  return std::string(dir) + "/" + relative;
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
  // --width N (columns for text and cells, pixels for svg) wraps the drawing to the screen
  int width = 0;
  std::vector<char*> kept;
  for (int i = 0; i < argc; ++i) {
    if (std::string(argv[i]) == "--width" && i + 1 < argc) {
      width = std::atoi(argv[++i]);
      continue;
    }
    kept.push_back(argv[i]);
  }
  argc = static_cast<int>(kept.size());
  argv = kept.data();
  if (argc < 3 || argc > 7) return usage();
  const std::string verb = argv[1];
  const std::string dir = argv[2];
  if (verb == "diff") {
    if (argc < 4) return usage();
    std::string a = argv[2], b = argv[3];
    int next = 4;
    std::string path = ".";
    if (argc > next && std::string(argv[next]).rfind("--", 0) != 0) path = argv[next++];
    if (!isDirectory(a) || !isDirectory(b)) {
      a = checkout(a, path);
      b = checkout(b, path);
      if (a.empty() || b.empty()) {
        std::cerr << "cannot read both revisions (give two directories, or two git revisions and a path)\n";
        return 1;
      }
    }
    const driftspec::model::Model before = load(a);
    const driftspec::model::Model after = load(b);
    const driftspec::diff::DiffResult result = driftspec::diff::Diff::compare(before, after);
    if (argc > next + 1) {
      const std::string how = argv[next], diagram = argv[next + 1];
      const driftspec::model::Model marked = driftspec::diff::Diff::marked(before, after, result);
      const std::string doc = driftspec::render::IrExport(marked).render(diagram.rfind("diagram:", 0) == 0 ? diagram : "diagram:" + diagram, false);
      if (doc.empty()) {
        std::cerr << "no diagram named " << diagram << "\n";
        return 1;
      }
      if (how == "--svg") std::cout << driftspec::render::Svg().render(driftspec::render::IrDocument::parse(doc), width);
      else if (how == "--cells") std::cout << driftspec::render::Text().cells(driftspec::render::IrDocument::parse(doc), width) << "\n";
      else std::cout << driftspec::render::Text().render(driftspec::render::IrDocument::parse(doc), isatty(1) != 0, width);
      return 0;
    }
    std::cout << driftspec::diff::Diff::report(result);
    return result.findings.empty() ? 0 : 1;
  }
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
    const bool cells = argc == 5 && std::string(argv[4]) == "--cells";
    const std::string doc = ir.render(name.rfind("diagram:", 0) == 0 ? name : "diagram:" + name, !svg && !text && !cells);
    if (doc.empty()) {
      std::cerr << "no diagram named " << name << "\n";
      return 1;
    }
    if (svg) {
      std::cout << driftspec::render::Svg().render(driftspec::render::IrDocument::parse(doc), width);
      return 0;
    }
    if (text) {
      std::cout << driftspec::render::Text().render(driftspec::render::IrDocument::parse(doc), isatty(1) != 0, width);
      return 0;
    }
    if (cells) {
      std::cout << driftspec::render::Text().cells(driftspec::render::IrDocument::parse(doc), width) << "\n";
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
  if (verb == "coupling") {
    // derived from behaviour: a local or an attribute typed in another module is coupling to it
    std::map<std::string, std::vector<std::string>> byBehaviour;
    for (const auto& r : model.relations()) {
      if (r.kind != "couples-structural" && r.kind != "couples-transient") continue;
      byBehaviour[r.from].push_back(r.to + (r.kind == "couples-transient" ? "  (transient: created inside)" : "  (structural: an attribute or parameter)"));
    }
    if (byBehaviour.empty()) std::cout << "no behaviour couples to another module\n";
    for (const auto& b : byBehaviour) {
      const driftspec::model::Entity* seq = model.find(b.first);
      std::cout << (seq ? seq->name : b.first) << "  (module " << (seq ? seq->scope : "?") << ")\n";
      for (const auto& t : b.second) std::cout << "  -> " << t << "\n";
    }
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
