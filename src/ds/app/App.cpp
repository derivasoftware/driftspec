// ds — the DriftSpec command line: check, export, render, query and the manual over a workspace.
#include "ds/app/App.hpp"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#define DS_ISATTY(fd) (_isatty(fd) != 0)
#else
#include <pthread.h>
#include <unistd.h>
#define DS_ISATTY(fd) (isatty(fd) != 0)
#endif

#include <CLI/CLI.hpp>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "ds/api/Api.hpp"
#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Report.hpp"
#include "ds/emit/SinkFactory.hpp"
#include "ds/export/JsonExport.hpp"
#include "ds/format/DiagramFormats.hpp"
#include "ds/format/SceneFormats.hpp"
#include "ds/format/TextFormats.hpp"
#include "ds/model/Manual.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"
#include "ds/query/Query.hpp"
#include "ds/render/IrExport.hpp"
#include "ds/render/Layout.hpp"
#include "ds/render/Svg.hpp"
#include "ds/render/Text.hpp"
#include "ds/scene/Scene.hpp"
#include "ds/theme/Theme.hpp"

namespace ds {
namespace app {

bool isDirectory(const std::string& path) {
  std::error_code error;
  return std::filesystem::is_directory(path, error);
}

ds::model::Model load(const ds::model::Metamodel& metamodel, const std::string& dir, ds::api::Output output, const std::string& diagram) {
  ds::parse::Parser parser;
  // loaded as the Api loads texts (DS-120); the Api chooses which steps of the check run for the output (DS-376)
  return ds::api::Api(metamodel).load(parser.parseDirectory(dir), output, diagram);
}

/// Whether any .ds file lies under a directory: a directory with none is no workspace (a mistyped path, most often).
bool holdsDesign(const std::string& dir) {
  std::error_code error;
  for (std::filesystem::recursive_directory_iterator it(dir, error), end; !error && it != end; it.increment(error))
    if (it->is_regular_file() && it->path().extension() == ".ds") return true;
  return false;
}

/// The workspace loaded for what the command gives of it (DS-376), or nothing when the path is no workspace
/// (reported on stderr): every command then exits 2, bad usage, since the command line names no design (DS-83).
bool loadWorkspace(const ds::model::Metamodel& metamodel, const std::string& dir, ds::api::Output output, const std::string& diagram,
                   ds::model::Model* model) {
  if (!isDirectory(dir)) {
    std::cerr << "cannot read " << dir << ": not a directory\n";
    return false;
  }
  if (!holdsDesign(dir)) {
    std::cerr << "no .ds file under " << dir << ": it is no workspace\n";
    return false;
  }
  *model = load(metamodel, dir, output, diagram);
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
  std::ofstream out(output, std::ios::binary);  // the same bytes on every system (DS-283)
  if (!out) {
    std::cerr << "cannot write " << output << "\n";
    return 1;
  }
  out << text;
  return 0;
}

/// A page of the manual ds embeds; an unknown topic lists the ones there are.
int manual(const ds::model::Metamodel& metamodel, const ds::model::Manual& book, const std::string& topic, bool json) {
  const std::string page = book.page(topic);
  if (page.empty()) {
    std::cerr << "no page '" << topic << "' in the manual; there are:";
    for (const auto& t : book.topics()) std::cerr << " " << t;
    std::cerr << "\n";
    return 1;
  }
  if (!json) {
    std::cout << page;
    return 0;
  }
  // the page as JSON, written by the sink asked for here (DS-397): its fields in byte order
  const auto sink = ds::emit::SinkFactory::json(true);
  sink->beginRecord();
  if (const ds::model::CodeSpec* c = metamodel.codeOf(topic)) {
    sink->field("code").beginRecord().field("code").value(c->code).field("fix").value(c->fix).field("meaning").value(c->meaning);
    sink->field("severity").value(metamodel.severityOf(c->severity).name).endRecord();
  }
  const ds::model::KindSpec* kind = nullptr;  // the kind the topic names, the last when two did
  for (const auto& k : metamodel.kinds())
    if (topic == metamodel.nameOf(k.kind)) kind = &k;
  if (kind) {
    sink->field("kind").beginRecord().field("example").value(kind->example).field("fields").beginList();
    for (const auto& f : kind->fields) {
      sink->beginRecord().field("meaning").value(f.meaning).field("name").value(f.name).field("source").value(
          metamodel.fieldSourceName(f.source));
      sink->field("type").value(metamodel.fieldTypeName(f.type)).field("values").beginList();
      for (const auto& v : f.values) sink->value(v);
      sink->endList().endRecord();
    }
    sink->endList().field("flags").beginList();
    for (const auto& flag : kind->flags) sink->value(flag);
    sink->endList().field("meaning").value(kind->meaning).field("syntax").value(kind->syntax).endRecord();
  }
  sink->field("text").value(page).field("topic").value(topic.empty() ? "index" : topic);
  if (topic.empty()) {
    sink->field("topics").beginList();
    for (const auto& t : book.topics()) sink->value(t);
    sink->endList();
  }
  sink->endRecord();
  std::cout << sink->result() << "\n";
  return 0;
}

int check(const ds::model::Metamodel& metamodel, const std::string& dir, bool json) {
  ds::model::Model model(metamodel);
  if (!loadWorkspace(metamodel, dir, ds::api::Output::Findings, "", &model))
    return 2;  // no workspace to check: the command line is wrong, not the design
  // the one report of a check (DS-120), written by the format asked for (DS-126)
  const ds::check::Report report = ds::check::Report::of(model, metamodel);
  ds::format::FormatOptions options;
  options.title = dir;  // the workspace as it was given: where the report says the open coverage is listed (DS-431)
  const auto sink = ds::emit::SinkFactory::json(true);  // indented, as ds check --json has always printed it
  std::cout << ds::format::reportFormats().at(json ? "json" : "text")->write(report, options, *sink) << (json ? "\n" : "");
  return report.errors ? 1 : 0;
}

std::string diagramId(const std::string& name) { return name.rfind("diagram:", 0) == 0 ? name : "diagram:" + name; }

namespace {

/// One way to look at the design: the whole model in the viewer (html, or its data as json), or one
/// diagram drawn still (svg, text, cells) or as its render-IR (ir, layout).
struct Render {
  std::string input, diagram, format = "html", output, css;
  int width = 0, height = 640;
  bool embed = false;

