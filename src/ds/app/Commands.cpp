#include "ds/app/Commands.hpp"

#ifdef _WIN32
#include <io.h>
#define DS_ISATTY(fd) (_isatty(fd) != 0)
#else
#include <unistd.h>
#define DS_ISATTY(fd) (isatty(fd) != 0)
#endif

#include <CLI/CLI.hpp>
#include <optional>
#include <set>
#include <vector>

#include "ds/api/Drawings.hpp"
#include "ds/app/Disk.hpp"
#include "ds/app/Output.hpp"
#include "ds/emit/SinkFactory.hpp"
#include "ds/format/DescriptionFactory.hpp"
#include "ds/format/FormatFactory.hpp"
#include "ds/model/Identity.hpp"
#include "ds/query/Query.hpp"
#include "ds/render/IrExport.hpp"
#include "ds/scene/Scene.hpp"

namespace ds {
namespace app {

// ---- check (UC-00001-1) ----------------------------------------------------------------------------------------

std::string CheckCommand::summary() const { return "the findings of a workspace, then the counts of its model"; }

void CheckCommand::declare(CLI::App& sub) {
  sub.add_option("workspace", workspace_, "a directory of .ds files")->required();
  sub.add_flag("--json", json_, "the findings and the counts as JSON");
}

service::Outcome CheckCommand::run() const {
  const std::string why = Disk::whyNoWorkspace(workspace_);
  if (!why.empty()) return {"bad-usage", why};  // no workspace to check: the command line is wrong, not the design
  api::Workspace workspace = api::Workspace::madeFor(metamodel_, Disk::files(workspace_), api::Output::Findings);
  // the one report of a check, described into the sink of its technology (DS-399): with --json the JSON sink, indented
  // as ds check --json has always printed it (DS-398), else the text sink; the workspace as it was given is where the
  // report says the open coverage is listed (DS-431)
  const auto sink = emit::SinkFactory::of(json_ ? "json" : "text", true);
  const service::Outcome outcome = workspace.run("check-design", {{{"workspace", workspace_}}}, *sink);
  Output().write(sink->result(), json_ ? "\n" : "");
  return {outcome.name};  // an error is said by the report, on stdout: ds check has never said it again on stderr
}

// ---- export (UC-00005-1) ---------------------------------------------------------------------------------------

std::string ExportCommand::summary() const { return "the model as JSON: entities, relations, findings and its metamodel"; }

void ExportCommand::declare(CLI::App& sub) { sub.add_option("workspace", workspace_, "a directory of .ds files")->required(); }

service::Outcome ExportCommand::run() const {
  const std::string why = Disk::whyNoWorkspace(workspace_);
  if (!why.empty()) return {"no-workspace", why};
  api::Workspace workspace = api::Workspace::madeFor(metamodel_, Disk::files(workspace_), api::Output::Model);
  const auto sink = emit::SinkFactory::json(true);  // indented, as ds export has always printed it; written whole or not at all
  const service::Outcome outcome = workspace.run("hand-model", {}, *sink);
  Output().write(sink->result(), "\n");
  return outcome;
}

// ---- render (UC-00002-1) ---------------------------------------------------------------------------------------

std::string RenderCommand::summary() const { return "look at the design: the whole model in the viewer, or one diagram drawn"; }

void RenderCommand::declare(CLI::App& sub) {
  sub.add_option("input", input_, "a workspace, or a render-IR document to draw")->required();
  sub.add_option("diagram", diagram_, "a diagram: the viewer opens framed on it, or it is drawn alone");
  sub.add_option("-f,--format", format_, "html: the viewer · json: its scene · svg, text, cells: a diagram drawn · ir, layout: its data")
      ->check(CLI::IsMember({"html", "json", "svg", "text", "cells", "ir", "layout"}))
      ->capture_default_str();
  sub.add_option("-w,--width", width_, "wrap the drawing: columns for text and cells, pixels for svg")->check(CLI::PositiveNumber);
  sub.add_option("-o,--output", output_, "write to this file instead of stdout");
  auto* embed = sub.add_flag("--embed", embed_, "html as an <iframe> carrying the page, to paste into any HTML document");
  sub.add_option("--height", height_, "the height of the embedded frame, in pixels")
      ->check(CLI::PositiveNumber)
      ->needs(embed)
      ->capture_default_str();
  sub.add_option("--css", css_, "a stylesheet over the viewer's; it may redefine its colour and font variables")->check(CLI::ExistingFile);
}

service::Outcome RenderCommand::run() const {
  if (!Disk::isDirectory(input_)) return document();  // a render-IR document someone else produced
  const std::string why = Disk::whyNoWorkspace(input_);
  if (!why.empty()) return {"no-workspace", why};
  // the viewer shows the findings and what every step derives; a drawing, only what it draws (DS-376)
  const bool viewer = format_ == "html" || format_ == "json";
  api::Workspace workspace =
      api::Workspace::madeFor(metamodel_, Disk::files(input_), viewer ? api::Output::Viewer : api::Output::Drawing, diagram_);
  if (format_ == "json") return scene(workspace);
  if (viewer) return page(workspace);
  return diagram(workspace);
}

service::Outcome RenderCommand::document() const {
  if (!Disk::readable(input_)) return {"no-workspace", "cannot read " + input_ + ": neither a workspace nor a render-IR document"};
  const std::string doc = Disk::read(input_);
  if (format_ == "html" || format_ == "json")
    return {"bad-usage", "a render-IR document draws as svg, text, cells or layout; the viewer needs a workspace"};
  if (format_ == "ir") return written(doc, "", {"default"});  // the IR as it came
  // read once, here where the text arrives (DS-458): what draws it is handed the document
  return written(draw(render::IrDocument::parse(doc), false), "", {"default"});
}

service::Outcome RenderCommand::scene(api::Workspace& workspace) const {
  // the scene is the document (DS-126), described in the technology -f names, indented for a reader (DS-398)
  const auto sink = emit::SinkFactory::of(format_, true);
  const service::Outcome outcome = workspace.run("see-design", {{{"format", format_}}}, *sink);
  return written(sink->result(), "\n", outcome);
}

// the door's until see-design frames the viewer on a diagram: what the diagram shows is the page's focus, and the
// page is the viewer's around the scene
service::Outcome RenderCommand::page(const api::Workspace& workspace) const {
  std::vector<std::string> focus;
  if (!diagram_.empty()) {
    const auto shown = query::QueryFactory::of(workspace.model(), metamodel_)->ask("shows", diagram_);
    if (!shown.empty() && shown.front().kind == "error") return {"no-diagram", "no diagram named " + diagram_};
    for (const auto& a : shown) focus.push_back(a.id);
  }
  format::FormatOptions options;
  options.title = Disk::titleOf(input_);
  options.css = css_.empty() ? "" : Disk::read(css_);
  options.focus = focus;
  options.embed = embed_;
  options.height = height_;
  const auto sink = emit::SinkFactory::json(false);  // the scene and the focus travel compact inside the page
  const scene::SceneDocument document = scene::Scene(workspace.model(), metamodel_).build();
  if (!document.walkthroughs.empty()) options.drawings = api::Drawings::of(workspace.model(), metamodel_, theme_);  // the cards (DS-470)
  return written(format::FormatFactory::scene().at("html")->write(document, options, *sink), "", {"default"});
}

service::Outcome RenderCommand::diagram(api::Workspace& workspace) const {
  if (!diagram_.empty()) {
    const std::string why = unread(workspace.model());
    if (!why.empty()) return {"no-diagram", why};
  }
  if (diagram_.empty()) return {"bad-usage", "which diagram? ds query " + input_ + " diagrams lists them"};
  if (format_ == "layout" || format_ == "cells") {
    // its layout and its cells are see-design's, described in compact JSON, a line after (DS-398)
    const auto sink = emit::SinkFactory::json(false);
    const service::Outcome outcome =
        workspace.run("see-design", {{{"format", format_}, {"diagram", diagram_}, {"width", std::to_string(width_)}}}, *sink);
    if (outcome.name != "default") return outcome;
    return written(sink->result(), "\n", outcome);
  }
  // the door's until see-design draws them (svg and text, DS-400) and writes the IR: the IR, made of the model, is
  // described indented for a reader, or handed as the document to the writer -f names (DS-418, DS-458)
  const std::optional<render::IrDocument> document =
      render::IrExport(workspace.model(), metamodel_).document(model::Identity::ofDiagram(diagram_).text());
  if (!document) return {"no-diagram", "no diagram named " + diagram_};
  if (format_ == "ir") {
    const auto sink = emit::SinkFactory::json(true);
    format::DescriptionFactory::ir(*document)->describe(*sink);
    return written(sink->result(), "\n", {"default"});
  }
  if (format_ == "svg")  // the one path from a diagram to its drawing, the walkthrough's cards' too (DS-470)
    return written(api::Drawings::svg(workspace.model(), metamodel_, theme_, diagram_, width_), "", {"default"});
  return written(draw(*document, true), "", {"default"});  // the document drawn, with no text between (DS-458)
}

// a diagram is not drawn of what did not read whole (DS-433): a file of its own, or one that declares what it shows,
// that did not parse may hold the very boxes it draws. A diagram that is missing or shows nothing may have been lost to
// any of them; a file it does not read stops nothing (DS-411). The door's until see-design says it.
std::string RenderCommand::unread(const model::Model& model) const {
  std::set<std::string> reads;
  const model::Entity* d = model.find(model::Identity::ofDiagram(diagram_).text());
  if (d) reads.insert(d->location.file);
  std::size_t shown = 0;
  for (const auto& a : query::QueryFactory::of(model, metamodel_)->ask("shows", diagram_))
    if (const model::Entity* e = a.kind == "error" ? nullptr : model.find(a.id)) {
      reads.insert(e->location.file);
      ++shown;
    }
  const bool lost = !d || shown == 0;  // missing, or showing nothing: what did not parse may be why
  std::string said;
  std::size_t count = 0;
  for (const auto& f : model.findings()) {
    if ((f.code != "E-SYNTAX" && f.code != "E-ENCODING") || !(lost || reads.count(f.location.file))) continue;
    said += f.location.file + ":" + std::to_string(f.location.line) + ":" + std::to_string(f.location.column) + "  " + f.code + "  " +
            f.message + "\n";
    ++count;
  }
  if (!count) return "";
  const std::string files = count == 1 ? "a file" : std::to_string(count) + " files";
  const std::string verb = count == 1 ? "does" : "do";
  if (!lost) return said + diagram_ + " is not drawn: " + files + " it reads " + verb + " not read whole";
  return said + diagram_ + (d ? " shows nothing" : " is not found") + ", and " + files + " of " + input_ + " " + verb +
         " not read whole: what did not parse may be why";
}

// a diagram is its IR (DS-126): its layout and its cells are described in compact JSON, a line after (DS-398); the
// drawings are written by the writer -f names; each is handed the document (DS-458)
std::string RenderCommand::draw(const render::IrDocument& ir, bool workspace) const {
  const auto sink = emit::SinkFactory::json(false);
  if (format_ == "layout" || format_ == "cells") {
    const auto description = format_ == "layout" ? format::DescriptionFactory::layout(ir, theme_, width_)
                                                 : format::DescriptionFactory::cells(ir, theme_, width_);
    description->describe(*sink);
    return sink->result() + "\n";
  }
  return format::FormatFactory::diagram(theme_).at(format_)->write(ir, drawingOptions(workspace), *sink);
}

format::FormatOptions RenderCommand::drawingOptions(bool workspace) const {
  format::FormatOptions options;
  options.pretty = workspace;
  options.width = width_;
  options.color = DS_ISATTY(1);
  return options;
}

service::Outcome RenderCommand::written(const std::string& text, const std::string& end, const service::Outcome& outcome) const {
  const Output output(output_);
  if (!output.write(text, end)) return {"unwritten", "cannot write " + output_};
  return outcome;
}

// ---- query (UC-00004-1) ----------------------------------------------------------------------------------------

std::string QueryCommand::summary() const { return "ask the model: " + query::Query::usage(); }

void QueryCommand::declare(CLI::App& sub) {
  sub.add_option("workspace", workspace_, "a directory of .ds files")->required();
  sub.add_option("question", question_, "what to ask")->required();
  std::string takes = "what it is asked about:";  // each question's, from the table the check of a subject reads (DS-436)
  for (const auto& q : query::Query::questions()) takes += "\n  " + q.name + ": " + q.takes;
  sub.add_option("subject", subject_, takes);
  sub.add_flag("--json", json_, "answer as JSON");
}

service::Outcome QueryCommand::run() const {
  const std::string why = Disk::whyNoWorkspace(workspace_);
  if (!why.empty()) return {"no-workspace", why};
  api::Workspace workspace = api::Workspace::madeFor(metamodel_, Disk::files(workspace_), api::Output::Answers);
  // the answers (DS-126), described into the sink of their technology (DS-399): with --json the JSON sink (DS-398),
  // else the text sink
  const auto sink = emit::SinkFactory::of(json_ ? "json" : "text", false);
  const service::Outcome outcome = workspace.run("ask-model", {{{"question", question_}, {"subject", subject_}}}, *sink);
  const std::string answers = sink->result();
  // asked for JSON, the error is an answer too; as text, it is only said, on stderr
  if (outcome.name == "default" || json_) Output().write(answers, json_ ? "\n" : "");
  return outcome;
}

// ---- manual (UC-00006-1) ---------------------------------------------------------------------------------------

std::string ManualCommand::summary() const { return "this manual: how each kind is written, what each finding means, the commands"; }

void ManualCommand::declare(CLI::App& sub) {
  sub.add_option("topic", topic_, "a kind (class, sequence...), a finding code, commands, annotations or codes");
  sub.add_flag("--json", json_, "the page as JSON");
}

service::Outcome ManualCommand::run() const {
  // the manual is in the tool: the workspace holds no design, and learn-language reads none
  api::Workspace workspace = api::Workspace::madeFor(metamodel_, {});
  // the page as it is printed, in the text sink, or as JSON, indented, in the JSON sink (DS-397)
  const auto sink = emit::SinkFactory::of(json_ ? "json" : "text", true);
  const service::Outcome outcome = workspace.run("learn-language", {{{"topic", topic_}}}, *sink);
  if (outcome.name == "default") Output().write(sink->result(), json_ ? "\n" : "");
  return outcome;
}

}  // namespace app
}  // namespace ds
