#ifndef DRIFTSPEC_EXPORT_JSONEXPORT_HPP
#define DRIFTSPEC_EXPORT_JSONEXPORT_HPP

#include <string>
#include <utility>
#include <vector>

#include "ds/emit/Sink.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/model/Walkthrough.hpp"

namespace ds {
namespace exporter {

/// Serialises the resolved model (entities, relations, findings) as JSON, into the sink it is lent (DS-397). An
/// entity and a list of relations are written the same way alone as inside the whole model: one shape for every
/// reader. Each record writes its fields in byte order.
class JsonExport {
 public:
  /// Writes in the language of `metamodel`: the kinds by their names, and the metamodel itself (DS-341).
  explicit JsonExport(const model::Metamodel& metamodel) : metamodel_(metamodel) {}
  /// The whole model as one document, indented or compact as the sink writes.
  void render(const model::Model& model, emit::ISink& out_sink) const;
  /// One entity, as the export writes it.
  void entity(const model::Entity& entity, emit::ISink& out_sink) const;
  /// Relations, as the export writes them, in the order given.
  void relations(const std::vector<model::Relation>& relations, emit::ISink& out_sink) const;

 private:
  /// Where a model element is written, and where it ends when known: the place right after its last character (DS-172).
  static void location(const model::Location& l, emit::ISink& out_sink);
  /// One relation: its kind, ends, location and multiplicity when set.
  static void relation(const model::Relation& r, emit::ISink& out_sink);
  /// Findings or coverage: code, location, message and severity.
  void findings(const std::vector<model::Finding>& findings, emit::ISink& out_sink) const;
  /// What breaks each rule, structured (DS-108): its bindings, its witness and where.
  static void breaches(const model::Model& model, emit::ISink& out_sink);
  /// The names of kinds, in the order given.
  void kindNames(const std::vector<model::Kind>& kinds, emit::ISink& out_sink) const;
  /// The language the model is written in: every kind with its fields, flags and parents, every relation with its ends.
  void metamodel(emit::ISink& out_sink) const;
  /// Text pairs, each a list of two: the bindings and the witness of a breach.
  static void pairs(const std::vector<std::pair<std::string, std::string>>& pairs, emit::ISink& out_sink);
  /// Texts, as a list.
  static void texts(const std::vector<std::string>& texts, emit::ISink& out_sink);
  /// Every walkthrough, by identity, its steps in order (DS-471).
  static void walkthroughs(const std::vector<model::Walkthrough>& walks, emit::ISink& out_sink);

  const model::Metamodel& metamodel_;
};

}  // namespace exporter
}  // namespace ds

#endif  // DRIFTSPEC_EXPORT_JSONEXPORT_HPP
