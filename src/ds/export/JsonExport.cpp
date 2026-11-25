#include "ds/export/JsonExport.hpp"

#include "ds/model/Metamodel.hpp"

namespace ds {
namespace exporter {

void JsonExport::location(const model::Location& l, emit::ISink& out_sink) {
  out_sink.beginRecord().field("column").integer(l.column);
  if (l.endLine > 0) out_sink.field("end").beginRecord().field("column").integer(l.endColumn).field("line").integer(l.endLine).endRecord();
  out_sink.field("file").value(l.file).field("line").integer(l.line).endRecord();
}

void JsonExport::texts(const std::vector<std::string>& texts, emit::ISink& out_sink) {
  out_sink.beginList();
  for (const auto& t : texts) out_sink.value(t);
  out_sink.endList();
}

void JsonExport::pairs(const std::vector<std::pair<std::string, std::string>>& pairs, emit::ISink& out_sink) {
  out_sink.beginList();
  for (const auto& p : pairs) out_sink.beginList().value(p.first).value(p.second).endList();
  out_sink.endList();
}

void JsonExport::entity(const model::Entity& e, emit::ISink& out_sink) const {
  out_sink.beginRecord().field("expected").boolean(e.expected).field("fields").beginRecord();
  for (const auto& f : e.fields) out_sink.field(f.first).value(f.second);  // a std::map: in byte order
  out_sink.endRecord().field("flags");
  texts(e.flags, out_sink);
  out_sink.field("id").value(e.id).field("kind").value(metamodel_.nameOf(e.kind)).field("location");
  location(e.location, out_sink);
  out_sink.field("name").value(e.name).field("scope").value(e.scope).endRecord();
}

void JsonExport::relation(const model::Relation& r, emit::ISink& out_sink) {
  out_sink.beginRecord().field("from").value(r.from).field("kind").value(r.kind).field("location");
  location(r.location, out_sink);
  if (!r.multiplicity.empty()) out_sink.field("multiplicity").value(r.multiplicity);
  out_sink.field("to").value(r.to).endRecord();
}

void JsonExport::relations(const std::vector<model::Relation>& relations, emit::ISink& out_sink) const {
  out_sink.beginList();
  for (const auto& r : relations) relation(r, out_sink);
  out_sink.endList();
}

void JsonExport::findings(const std::vector<model::Finding>& findings, emit::ISink& out_sink) const {
  out_sink.beginList();
  for (const auto& f : findings) {
    out_sink.beginRecord().field("code").value(f.code).field("location");
    location(f.location, out_sink);
    out_sink.field("message").value(f.message).field("severity").value(metamodel_.severityOf(f.severity).name);
    if (!f.subject.empty()) out_sink.field("subject").value(f.subject);  // what it is about, when it says (DS-426)
    out_sink.endRecord();
  }
  out_sink.endList();
}

void JsonExport::breaches(const model::Model& model, emit::ISink& out_sink) {
  out_sink.beginRecord();
  for (const auto& rule : model.breaches()) {  // a std::map: in byte order
    out_sink.field(rule.first).beginList();
    for (const auto& b : rule.second) {
      out_sink.beginRecord().field("bindings");
      pairs(b.bindings, out_sink);
      out_sink.field("location");
      location(b.location, out_sink);
      out_sink.field("witness");
      pairs(b.witness, out_sink);
      out_sink.endRecord();
    }
    out_sink.endList();
  }
  out_sink.endRecord();
}

void JsonExport::kindNames(const std::vector<model::Kind>& kinds, emit::ISink& out_sink) const {
  out_sink.beginList();
  for (model::Kind k : kinds) out_sink.value(metamodel_.nameOf(k));
  out_sink.endList();
}

void JsonExport::metamodel(emit::ISink& out_sink) const {
  out_sink.beginRecord().field("kinds").beginList();
  for (const auto& k : metamodel_.kinds()) {
    // its fields in byte order: what the kind is part of first (DS-427)
    out_sink.beginRecord().field("category").value(model::Metamodel::categoryName(k.category)).field("fields").beginList();
    for (const auto& f : k.fields) {
      out_sink.beginRecord().field("meaning").value(f.meaning).field("name").value(f.name);
      out_sink.field("source").value(metamodel_.fieldSourceName(f.source)).field("type").value(metamodel_.fieldTypeName(f.type));
      out_sink.field("values");
      texts(f.values, out_sink);
      out_sink.endRecord();
    }
    out_sink.endList().field("flags");
    texts(k.flags, out_sink);
    out_sink.field("kind").value(metamodel_.nameOf(k.kind)).field("meaning").value(k.meaning).field("parents");
    kindNames(k.parents, out_sink);
    out_sink.field("top").boolean(k.top).endRecord();
  }
  out_sink.endList().field("relations").beginList();
  for (const auto& r : metamodel_.relations()) {
    out_sink.beginRecord().field("from");
    kindNames(r.from, out_sink);
    out_sink.field("kind").value(r.kind).field("meaning").value(r.meaning);
    if (!r.pairs.empty()) {  // the pairs of kinds it may join, when it says them
      out_sink.field("pairs").beginList();
      for (const auto& p : r.pairs) out_sink.beginList().value(metamodel_.nameOf(p.first)).value(metamodel_.nameOf(p.second)).endList();
      out_sink.endList();
    }
    out_sink.field("to");
    kindNames(r.to, out_sink);
    out_sink.endRecord();
  }
  out_sink.endList().endRecord();
}

void JsonExport::render(const model::Model& model, emit::ISink& out_sink) const {
  // what breaks each rule, structured (DS-108): a reader lights it without reading the findings' prose
  out_sink.beginRecord().field("breaches");
  breaches(model, out_sink);
  out_sink.field("coverage");
  findings(model.coverage(), out_sink);
  out_sink.field("entities").beginList();
  for (const auto& pair : model.entities()) entity(pair.second, out_sink);
  out_sink.endList().field("findings");
  findings(model.findings(), out_sink);
  // who wrote it and in which shape: a reader can tell a document it does not know (DS-278)
  out_sink.field("format").integer(1).field("metamodel");
  metamodel(out_sink);
  out_sink.field("relations");
  relations(model.index().all(), out_sink);
  out_sink.field("version").value(metamodel_.version()).endRecord();
}

}  // namespace exporter
}  // namespace ds
