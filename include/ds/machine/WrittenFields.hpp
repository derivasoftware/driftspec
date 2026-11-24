#ifndef DRIFTSPEC_MACHINE_WRITTENFIELDS_HPP
#define DRIFTSPEC_MACHINE_WRITTENFIELDS_HPP

#include <string>
#include <vector>

namespace ds {
namespace machine {

/// The lists the binder writes into a field of a machine and of the entities it reads (DS-367): the rules, the
/// transitions and the values of an enum are kept as joined text, so whoever reads them splits them again.
/// What a field keeps as text, this reads and writes; the grammar of the state rules (DS-384) takes it away.
class WrittenFields {
 public:
  /// The pieces of a field's text between each separator, as written: nothing is trimmed.
  static std::vector<std::string> pieces(const std::string& text, char separator);
  /// The parts joined with `with` between them, once something is written: parts empty at the start add nothing.
  static std::string joined(const std::vector<std::string>& parts, const std::string& with);
};

}  // namespace machine
}  // namespace ds

#endif  // DRIFTSPEC_MACHINE_WRITTENFIELDS_HPP
