#ifndef DRIFTSPEC_MACHINE_LITERAL_HPP
#define DRIFTSPEC_MACHINE_LITERAL_HPP

#include <string>

namespace ds {
namespace machine {

/// One literal of a state rule (DS-249): a name of the context, alone or compared with a value; `not` turns it round.
struct Literal {
  bool negated = false;
  std::string name;
  std::string op;  ///< "=", "!=", or empty for a name alone
  std::string value;

  /// A literal as the binder writes it: `not` kept as a leading !, then the name, alone or with = or != and a value.
  static Literal read(std::string text);
  /// The literal as a reader is told it: not x, x = v, x != v.
  std::string written() const;
};

}  // namespace machine
}  // namespace ds

#endif  // DRIFTSPEC_MACHINE_LITERAL_HPP
