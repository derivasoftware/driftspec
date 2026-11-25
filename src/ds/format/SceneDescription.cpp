#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "ds/format/Descriptions.hpp"
#include "ds/model/Identity.hpp"
#include "ds/scene/Map.hpp"

namespace ds {
namespace format {

SceneDescription::SceneDescription(const scene::SceneDocument& document) : document_(document), landing_(document.nodes) {
  for (size_t i = 0; i < document.nodes.size(); ++i) nodeAt_[document.nodes[i].id] = static_cast<int>(i);
  for (size_t i = 0; i < document.behaviours.size(); ++i) behaviourAt_[document.behaviours[i].id] = static_cast<int>(i);
  for (size_t i = 0; i < document.requirements.size(); ++i) requirementAt_[document.requirements[i].id] = static_cast<int>(i);
}

int SceneDescription::node(const std::string& id) const {
  const auto at = nodeAt_.find(id);
  return at == nodeAt_.end() ? -1 : at->second;
}

int SceneDescription::requirement(const std::string& id) const {
  const auto at = requirementAt_.find(id);
  return at == requirementAt_.end() ? -1 : at->second;
}

int SceneDescription::behaviourOf(const std::string& sequence) const {
  const std::string scenario = model::Identity(sequence).scenario();
  const auto at = behaviourAt_.find(landing_.land(model::Identity(sequence).subject().text()) + (scenario.empty() ? "" : " / " + scenario));
  return at == behaviourAt_.end() ? -1 : at->second;
}

void SceneDescription::box(const layout::Box& b, emit::ISink& out_sink) {
  out_sink.beginList().integer(std::lround(b.x)).integer(std::lround(b.y)).integer(std::lround(b.w)).integer(std::lround(b.h)).endList();
}

void SceneDescription::texts(const std::vector<std::string>& texts, emit::ISink& out_sink) {
  out_sink.beginList();
  for (const auto& t : texts) out_sink.value(t);
  out_sink.endList();
}

void SceneDescription::pairs(const std::vector<std::pair<std::string, std::string>>& pairs, emit::ISink& out_sink) {
  out_sink.beginList();
  for (const auto& p : pairs) out_sink.beginList().value(p.first).value(p.second).endList();
  out_sink.endList();
}

// The document says how to read itself: every record is an array whose columns "fields" names, and a node, a
// behaviour or a link is named by its index. The keys of the one record with keys, the document itself, are
// written in byte order.
void SceneDescription::describe(emit::ISink& out_sink) const {
  const scene::SceneDocument& doc = document_;
  out_sink.beginRecord().field("behaviours").beginList();
  for (const auto& b : doc.behaviours) {
    out_sink.beginList().integer(node(b.subject)).value(b.scenario).beginList();
    for (const auto& c : b.calls) out_sink.beginList().integer(node(c.to)).value(c.label).integer(c.expands).value(c.result).endList();
    out_sink.endList().endList();
  }
  out_sink.endList().field("diagrams").beginList();
  for (const auto& d : doc.diagrams) {
    out_sink.beginList().value(d.name).value(d.kind).beginList();
    for (const auto& n : d.nodes) out_sink.integer(node(n));
    out_sink.endList().endList();
  }
  out_sink.endList().field("fields").beginRecord();
  out_sink.field("behaviour");
  texts({"subject", "scenario", "calls"}, out_sink);
  out_sink.field("breach");
  texts({"node", "to", "bindings", "witness", "file", "line"}, out_sink);
  out_sink.field("call");
  texts({"to", "label", "behaviour", "result"}, out_sink);
  out_sink.field("diagram");
  texts({"name", "kind", "nodes"}, out_sink);
  out_sink.field("link");
  texts({"from", "to", "kind", "count", "multiplicity", "behaviours"}, out_sink);
  out_sink.field("node");
  texts({"id", "kind", "parent", "label", "stereotype", "visibility", "requirements", "full", "lean"}, out_sink);
  out_sink.field("requirement");
  texts({"id", "kind", "title", "status", "text", "parent", "scope", "nodes", "signature", "outcomes", "binds"}, out_sink);
  out_sink.field("rule");
  texts({"id", "kind", "name", "title", "scope", "lines", "requirements", "breaches", "file", "line"}, out_sink);
  out_sink.field("tour");
  texts({"behaviour", "group"}, out_sink);
  out_sink.field("verification");
  texts({"id", "title", "status", "method", "tests", "requirements"}, out_sink);
  out_sink.endRecord().field("lean_world");
  box(doc.leanWorld, out_sink);
  out_sink.field("links").beginList();
  for (const auto& l : doc.links) {
    out_sink.beginList().integer(node(l.from)).integer(node(l.to)).value(l.kind).integer(l.count).value(l.multiplicity).beginList();
    for (const auto& s : l.sequences) {  // a sequence id, X::sequence or X::sequence/scenario, as the behaviour it is
      const int b = behaviourOf(s);
      if (b >= 0) out_sink.integer(b);
    }
    out_sink.endList().endList();
  }
  out_sink.endList().field("nodes").beginList();
  for (const auto& n : doc.nodes) {
    out_sink.beginList().value(n.id).value(n.kind).integer(node(n.parent)).value(n.label).value(n.stereotype).value(n.visibility);
    texts(n.requirements, out_sink);
    box(n.full, out_sink);
    if (n.lean)
      box(n.leanBox, out_sink);
    else
      out_sink.null();
    out_sink.endList();
  }
  out_sink.endList().field("requirements").beginList();
  for (const auto& r : doc.requirements) {
    out_sink.beginList().value(r.id).value(r.kind).value(r.title).value(r.status).value(r.text).integer(requirement(r.parent));
    out_sink.integer(node(r.scope)).beginList();
    for (const auto& n : r.nodes) out_sink.integer(node(n));
    out_sink.endList().value(r.signature).beginList();
    for (const auto& o : r.outcomes) out_sink.beginList().value(o.name).value(o.returns).value(o.text).endList();
    out_sink.endList().beginList();
    for (const auto& b : r.binds)
      if (requirement(b) >= 0) out_sink.integer(requirement(b));
    out_sink.endList().endList();
  }
  out_sink.endList().field("rules").beginList();
  for (const auto& r : doc.rules) {
    out_sink.beginList().value(r.id).value(r.kind).value(r.name).value(r.title).integer(node(r.scope));
    pairs(r.lines, out_sink);
    texts(r.requirements, out_sink);
    out_sink.beginList();
    for (const auto& b : r.breaches) {
      out_sink.beginList().integer(node(b.node)).integer(node(b.to));
      pairs(b.bindings, out_sink);
      pairs(b.witness, out_sink);
      out_sink.value(b.file).integer(b.line).endList();
    }
    out_sink.endList().value(r.file).integer(r.line).endList();
  }
  out_sink.endList().field("titles").beginRecord();
  for (const auto& t : doc.titles) out_sink.field(t.first).value(t.second);  // a std::map: in byte order
  out_sink.endRecord().field("tours").beginList();
  for (const auto& t : doc.tours) out_sink.beginList().integer(t.behaviour).value(t.group).endList();
  out_sink.endList().field("verifications").beginList();
  for (const auto& v : doc.verifications) {
    out_sink.beginList().value(v.id).value(v.title).value(v.status).value(v.method);
    texts(v.tests, out_sink);
    out_sink.beginList();
    for (const auto& r : v.requirements)
      if (requirement(r) >= 0) out_sink.integer(requirement(r));
    out_sink.endList().endList();
  }
  out_sink.endList().field("version").integer(2).field("world");
  box(doc.world, out_sink);
  out_sink.endRecord();
}

}  // namespace format
}  // namespace ds
