#include "ds/app/Commands.hpp"

#ifdef _WIN32
#include <io.h>
#define DS_ISATTY(fd) (_isatty(fd) != 0)
#else
#include <unistd.h>
#define DS_ISATTY(fd) (isatty(fd) != 0)
#endif

#include <CLI/CLI.hpp>

#include "ds/app/Disk.hpp"
#include "ds/app/Output.hpp"
#include "ds/emit/SinkFactory.hpp"
#include "ds/format/DescriptionFactory.hpp"
#include "ds/model/Manual.hpp"
#include "ds/render/Svg.hpp"
#include "ds/render/Text.hpp"

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
  if (!viewer && diagram_.empty()) return {"bad-usage", "which diagram? ds query " + input_ + " diagrams lists them"};
  // every format is see-design's (DS-474): the structure in JSON, the scene and the IR indented for a reader and a
  // layout and its cells compact, a line after (DS-398); a drawing in svg or text and the page in the text sink, as
  // they are drawn, with no text between (DS-458)
  const bool structure = format_ == "json" || format_ == "ir" || format_ == "layout" || format_ == "cells";
  const auto sink = emit::SinkFactory::of(structure ? "json" : "text", format_ == "json" || format_ == "ir");
  const service::Outcome outcome = workspace.run("see-design", request(), *sink);
  if (outcome.name != "default") return outcome;  // no diagram, or one that did not read whole: said, nothing written
  return written(sink->result(), structure ? "\n" : "", outcome);
}

service::Request RenderCommand::request() const {
  service::Request asked{{{"format", format_},
                          {"diagram", diagram_},
                          {"width", std::to_string(width_)},
                          {"color", DS_ISATTY(1) ? "true" : "false"},
                          {"workspace", input_},
                          {"whole", "true"}}};
  if (format_ == "html") {  // the page: titled after the workspace, a reader's stylesheet over the viewer's, embedded or not
    asked.arguments["title"] = Disk::titleOf(input_);
    asked.arguments["css"] = css_.empty() ? "" : Disk::read(css_);
    asked.arguments["embed"] = embed_ ? "true" : "false";
    asked.arguments["height"] = std::to_string(height_);
  }
  return asked;
}

service::Outcome RenderCommand::document() const {
  if (!Disk::readable(input_)) return {"no-workspace", "cannot read " + input_ + ": neither a workspace nor a render-IR document"};
  const std::string doc = Disk::read(input_);
  if (format_ == "html" || format_ == "json")
    return {"bad-usage", "a render-IR document draws as svg, text, cells or layout; the viewer needs a workspace"};
  if (format_ == "ir") return written(doc, "", {"default"});  // the IR as it came
  // read once, here where the text arrives (DS-458): what draws it is handed the document
  return written(draw(render::IrDocument::parse(doc)), "", {"default"});
}

// a render-IR document is drawn here, the one drawing the command line makes itself, since it has no workspace: its
// layout and its cells described in compact JSON, a line after (DS-398); a drawing in svg or text as render draws it,
// each handed the document (DS-458)
std::string RenderCommand::draw(const render::IrDocument& ir) const {
  if (format_ == "layout" || format_ == "cells") {
    const auto sink = emit::SinkFactory::json(false);
    const auto description = format_ == "layout" ? format::DescriptionFactory::layout(ir, theme_, width_)
                                                 : format::DescriptionFactory::cells(ir, theme_, width_);
    description->describe(*sink);
    return sink->result() + "\n";
  }
  if (format_ == "svg") return render::Svg(theme_).render(ir, width_);
  return render::Text(theme_).render(ir, DS_ISATTY(1), width_);
}

service::Outcome RenderCommand::written(const std::string& text, const std::string& end, const service::Outcome& outcome) const {
  const Output output(output_);
  if (!output.write(text, end)) return {"unwritten", "cannot write " + output_};
  return outcome;
}

// ---- query (UC-00004-1) ----------------------------------------------------------------------------------------

std::string QueryCommand::summary() const { return "ask the model: " + model::Manual::usage(); }

void QueryCommand::declare(CLI::App& sub) {
  sub.add_option("workspace", workspace_, "a directory of .ds files")->required();
  sub.add_option("question", question_, "what to ask")->required();
  std::string takes = "what it is asked about:";  // each question's, from the manual's table the query reads (DS-436, DS-474)
  for (const auto& q : model::Manual::questions()) takes += "\n  " + q.name + ": " + q.takes;
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
