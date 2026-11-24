#include "ds/check/Report.hpp"

namespace ds {
namespace check {

Report Report::of(const model::Model& model, const model::Metamodel& metamodel) {
  Report out;
  out.findings = model.findings();
  out.coverage = model.coverage();
  out.severities = metamodel.severities();
  for (const auto& f : out.findings) out.errors += out.severityOf(f.severity).fails ? 1 : 0;
  for (const auto& e : model.entities()) out.expected += e.second.expected ? 1 : 0;
  out.entities = static_cast<int>(model.entities().size()) - out.expected;
  out.relations = static_cast<int>(model.index().count());
  return out;
}

}  // namespace check
}  // namespace ds
