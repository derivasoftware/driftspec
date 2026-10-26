#include "ds/render/Ir.hpp"

#include <map>
#include <nlohmann/json.hpp>

namespace ds {
namespace render {

namespace {

std::map<std::string, std::string> refsOf(const nlohmann::json& j) {
  std::map<std::string, std::string> out;
  auto it = j.find("refs");
  if (it == j.end() || !it->is_object()) return out;
  for (auto r = it->begin(); r != it->end(); ++r) {
    if (r.value().is_string()) out[r.key()] = r.value().get<std::string>();
  }
  return out;
}

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
    node.isAbstract = n.value("abstract", false);
    node.references = refsOf(n);
    for (const auto& section : n.value("sections", nlohmann::json::array())) {
      std::vector<std::string> lines;
      for (const auto& line : section) lines.push_back(line.get<std::string>());
      node.sections.push_back(lines);
    }
    for (const auto& k : n.value("sectionKinds", nlohmann::json::array())) node.sectionKinds.push_back(k.get<std::string>());
    if (n.contains("span") && n["span"].size() == 2) node.span = {n["span"][0].get<int>(), n["span"][1].get<int>()};
    for (const auto& d : n.value("dividers", nlohmann::json::array()))
      node.dividers.push_back({d.value("at", 0), d.value("label", std::string())});
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
    edge.order = e.value("order", -1);
    edge.references = refsOf(e);
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
}  // namespace ds