  int run(const ds::model::Metamodel& metamodel, const ds::theme::Theme& theme) const {
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
      const auto sink = ds::emit::SinkFactory::json(false);                                   // what layout and cells write
      return emit(output, diagramFormat(theme)->write({doc}, drawingOptions(false), *sink));  // as it came, drawn by the format named
    }
    // the viewer shows the findings and what every step derives; a drawing, only what it draws (DS-376)
    const bool viewer = format == "html" || format == "json";
    ds::model::Model model(metamodel);
    if (!loadWorkspace(metamodel, input, viewer ? ds::api::Output::Viewer : ds::api::Output::Drawing, diagram, &model)) return 2;
    if (viewer) {
      // the scene is the document; the format named writes it (DS-126)
      const auto formats = ds::format::sceneFormats();
      ds::format::FormatOptions options;
      if (format == "json") {
        const auto sink = ds::emit::SinkFactory::json(true);  // the scene indented for a reader
        return emit(output, formats.at("json")->write(ds::scene::Scene(model, metamodel).build(), options, *sink) + "\n");
      }
      std::vector<std::string> focus;
      if (!diagram.empty()) {
        const auto shown = ds::query::Query(model, metamodel).ask("shows", diagram);
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
      const auto sink = ds::emit::SinkFactory::json(false);  // the scene and the focus travel compact inside the page
      return emit(output, formats.at("html")->write(ds::scene::Scene(model, metamodel).build(), options, *sink));
    }
    // a diagram is not drawn of what did not read whole (DS-433): a file of its own, or one that declares what it
    // shows, that did not parse may hold the very boxes it draws. A diagram that is missing or shows nothing may have
    // been lost to any of them; a file it does not read stops nothing (DS-411)
    if (!diagram.empty()) {
      std::set<std::string> reads;
      const ds::model::Entity* d = model.find(diagramId(diagram));
      if (d) reads.insert(d->location.file);
      std::size_t shown = 0;
      for (const auto& a : ds::query::Query(model, metamodel).ask("shows", diagram))
        if (const ds::model::Entity* e = a.kind == "error" ? nullptr : model.find(a.id)) {
          reads.insert(e->location.file);
          ++shown;
        }
      const bool lost = !d || shown == 0;  // missing, or showing nothing: what did not parse may be why
      std::size_t unread = 0;
      for (const auto& f : model.findings()) {
        if ((f.code != "E-SYNTAX" && f.code != "E-ENCODING") || !(lost || reads.count(f.location.file))) continue;
        std::cerr << f.location.file << ":" << f.location.line << ":" << f.location.column << "  " << f.code << "  " << f.message << "\n";
        ++unread;
      }
      if (unread) {
        const std::string files = unread == 1 ? "a file" : std::to_string(unread) + " files";
        if (!lost)
          std::cerr << diagram << " is not drawn: " << files << " it reads " << (unread == 1 ? "does" : "do") << " not read whole\n";
        else
          std::cerr << diagram << (d ? " shows nothing" : " is not found") << ", and " << files << " of " << input << " "
                    << (unread == 1 ? "does" : "do") << " not read whole: what did not parse may be why\n";
        return 1;
      }
    }
    if (diagram.empty()) {
      std::cerr << "which diagram? ds query " << input << " diagrams lists them\n";
      return 2;
    }
    const std::string doc = ds::render::IrExport(model, metamodel).render(diagramId(diagram), false);
    if (doc.empty()) {
      std::cerr << "no diagram named " << diagram << "\n";
      return 1;
    }
    const auto sink = ds::emit::SinkFactory::json(false);  // what layout and cells write
    return emit(output, diagramFormat(theme)->write({doc}, drawingOptions(true), *sink) + (format == "ir" ? "\n" : ""));
  }

