#include "driftspec/render/Ir.hpp"

#include <map>
#include <nlohmann/json.hpp>

namespace driftspec {
namespace render {

namespace {

std::string text(const nlohmann::json& j, const char* key) {
  auto it = j.find(key);
  return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
}

}  // namespace

IrDocument IrDocument::parse(const std::string& json) {
  const nlohmann::json j = nlohmann::json::parse(json);
  IrDocument doc;
  doc.title = text(j, "title");
  doc.notice = text(j, "notice");
  for (const auto& n : j.value("nodes", nlohmann::json::array())) {
    IrNode node;
    node.id = text(n, "id");
    node.kind = text(n, "kind");
    node.label = text(n, "label");
    node.classifier = text(n, "classifier");
    node.stereotype = text(n, "stereotype");
    node.parent = text(n, "parent");
    node.title = text(n, "title");
    node.href = text(n, "href");
    node.abstract = n.value("abstract", false);
    for (const auto& section : n.value("sections", nlohmann::json::array())) {
      std::vector<std::string> lines;
      for (const auto& line : section) lines.push_back(line.get<std::string>());
      node.sections.push_back(lines);
    }
    for (const auto& k : n.value("sectionKinds", nlohmann::json::array())) node.sectionKinds.push_back(k.get<std::string>());
    doc.nodes.push_back(node);
  }
  for (const auto& e : j.value("edges", nlohmann::json::array())) {
    IrEdge edge;
    edge.from = text(e, "from");
    edge.to = text(e, "to");
    edge.kind = text(e, "kind");
    edge.label = text(e, "label");
    edge.title = text(e, "title");
    edge.href = text(e, "href");
    edge.dashed = e.value("dashed", false);
    doc.edges.push_back(edge);
  }
  return doc;
}

IrDocument IrDocument::bundled() const {
  IrDocument out;
  out.title = title;
  out.notice = notice;
  out.nodes = nodes;
  std::map<std::string, size_t> seen;
  for (const auto& e : edges) {
    const std::string key = e.from + '\0' + e.to + '\0' + e.kind + '\0' + (e.dashed ? "1" : "0") + '\0' + e.href;
    auto it = seen.find(key);
    if (it == seen.end()) {
      seen[key] = out.edges.size();
      out.edges.push_back(e);
      continue;
    }
    IrEdge& first = out.edges[it->second];
    if (!e.label.empty()) first.label += (first.label.empty() ? "" : "\n") + e.label;
    if (!e.title.empty()) first.title += (first.title.empty() ? "" : "\n") + e.title;
  }
  return out;
}

}  // namespace render
}  // namespace driftspec
