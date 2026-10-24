#include "driftspec/export/JsonExport.hpp"

#include <nlohmann/json.hpp>

namespace driftspec {
namespace exporter {

namespace {

nlohmann::json locationJson(const model::Location& l) { return {{"file", l.file}, {"line", l.line}, {"column", l.column}}; }

}  // namespace

std::string JsonExport::render(const model::Model& model, bool pretty) const {
  nlohmann::json out;
  out["entities"] = nlohmann::json::array();
  for (const auto& pair : model.entities()) {
    const model::Entity& e = pair.second;
    out["entities"].push_back({{"id", e.id}, {"kind", model::kindName(e.kind)}, {"name", e.name}, {"scope", e.scope}, {"expected", e.expected},
                               {"fields", e.fields}, {"flags", e.flags}, {"location", locationJson(e.location)}});
  }
  out["relations"] = nlohmann::json::array();
  for (const auto& r : model.relations()) {
    nlohmann::json rel = {{"kind", r.kind}, {"from", r.from}, {"to", r.to}, {"location", locationJson(r.location)}};
    if (!r.multiplicity.empty()) rel["multiplicity"] = r.multiplicity;
    out["relations"].push_back(rel);
  }
  out["findings"] = nlohmann::json::array();
  for (const auto& f : model.findings()) {
    out["findings"].push_back({{"code", f.code}, {"severity", f.severity}, {"message", f.message}, {"location", locationJson(f.location)}});
  }
  return pretty ? out.dump(2) : out.dump();
}

}  // namespace exporter
}  // namespace driftspec
