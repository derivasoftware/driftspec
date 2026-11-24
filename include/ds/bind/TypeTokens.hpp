#ifndef DRIFTSPEC_BIND_TYPETOKENS_HPP
#define DRIFTSPEC_BIND_TYPETOKENS_HPP

#include <string>
#include <vector>

namespace ds {
namespace bind {

/// One name a type's text holds (pkg@mod::Name), with the arguments of a bracket written right after it.
struct TypeToken {
  std::string name;
  std::vector<std::string> args;

  /// The names of a type's text, left to right. The text is the code's: no name of any language is known,
  /// so `std::map<std::string, Pool*>` holds std::map (two arguments), std::string and Pool (DS-18).
  /// A dot qualifies as `::` does: `lsp.Location` is the name lsp::Location (DS-319).
  static std::vector<TypeToken> scan(const std::string& text);
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_TYPETOKENS_HPP
