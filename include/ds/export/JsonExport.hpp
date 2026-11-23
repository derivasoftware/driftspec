#ifndef DRIFTSPEC_EXPORT_JSONEXPORT_HPP
#define DRIFTSPEC_EXPORT_JSONEXPORT_HPP

#include <string>

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
  const model::Metamodel& metamodel_;
};

}  // namespace exporter
}  // namespace ds

#endif  // DRIFTSPEC_EXPORT_JSONEXPORT_HPP
