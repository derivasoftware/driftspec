#ifndef DRIFTSPEC_EXPORT_JSONEXPORT_HPP
#define DRIFTSPEC_EXPORT_JSONEXPORT_HPP

#include <string>

#include "driftspec/model/Model.hpp"

namespace driftspec {
namespace exporter {

/// Serialises the resolved model (entities, relations, findings) as JSON.
class JsonExport {
 public:
  std::string render(const model::Model& model, bool pretty = true) const;
};

}  // namespace exporter
}  // namespace driftspec

#endif  // DRIFTSPEC_EXPORT_JSONEXPORT_HPP
