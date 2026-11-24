#include "ds/export/JsonExport.hpp"

#include <nlohmann/json.hpp>

#include "ds/model/Metamodel.hpp"

namespace ds {
namespace exporter {

nlohmann::json JsonExport::locationJson(const model::Location& l) {
  nlohmann::json out = {{"file", l.file}, {"line", l.line}, {"column", l.column}};
  if (l.endLine > 0) out["end"] = {{"line", l.endLine}, {"column", l.endColumn}};
  return out;
}

nlohmann::json JsonExport::entityJson(const model::Entity& e) const {
  return {{"id", e.id},
          {"kind", metamodel_.nameOf(e.kind)},
          {"name", e.name},
          {"scope", e.scope},
          {"expected", e.expected},
          {"fields", e.fields},
          {"flags", e.flags},
          {"location", locationJson(e.location)}};
}

nlohmann::json JsonExport::relationJson(const model::Relation& r) {
  nlohmann::json rel = {{"kind", r.kind}, {"from", r.from}, {"to", r.to}, {"location", locationJson(r.location)}};
  if (!r.multiplicity.empty()) rel["multiplicity"] = r.multiplicity;
  return rel;
}

nlohmann::json JsonExport::kindNames(const std::vector<model::Kind>& kinds) const {
  nlohmann::json out = nlohmann::json::array();
  for (model::Kind k : kinds) out.push_back(metamodel_.nameOf(k));
  return out;
}

nlohmann::json JsonExport::metamodelJson() const {
  nlohmann::json out;
  out["kinds"] = nlohmann::json::array();
  for (const auto& k : metamodel_.kinds()) {
    nlohmann::json fields = nlohmann::json::array();
    for (const auto& f : k.fields) {
      fields.push_back({{"name", f.name},
                        {"type", metamodel_.fieldTypeName(f.type)},
                        {"source", metamodel_.fieldSourceName(f.source)},
                        {"meaning", f.meaning},
                        {"values", f.values}});
    }
    out["kinds"].push_back({{"kind", metamodel_.nameOf(k.kind)},
                            {"meaning", k.meaning},
                            {"fields", fields},
                            {"flags", k.flags},
                            {"parents", kindNames(k.parents)},
                            {"top", k.top}});
  }
  out["relations"] = nlohmann::json::array();
  for (const auto& r : metamodel_.relations()) {
    nlohmann::json spec = {{"kind", r.kind}, {"meaning", r.meaning}, {"from", kindNames(r.from)}, {"to", kindNames(r.to)}};
    if (!r.pairs.empty()) {
      spec["pairs"] = nlohmann::json::array();
      for (const auto& p : r.pairs) spec["pairs"].push_back({metamodel_.nameOf(p.first), metamodel_.nameOf(p.second)});
    }
    out["relations"].push_back(spec);
  }
  return out;
}

std::string JsonExport::render(const model::Model& model, bool pretty) const {
  nlohmann::json out;
  // who wrote it and in which shape: a reader can tell a document it does not know (DS-278)
  out["version"] = metamodel_.version();
  out["format"] = 1;
  out["entities"] = nlohmann::json::array();
  for (const auto& pair : model.entities()) out["entities"].push_back(entityJson(pair.second));
  out["relations"] = nlohmann::json::array();
  for (const auto& r : model.index().all()) out["relations"].push_back(relationJson(r));
  out["coverage"] = nlohmann::json::array();
  for (const auto& f : model.coverage())
    out["coverage"].push_back({{"code", f.code},
                               {"severity", metamodel_.severityOf(f.severity).name},
                               {"message", f.message},
                               {"location", locationJson(f.location)}});
  out["findings"] = nlohmann::json::array();
  for (const auto& f : model.findings()) {
    out["findings"].push_back({{"code", f.code},
                               {"severity", metamodel_.severityOf(f.severity).name},
                               {"message", f.message},
                               {"location", locationJson(f.location)}});
  }
  // what breaks each rule, structured (DS-108): a reader lights it without reading the findings' prose
  out["breaches"] = nlohmann::json::object();
  for (const auto& rule : model.breaches()) {
    nlohmann::json list = nlohmann::json::array();
    for (const auto& b : rule.second)
      list.push_back({{"bindings", b.bindings}, {"witness", b.witness}, {"location", locationJson(b.location)}});
    out["breaches"][rule.first] = list;
  }
  out["metamodel"] = metamodelJson();
  return pretty ? out.dump(2) : out.dump();
}

std::string JsonExport::entity(const model::Entity& entity) const { return entityJson(entity).dump(); }

std::string JsonExport::relations(const std::vector<model::Relation>& relations) const {
  nlohmann::json out = nlohmann::json::array();
  for (const auto& r : relations) out.push_back(relationJson(r));
  return out.dump();
}

}  // namespace exporter
}  // namespace ds