  // a diagram is its IR, written by the format named (DS-126); the IR of a workspace is indented
  std::unique_ptr<ds::format::IFormat<ds::render::IrText>> diagramFormat(const ds::theme::Theme& theme) const {
    auto formats = ds::format::diagramFormats(theme);
    return std::move(formats.at(format));
  }
  ds::format::FormatOptions drawingOptions(bool workspace) const {
    ds::format::FormatOptions options;
    options.pretty = workspace;
    options.width = width;
    options.color = DS_ISATTY(1);
    return options;
  }
};

}  // namespace

int run(int argc, char** argv) {
  const ds::model::Metamodel metamodel;     // the language, made once where the command starts (DS-341)
  const ds::model::Manual book(metamodel);  // its manual, with the commands and their exit codes (DS-342)
  const ds::theme::Theme theme;             // the style every drawing is written in, made once (DS-393)
  CLI::App app{"ds: a software design as text, checked, drawn and asked", "ds"};
  app.set_version_flag("--version", "ds " + metamodel.version());
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

  auto* queryCmd = app.add_subcommand("query", "ask the model: " + ds::query::Query::usage());
  workspace(queryCmd, &dir);
  queryCmd->add_option("question", question, "what to ask")->required();
  std::string takes = "what it is asked about:";  // each question's, from the table the check of a subject reads (DS-436)
  for (const auto& q : ds::query::Query::questions()) takes += "\n  " + q.name + ": " + q.takes;
  queryCmd->add_option("subject", subject, takes);
  queryCmd->add_flag("--json", json, "answer as JSON");

  std::string topic;
  auto* manualCmd = app.add_subcommand("manual", "this manual: how each kind is written, what each finding means, the commands");
  manualCmd->add_option("topic", topic, "a kind (class, sequence...), a finding code, commands, annotations or codes");
  manualCmd->add_flag("--json", json, "the page as JSON");

  // every command says its exit codes, from the one table the manual prints too (DS-79)
  for (auto* cmd : {checkCmd, exportCmd, renderCmd, queryCmd, manualCmd}) {
    const ds::model::CommandSpec* spec = book.commandOf(cmd->get_name());
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

  if (manualCmd->parsed()) return manual(metamodel, book, topic, json);
  if (checkCmd->parsed()) return check(metamodel, dir, json);
  if (renderCmd->parsed()) return render.run(metamodel, theme);
  ds::model::Model model(metamodel);
  if (!loadWorkspace(metamodel, dir, exportCmd->parsed() ? ds::api::Output::Model : ds::api::Output::Answers, "", &model)) return 2;
  if (exportCmd->parsed()) {
    const auto sink = ds::emit::SinkFactory::json(true);  // indented, as ds export has always printed it; written whole or not at all
    ds::exporter::JsonExport(metamodel).render(model, *sink);
    std::cout << sink->result() << "\n";
    return 0;
  }
  const ds::query::Query query(model, metamodel);
  const std::vector<ds::query::Answer> answers = query.ask(question, subject);
  if (!answers.empty() && answers.front().kind == "error") {
    const auto sink = ds::emit::SinkFactory::json(false);
    if (json) std::cout << ds::format::AnswersJson().write(answers, {}, *sink) << "\n";  // asked for JSON, the error is an answer too
    std::cerr << answers.front().id << ": " << answers.front().detail << "\n";
    if (answers.front().detail.find("not a question") != std::string::npos) {
      std::cerr << "questions: " << ds::query::Query::usage() << "\n";
    }
    return 1;
  }
  // the answers, written by the format asked for (DS-126)
  const auto sink = ds::emit::SinkFactory::json(false);
  std::cout << ds::format::answerFormats().at(json ? "json" : "text")->write(answers, {}, *sink) << (json ? "\n" : "");
  return 0;
}

int work(int argc, char** argv) {
  try {
    return run(argc, argv);
  } catch (const std::exception& e) {
    // a file that cannot be read, or anything else the input makes impossible: said, with the code of bad
    // input, never an abort (DS-279)
    std::cerr << "ds: " << e.what() << "\n";
    return 2;
  }
}

// The work runs on a thread with a large stack: the binder walks a design as deep as it is nested, and the
// 8 MB of the main thread end near 1,500 nested blocks (DS-279). The memory is reserved, not used. On Windows
// the stack of the main thread is set when ds is linked (DS-283), and the work runs there.
int main(int argc, char** argv) {
#ifdef _WIN32
  // what ds writes is the same bytes on every system: no line ending is turned into \r\n (DS-283)
  _setmode(_fileno(stdout), _O_BINARY);
  return work(argc, argv);
#else
  struct Call {  // what the thread that does the work is given and gives back
    int argc;
    char** argv;
    int code;
  } call{argc, argv, 2};
  const auto thread = [](void* raw) -> void* {
    auto* c = static_cast<Call*>(raw);
    c->code = work(c->argc, c->argv);
    return nullptr;
  };
  pthread_attr_t attributes;
  pthread_attr_init(&attributes);
  pthread_attr_setstacksize(&attributes, std::size_t{512} * 1024 * 1024);
  pthread_t worker;
  if (pthread_create(&worker, &attributes, thread, &call) != 0) {
    call.code = work(argc, argv);  // no thread to be had: the work runs here, on the stack there is
  } else {
    pthread_join(worker, nullptr);
  }
  pthread_attr_destroy(&attributes);
  return call.code;
#endif
}

}  // namespace app
}  // namespace ds
