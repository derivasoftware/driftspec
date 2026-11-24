#ifndef DRIFTSPEC_EXPORT_JSONEXPORT_HPP
#define DRIFTSPEC_EXPORT_JSONEXPORT_HPP

#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace exporter {

/// Serialises the resolved model (entities, relations, findings) as JSON. An entity and a list of
/// relations are written the same way alone as inside the whole model: one shape for every reader.
class JsonExport {
 public:
  /// Writes in the language of `metamodel`: the kinds by their names, and the metamodel itself (DS-341).
  explicit JsonExport(const model::Metamodel& metamodel) : metamodel_(metamodel) {}
  std::string render(const model::Model& model, bool pretty = true) const;
  /// One entity, as the export writes it.
  std::string entity(const model::Entity& entity) const;
  /// Relations, as the export writes them, in the order given.
  std::string relations(const std::vector<model::Relation>& relations) const;

 private:
  /// Where a model element is written, and where it ends when known: the place right after its last character (DS-172).
  static nlohmann::json locationJson(const model::Location& l);
  /// One entity: its identity, kind, name, scope, fields, flags and location.
  nlohmann::json entityJson(const model::Entity& e) const;
  /// One relation: its kind, ends, location and multiplicity when set.
  static nlohmann::json relationJson(const model::Relation& r);
  /// The names of kinds, in the order given.
  nlohmann::json kindNames(const std::vector<model::Kind>& kinds) const;
  /// The language the model is written in: every kind with its fields, flags and parents, every relation with its ends.
  nlohmann::json metamodelJson() const;

  const model::Metamodel& metamodel_;
};

}  // namespace exporter
}  // namespace ds

#endif  // DRIFTSPEC_EXPORT_JSONEXPORT_HPP
