#ifndef DRIFTSPEC_PARSE_SYNTAXERROR_HPP
#define DRIFTSPEC_PARSE_SYNTAXERROR_HPP

#include <string>

#include "ds/parse/Parser.hpp"
#include "tree_sitter/api.h"

namespace ds {
namespace parse {

/// The first place a tree does not parse (REQ-00080-1, DS-374): an ERROR node, text the grammar did not expect, or a
/// MISSING one, a token it had to invent, found in document order; where it sits, in the reader's words, and what the
/// grammar takes there.
class SyntaxError {
 public:
  /// Describes the errors of a tree parsed from `text`, which it reads and does not keep beyond itself.
  explicit SyntaxError(const std::string& text) : text_(text) {}

  /// Writes into `file` the place of the first error under `node` and what is wrong there; false when there is none.
  bool place(TSNode node, ParsedFile& file) const;

 private:
  /// Where an error sits: the nearest declaration around it.
  static std::string contextOf(TSNode node);
  /// What the grammar would have taken where the error starts, joined; "" when recovery lost its place.
  static std::string expectedAt(TSNode node);

  const std::string& text_;
};

}  // namespace parse
}  // namespace ds

#endif  // DRIFTSPEC_PARSE_SYNTAXERROR_HPP
