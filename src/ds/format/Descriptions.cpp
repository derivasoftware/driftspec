#include "ds/format/Descriptions.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ds {
namespace format {

ReportDescription::ReportDescription(const check::Report& report, const std::string& workspace) : report_(report), workspace_(workspace) {}

std::string ReportDescription::name() const { return "report"; }

void ReportDescription::findings(const std::vector<model::Finding>& findings, emit::ISink& out_sink) const {
  out_sink.beginList();
  for (const auto& f : findings) {
    out_sink.beginRecord().field("code").value(f.code).field("location").beginRecord();
    out_sink.field("column").integer(f.location.column).field("file").value(f.location.file).field("line").integer(f.location.line);
    out_sink.endRecord().field("message").value(f.message).field("severity").value(report_.severityOf(f.severity).name);
    if (!f.subject.empty()) out_sink.field("subject").value(f.subject);  // what it is about, when it says (DS-426)
    out_sink.endRecord();
  }
  out_sink.endList();
}

std::string ReportDescription::where(const model::Finding& finding) {
  return finding.location.file + ":" + std::to_string(finding.location.line) + ":" + std::to_string(finding.location.column);
}

std::string ReportDescription::count(std::size_t n, std::string_view one, std::string_view many) {
  return std::to_string(n) + " " + std::string(n == 1 ? one : many);
}

// what is not yet written is no finding, but a check that says nothing of it reads as done (DS-431): its count ends
// the sentence; with none open, the counts are as they were
std::string ReportDescription::counts() const {
  const std::size_t open = report_.coverage.size();
  return count(report_.entities, "entity", "entities") + " · " + std::to_string(report_.expected) + " expected · " +
         count(report_.relations, "relation", "relations") + " · " + count(report_.findings.size(), "finding", "findings") +
         (open ? " · " + std::to_string(open) + " open" : "");
}

std::string ReportDescription::listing() const {
  const std::size_t open = report_.coverage.size();
  return std::to_string(open) + (open == 1 ? " open item" : " open items") + " of coverage: ds query " +
         (workspace_.empty() ? "<workspace>" : workspace_) + " coverage lists them";
}

void ReportDescription::say(emit::ISink& out_sink) const {
  // the structure: one record of the findings, the coverage and the counts
  out_sink.beginRecord().field("coverage");
  findings(report_.coverage, out_sink);
  out_sink.field("entities").integer(report_.entities).field("errors").integer(report_.errors);
  out_sink.field("expected").integer(report_.expected).field("findings");
  findings(report_.findings, out_sink);
  out_sink.field("relations").integer(report_.relations).endRecord();
  // the presentation: each finding where it is, then the counts, then where the open coverage is listed
  out_sink.table({"location", "code", "message"});
  for (const auto& f : report_.findings) out_sink.row({where(f), f.code, f.message});
  out_sink.endTable().paragraph(counts());
  if (!report_.coverage.empty()) out_sink.paragraph(listing());
}

AnswersDescription::AnswersDescription(const std::vector<query::Answer>& answers) : answers_(answers) {}

std::string AnswersDescription::name() const { return "answers"; }

void AnswersDescription::say(emit::ISink& out_sink) const {
  out_sink.beginList();
  for (const auto& a : answers_) {
    // a relation's two ends when the answer has them (DS-445); every other answer is written as it was
    out_sink.beginRecord().field("detail").value(a.detail);
    if (!a.from.empty()) out_sink.field("from").value(a.from);
    out_sink.field("id").value(a.id).field("kind").value(a.kind);
    if (!a.to.empty()) out_sink.field("to").value(a.to);
    out_sink.endRecord();
  }
  out_sink.endList();
  // a row ends at its last cell: an answer with no detail has two
  out_sink.table({"kind", "id", "detail"});
  for (const auto& a : answers_) {
    if (a.detail.empty())
      out_sink.row({a.kind, a.id});
    else
      out_sink.row({a.kind, a.id, a.detail});
  }
  out_sink.endTable();
}

LayoutDescription::LayoutDescription(const render::IrText& ir, const theme::Theme& theme, int width)
    : ir_(ir), metrics_(render::Metrics::text(theme.measures(theme::Sink::text))), width_(width) {}

// The placed rectangles and routes of one drawing, in cells; of a drawing made of parts, those of each part. A
// part is named after the fields its drawing gives: part before parts, nodes before part before routes.
void LayoutDescription::placed(const render::IrDocument& document, const std::string* part, emit::ISink& out_sink) const {
  out_sink.beginRecord();
  if (!document.parts.empty()) {
    if (part) out_sink.field("part").value(*part);
    out_sink.field("parts").beginList();
    for (const auto& name : document.parts) placed(document.part(name), &name, out_sink);
    out_sink.endList().endRecord();
    return;
  }
  const render::IrDocument parsed = document.bundled();
  const render::LayoutResult lay = render::Layout(metrics_).place(parsed, width_);
  out_sink.field("nodes").beginList();
  for (const auto& p : lay.placed) {
    out_sink.beginRecord().field("h").integer(p.rect.h).field("id").value(p.node->id).field("kind").value(p.node->kind);
    out_sink.field("w").integer(p.rect.w).field("x").integer(p.rect.x).field("y").integer(p.rect.y).endRecord();
  }
  out_sink.endList();
  if (part) out_sink.field("part").value(*part);
  out_sink.field("routes").beginList();
  for (const auto& r : lay.routes) {
    const render::IrEdge& e = parsed.edges[r.first];
    out_sink.beginRecord().field("from").value(e.from).field("kind").value(e.kind).field("points").beginList();
    for (const auto& pt : r.second.points) out_sink.beginList().integer(pt.x).integer(pt.y).endList();
    out_sink.endList().field("to").value(e.to).endRecord();
  }
  out_sink.endList().endRecord();
}

std::string LayoutDescription::name() const { return "layout"; }

void LayoutDescription::say(emit::ISink& out_sink) const { placed(render::IrDocument::parse(ir_.json), nullptr, out_sink); }

CellsDescription::CellsDescription(const render::IrText& ir, const theme::Theme& theme, int width) : ir_(ir), text_(theme), width_(width) {}

std::string CellsDescription::name() const { return "cells"; }

void CellsDescription::say(emit::ISink& out_sink) const { text_.cells(render::IrDocument::parse(ir_.json), width_, out_sink); }

}  // namespace format
}  // namespace ds
