#include "ds/format/Descriptions.hpp"

#include <string>
#include <vector>

namespace ds {
namespace format {

ReportDescription::ReportDescription(const check::Report& report) : report_(report) {}

void ReportDescription::findings(const std::vector<model::Finding>& findings, emit::ISink& out_sink) const {
  out_sink.beginList();
  for (const auto& f : findings) {
    out_sink.beginRecord().field("code").value(f.code).field("location").beginRecord();
    out_sink.field("column").integer(f.location.column).field("file").value(f.location.file).field("line").integer(f.location.line);
    out_sink.endRecord().field("message").value(f.message).field("severity").value(report_.severityOf(f.severity).name).endRecord();
  }
  out_sink.endList();
}

void ReportDescription::describe(emit::ISink& out_sink) const {
  out_sink.beginRecord().field("coverage");
  findings(report_.coverage, out_sink);
  out_sink.field("entities").integer(report_.entities).field("errors").integer(report_.errors);
  out_sink.field("expected").integer(report_.expected).field("findings");
  findings(report_.findings, out_sink);
  out_sink.field("relations").integer(report_.relations).endRecord();
}

AnswersDescription::AnswersDescription(const std::vector<query::Answer>& answers) : answers_(answers) {}

void AnswersDescription::describe(emit::ISink& out_sink) const {
  out_sink.beginList();
  for (const auto& a : answers_)
    out_sink.beginRecord().field("detail").value(a.detail).field("id").value(a.id).field("kind").value(a.kind).endRecord();
  out_sink.endList();
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

void LayoutDescription::describe(emit::ISink& out_sink) const { placed(render::IrDocument::parse(ir_.json), nullptr, out_sink); }

CellsDescription::CellsDescription(const render::IrText& ir, const theme::Theme& theme, int width) : ir_(ir), text_(theme), width_(width) {}

void CellsDescription::describe(emit::ISink& out_sink) const { text_.cells(render::IrDocument::parse(ir_.json), width_, out_sink); }

}  // namespace format
}  // namespace ds
