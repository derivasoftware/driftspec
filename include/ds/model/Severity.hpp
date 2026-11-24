#ifndef DRIFTSPEC_MODEL_SEVERITY_HPP
#define DRIFTSPEC_MODEL_SEVERITY_HPP

#include <cstddef>

namespace ds {
namespace model {

/// How much a finding weighs (DS-379). What each one means, its printed name, whether it fails the check, whether it
/// is kept apart and what an editor shows it as, is a row of the metamodel's severity table: a new severity is one
/// value here and one row there, and every reader asks the row.
enum class Severity : std::size_t { error, warning, coverage };

}  // namespace model
}  // namespace ds

#endif  // DRIFTSPEC_MODEL_SEVERITY_HPP
