#include "ds/export/JsonExport.hpp"

#include <nlohmann/json.hpp>

#include "ds/model/Metamodel.hpp"

namespace ds {
namespace exporter {

namespace {

// where it starts, and where it ends when known: the place right after its last character (DS-172)
nlohmann::json locationJson(const model::Location& l) {
  nlohmann::json out = {{"file", l.file}, {"line", l.line}, {"column", l.column}};
  if (l.endLine > 0) out["end"] = {{"line", l.endLine}, {"column", l.endColumn}};
  return out;
}

nlohmann::json entityJson(const model::Entity& e) {
  return {{"id", e.id},
          {"kind", model::kindName(e.kind)},
          {"name", e.name},
          {"scope", e.scope},
          {"expected", e.expected},
          {"fields", e.fields},
          {"flags", e.flags},
          {"location", locationJson(e.location)}};
}

nlohmann::json relationJson(const model::Relation& r) {
  nlohmann::json rel = {{"kind", r.kind}, {"from", r.from}, {"to", r.to}, {"location", locationJson(r.location)}};
  if (!r.multiplicity.empty()) rel["multiplicity"] = r.multiplicity;
  return rel;
}

nlohmann::json kindNames(const std::vector<model::Kind>& kinds) {
  nlohmann::json out = nlohmann::json::array();
  for (model::Kind k : kinds) out.push_back(model::kindName(k));
  return out;
}

// the language the model is written in: every kind with its fields, flags and parents, every relation with its ends
nlohmann::json metamodelJson() {
  nlohmann::json out;
  out["kinds"] = nlohmann::json::array();
  for (const auto& k : model::kinds()) {
    nlohmann::json fields = nlohmann::json::array();
    for (const auto& f : k.fields) {
      fields.push_back({{"name", f.name},
                        {"type", model::fieldTypeName(f.type)},
                        {"source", model::fieldSourceName(f.source)},
                        {"meaning", f.meaning},
                        {"values", f.values}});
    }
    out["kinds"].push_back({{"kind", model::kindName(k.kind)},
                            {"meaning", k.meaning},
                            {"fields", fields},
                            {"flags", k.flags},
                            {"parents", kindNames(k.parents)},
                            {"top", k.top}});
  }
  out["relations"] = nlohmann::json::array();
  for (const auto& r : model::relations()) {
    nlohmann::json spec = {{"kind", r.kind}, {"meaning", r.meaning}, {"from", kindNames(r.from)}, {"to", kindNames(r.to)}};
    if (!r.pairs.empty()) {
      spec["pairs"] = nlohmann::json::array();
      for (const auto& p : r.pairs) spec["pairs"].push_back({model::kindName(p.first), model::kindName(p.second)});
    }
    out["relations"].push_back(spec);
  }
  return out;
}

}  // namespace

std::string JsonExport::render(const model::Model& model, bool pretty) const {
  nlohmann::json out;
  out["entities"] = nlohmann::json::array();
  for (const auto& pair : model.entities()) out["entities"].push_back(entityJson(pair.second));
  out["relations"] = nlohmann::json::array();
  for (const auto& r : model.index().all()) out["relations"].push_back(relationJson(r));
  out["coverage"] = nlohmann::json::array();
  for (const auto& f : model.coverage())
    out["coverage"].push_back({{"code", f.code}, {"severity", f.severity}, {"message", f.message}, {"location", locationJson(f.location)}});
  out["findings"] = nlohmann::json::array();
  for (const auto& f : model.findings()) {
    out["findings"].push_back({{"code", f.code}, {"severity", f.severity}, {"message", f.message}, {"location", locationJson(f.location)}});
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
