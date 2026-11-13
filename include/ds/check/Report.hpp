#ifndef DRIFTSPEC_CHECK_REPORT_HPP
#define DRIFTSPEC_CHECK_REPORT_HPP

#include <vector>

#include "ds/model/Model.hpp"

namespace ds {
namespace check {

/// What a check of a workspace says (DS-120, DS-126): its findings in order, and the counts of the
/// model they were found in. Built once from the model; its formats write it without the model.
struct Report {
  std::vector<model::Finding> findings;
  int entities = 0;  ///< declared: what the design says
  int expected = 0;  ///< named but declared nowhere
  int relations = 0;
  int errors = 0;  ///< findings of severity error: what makes the check fail

  /// The report of a checked model.
  static Report of(const model::Model& model);
};

}  // namespace check
}  // namespace ds

#endif  // DRIFTSPEC_CHECK_REPORT_HPP
