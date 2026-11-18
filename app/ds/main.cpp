// ds — the DriftSpec command line: check, export, render, query and the manual over a workspace.
#include <pthread.h>
#include <unistd.h>

#include <CLI/CLI.hpp>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "ds/api/Api.hpp"
#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Report.hpp"
#include "ds/export/JsonExport.hpp"
#include "ds/format/DiagramFormats.hpp"
#include "ds/format/SceneFormats.hpp"
#include "ds/format/TextFormats.hpp"
#include "ds/model/Metamodel.hpp"
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
  std::error_code error;
  return std::filesystem::is_directory(path, error);
}

ds::model::Model load(const std::string& dir) {
  ds::parse::Parser parser;
  return ds::api::Api::load(parser.parseDirectory(dir));  // loaded as the Api loads texts (DS-120)
}

/// Whether any .ds file lies under a directory: a directory with none is no workspace (a mistyped path, most often).
bool holdsDesign(const std::string& dir) {
  std::error_code error;
  for (std::filesystem::recursive_directory_iterator it(dir, error), end; !error && it != end; it.increment(error))
    if (it->is_regular_file() && it->path().extension() == ".ds") return true;
  return false;
}

/// The workspace loaded, or nothing when the path is no workspace (reported on stderr): every command
/// then exits 2, bad usage, since the command line names no design (DS-83).
bool loadWorkspace(const std::string& dir, ds::model::Model* model) {
  if (!isDirectory(dir)) {
    std::cerr << "cannot read " << dir << ": not a directory\n";
    return false;
  }
  if (!holdsDesign(dir)) {
    std::cerr << "no .ds file under " << dir << ": it is no workspace\n";
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

/// A page of the manual ds embeds; an unknown topic lists the ones there are.
int manual(const std::string& topic, bool json) {
  const std::string page = ds::model::manualPage(topic);
  if (page.empty()) {
    std::cerr << "no page '" << topic << "' in the manual; there are:";
    for (const auto& t : ds::model::manualTopics()) std::cerr << " " << t;
    std::cerr << "\n";
    return 1;
  }
  if (!json) {
    std::cout << page;
    return 0;
  }
  nlohmann::json out{{"topic", topic.empty() ? "index" : topic}, {"text", page}};
  for (const auto& k : ds::model::kinds()) {
    if (topic != ds::model::kindName(k.kind)) continue;
    nlohmann::json fields = nlohmann::json::array();
    for (const auto& f : k.fields)
      fields.push_back({{"name", f.name},
                        {"type", ds::model::fieldTypeName(f.type)},
                        {"source", ds::model::fieldSourceName(f.source)},
                        {"meaning", f.meaning},
                        {"values", f.values}});
    out["kind"] = {{"meaning", k.meaning}, {"syntax", k.syntax}, {"fields", fields}, {"flags", k.flags}, {"example", k.example}};
  }
  if (const ds::model::CodeSpec* c = ds::model::codeOf(topic))
    out["code"] = {{"code", c->code}, {"severity", c->severity}, {"meaning", c->meaning}, {"fix", c->fix}};
  if (topic.empty()) out["topics"] = ds::model::manualTopics();
  std::cout << out.dump(2) << "\n";
  return 0;
}

int check(const std::string& dir, bool json) {
  ds::model::Model model;
  if (!loadWorkspace(dir, &model)) return 2;  // no workspace to check: the command line is wrong, not the design
  // the one report of a check (DS-120), written by the format asked for (DS-126)
  const ds::check::Report report = ds::check::Report::of(model);
  ds::format::FormatOptions options;
  options.pretty = true;
  std::cout << ds::format::reportFormats().at(json ? "json" : "text")->write(report, options) << (json ? "\n" : "");
  return report.errors ? 1 : 0;
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
        std::cerr << "cannot read " << input << ": neither a workspace nor a render-IR document\n";
        return 2;
      }
      const std::string doc = readFile(input);
      if (format == "html" || format == "json") {
        std::cerr << "a render-IR document draws as svg, text, cells or layout; the viewer needs a workspace\n";
        return 2;
      }
      return emit(output, diagramFormat(false)->write({doc}, drawingOptions(false)));  // as it came, drawn by the format named
    }
    ds::model::Model model;
    if (!loadWorkspace(input, &model)) return 2;
    if (format == "html" || format == "json") {
      // the scene is the document; the format named writes it (DS-126)
      const auto formats = ds::format::sceneFormats();
      ds::format::FormatOptions options;
      if (format == "json") {
        options.pretty = true;
        return emit(output, formats.at("json")->write(ds::scene::Scene(model).build(), options) + "\n");
      }
      std::vector<std::string> focus;
      if (!diagram.empty()) {
        const auto shown = ds::query::Query(model).ask("shows", diagram);
        if (!shown.empty() && shown.front().kind == "error") {
          std::cerr << "no diagram named " << diagram << "\n";
          return 1;
        }
        for (const auto& a : shown) focus.push_back(a.id);
      }
      options.title = titleOf(input);
      options.css = css.empty() ? "" : readFile(css);
      options.focus = focus;
      options.embed = embed;
      options.height = height;
      return emit(output, formats.at("html")->write(ds::scene::Scene(model).build(), options));
    }
    if (diagram.empty()) {
      std::cerr << "which diagram? ds query " << input << " diagrams lists them\n";
      return 2;
    }
    const std::string doc = ds::render::IrExport(model).render(diagramId(diagram), false);
    if (doc.empty()) {
      std::cerr << "no diagram named " << diagram << "\n";
      return 1;
    }
    return emit(output, diagramFormat(true)->write({doc}, drawingOptions(true)) + (format == "ir" ? "\n" : ""));
  }

  // a diagram is its IR, written by the format named (DS-126); the IR of a workspace is indented
  std::unique_ptr<ds::format::IFormat<ds::render::IrText>> diagramFormat(bool) const {
    auto formats = ds::format::diagramFormats();
    return std::move(formats.at(format));
  }
  ds::format::FormatOptions drawingOptions(bool workspace) const {
    ds::format::FormatOptions options;
    options.pretty = workspace;
    options.width = width;
    options.color = isatty(1) != 0;
    return options;
  }
};

}  // namespace

