#ifndef DRIFTSPEC_PARSE_LOOKAHEAD_HPP
#define DRIFTSPEC_PARSE_LOOKAHEAD_HPP

#include <string>
#include <vector>

#include "tree_sitter/api.h"

namespace ds {
namespace parse {

/// What the grammar takes in one parse state (DS-84, DS-374): its keywords, and whether it takes an annotation and a
/// field written key: value. What a syntax error says the grammar takes and what an editor completes with read it here.
class Lookahead {
 public:
  /// Reads the state `state` of `language`; a state the grammar cannot iterate takes nothing.
  Lookahead(const TSLanguage& language, TSStateId state);

  /// The keywords it takes, each once, in alphabetical order.
  const std::vector<std::string>& keywords() const { return keywords_; }
  /// The keywords, then "@annotation" and "key: value" when it takes those.
  std::vector<std::string> words() const;

 private:
  std::vector<std::string> keywords_;
  bool annotation_ = false;
  bool field_ = false;
};

}  // namespace parse
}  // namespace ds

#endif  // DRIFTSPEC_PARSE_LOOKAHEAD_HPP
