#include "ds/format/Descriptions.hpp"

namespace ds {
namespace format {

// Every record names its keys in byte order (the sink checks it in tests): edges, ir, kind, nodes, notice, parts,
// table, title; a node's abstract to title; an edge's dashed to to; a table's columns, corner, rows.

std::string IrDescription::name() const { return "ir"; }

void IrDescription::say(emit::ISink& out_sink) const {
  const render::IrDocument& d = document_;
  out_sink.beginRecord().field("edges").beginList();
  for (const render::IrEdge& e : d.edges) edge(e, out_sink);
  out_sink.endList().field("ir").integer(1);  // the contract's version
  text("kind", d.kind, out_sink);
  out_sink.field("nodes").beginList();
  for (const render::IrNode& n : d.nodes) node(n, out_sink);
  out_sink.endList();
  text("notice", d.notice, out_sink);
  texts("parts", d.parts, out_sink);
  if (d.table) {
    out_sink.field("table");
    table(*d.table, out_sink);
  }
  text("title", d.title, out_sink);
  out_sink.endRecord();
}

void IrDescription::node(const render::IrNode& n, emit::ISink& out_sink) {
  out_sink.beginRecord();
  if (n.abstract) out_sink.field("abstract").boolean(*n.abstract);
  text("classifier", n.classifier, out_sink);
  if (!n.dividers.empty()) {
    out_sink.field("dividers").beginList();
    for (const auto& [at, label] : n.dividers) out_sink.beginRecord().field("at").integer(at).field("label").value(label).endRecord();
    out_sink.endList();
  }
  text("href", n.href, out_sink);
  out_sink.field("id").value(n.id);
  out_sink.field("kind").value(n.kind);
  out_sink.field("label").value(n.label);  // said even when empty: a start dot has no text (DS-442)
  text("parent", n.parent, out_sink);
  text("part", n.part, out_sink);
  refs(n.refs, out_sink);
  texts("sectionKinds", n.sectionKinds, out_sink);
  if (!n.sections.empty()) {
    out_sink.field("sections").beginList();
    for (const auto& section : n.sections) {
      out_sink.beginList();
      for (const std::string& line : section) out_sink.value(line);
      out_sink.endList();
    }
    out_sink.endList();
  }
  if (n.span.first >= 0) out_sink.field("span").beginList().integer(n.span.first).integer(n.span.second).endList();
  text("stereotype", n.stereotype, out_sink);
  text("title", n.title, out_sink);
  out_sink.endRecord();
}

void IrDescription::edge(const render::IrEdge& e, emit::ISink& out_sink) {
  out_sink.beginRecord();
  if (e.dashed) out_sink.field("dashed").boolean(true);
  out_sink.field("from").value(e.from);
  text("href", e.href, out_sink);
  out_sink.field("kind").value(e.kind);
  text("label", e.label, out_sink);
  if (e.order >= 0) out_sink.field("order").integer(e.order);
  text("part", e.part, out_sink);
  refs(e.refs, out_sink);
  text("title", e.title, out_sink);
  out_sink.field("to").value(e.to);
  out_sink.endRecord();
}

void IrDescription::table(const render::IrTable& t, emit::ISink& out_sink) {
  out_sink.beginRecord().field("columns").beginList();
  for (const std::string& column : t.columns) out_sink.value(column);
  out_sink.endList().field("corner").value(t.corner).field("rows").beginList();
  for (const render::IrRow& row : t.rows) {
    out_sink.beginRecord().field("cells").beginList();
    for (const render::IrCell& c : row.cells)
      out_sink.beginRecord().field("kind").value(c.kind).field("text").value(c.text).field("title").value(c.title).endRecord();
    out_sink.endList().field("label").value(row.label).field("title").value(row.title).endRecord();
  }
  out_sink.endList().endRecord();
}

void IrDescription::text(std::string_view name, const std::string& value, emit::ISink& out_sink) {
  if (!value.empty()) out_sink.field(name).value(value);
}

void IrDescription::refs(const std::map<std::string, std::string>& refs, emit::ISink& out_sink) {
  if (refs.empty()) return;
  out_sink.field("refs").beginRecord();
  for (const auto& [key, value] : refs) out_sink.field(key).value(value);
  out_sink.endRecord();
}

void IrDescription::texts(std::string_view name, const std::vector<std::string>& values, emit::ISink& out_sink) {
  if (values.empty()) return;
  out_sink.field(name).beginList();
  for (const std::string& v : values) out_sink.value(v);
  out_sink.endList();
}

}  // namespace format
}  // namespace ds