int run(int argc, char** argv) {
  CLI::App app{"ds: a software design as text, checked, drawn, compared and asked", "ds"};
  app.require_subcommand(1);
  app.footer("A workspace is a directory of .ds files read as one. Each command takes --help; ds manual tells the rest.");
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

  auto* queryCmd = app.add_subcommand(
      "query",
      "ask the model: diagrams · entities · relations · artefacts · behaviours · shows · findings · coverage · coupling · failures");
  workspace(queryCmd, &dir);
  queryCmd->add_option("question", question, "what to ask")->required();
  queryCmd->add_option("subject", subject, "what it is asked about");
  queryCmd->add_flag("--json", json, "answer as JSON");

  std::string topic;
  auto* manualCmd = app.add_subcommand("manual", "this manual: how each kind is written, what each finding means, the commands");
  manualCmd->add_option("topic", topic, "a kind (class, sequence...), a finding code, commands, annotations or codes");
  manualCmd->add_flag("--json", json, "the page as JSON");

  // every command says its exit codes, from the one table the manual prints too (DS-79)
  for (auto* cmd : {checkCmd, exportCmd, renderCmd, queryCmd, manualCmd}) {
    const ds::model::CommandSpec* spec = ds::model::commandOf(cmd->get_name());
    if (!spec) continue;
    std::string exits = "Exit codes:";
    for (const auto& e : spec->exits) exits += "\n  " + std::to_string(e.first) + "  " + e.second;
    cmd->footer(exits);
  }

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    const int code = app.exit(e);
    return code == 0 ? 0 : 2;  // help is not a failure; a wrong command line is bad usage
  }

  if (manualCmd->parsed()) return manual(topic, json);
  if (checkCmd->parsed()) return check(dir, json);
  if (renderCmd->parsed()) return render.run();
  ds::model::Model model;
  if (!loadWorkspace(dir, &model)) return 2;
  if (exportCmd->parsed()) {
    std::cout << ds::exporter::JsonExport().render(model) << "\n";
    return 0;
  }
  const ds::query::Query query(model);
  const std::vector<ds::query::Answer> answers = query.ask(question, subject);
  if (!answers.empty() && answers.front().kind == "error") {
    if (json) std::cout << ds::format::AnswersJson().write(answers, {}) << "\n";  // asked for JSON, the error is an answer too
    std::cerr << answers.front().id << ": " << answers.front().detail << "\n";
    if (answers.front().detail.find("not a question") != std::string::npos) {
      std::cerr << "questions:";
      for (const auto& q : ds::query::Query::questions()) std::cerr << " " << q.first << (q.second.empty() ? "" : " " + q.second) << " ·";
      std::cerr << "\n";
    }
    return 1;
  }
  // the answers, written by the format asked for (DS-126)
  std::cout << ds::format::answerFormats().at(json ? "json" : "text")->write(answers, {}) << (json ? "\n" : "");
  return 0;
}

namespace {

/// What the thread that does the work is given and gives back.
struct Call {
  int argc;
  char** argv;
  int code;
};

void* work(void* raw) {
  auto* call = static_cast<Call*>(raw);
  try {
    call->code = run(call->argc, call->argv);
  } catch (const std::exception& e) {
    // a file that cannot be read, or anything else the input makes impossible: said, with the code of bad
    // input, never an abort (DS-279)
    std::cerr << "ds: " << e.what() << "\n";
    call->code = 2;
  }
  return nullptr;
}

}  // namespace

// The work runs on a thread with a large stack: the binder walks a design as deep as it is nested, and the
// 8 MB of the main thread end near 1,500 nested blocks (DS-279). The memory is reserved, not used.
int main(int argc, char** argv) {
  Call call{argc, argv, 2};
  pthread_attr_t attributes;
  pthread_attr_init(&attributes);
  pthread_attr_setstacksize(&attributes, std::size_t{512} * 1024 * 1024);
  pthread_t worker;
  if (pthread_create(&worker, &attributes, work, &call) != 0) {
    work(&call);  // no thread to be had: the work runs here, on the stack there is
  } else {
    pthread_join(worker, nullptr);
  }
  pthread_attr_destroy(&attributes);
  return call.code;
}
