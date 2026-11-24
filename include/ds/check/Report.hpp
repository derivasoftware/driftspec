#ifndef DRIFTSPEC_CHECK_REPORT_HPP
#define DRIFTSPEC_CHECK_REPORT_HPP

#include <cstddef>
#include <vector>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace check {

/// What a check of a workspace says (DS-120, DS-126): its findings in order, and the counts of the
/// model they were found in. Built once from the model; its formats write it without the model.
struct Report {
  std::vector<model::Finding> findings;
  std::vector<model::Finding> coverage;  ///< what is not yet written: no finding, answered apart (DS-216)
  int entities = 0;                      ///< declared: what the design says
  int expected = 0;                      ///< named but declared nowhere
  int relations = 0;
  int errors = 0;                               ///< findings of a severity that fails: what makes the check fail
  std::vector<model::SeveritySpec> severities;  ///< what each severity means, copied from the metamodel (DS-379)

  /// The report of a checked model, written in `metamodel`.
  static Report of(const model::Model& model, const model::Metamodel& metamodel);
  /// What a severity means, as the metamodel the report was built in says.
  const model::SeveritySpec& severityOf(model::Severity severity) const { return severities[static_cast<std::size_t>(severity)]; }
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_REPORT_HPP
