#ifndef DRIFTSPEC_PARSE_TREEREADER_HPP
#define DRIFTSPEC_PARSE_TREEREADER_HPP

#include <string>
#include <vector>

#include "ds/parse/Parser.hpp"
#include "tree_sitter/api.h"

namespace ds {
namespace parse {

/// Where a node of a file is written: lines and columns from 1, its end included.
struct Span {
  std::string file;
  int line = 0;
  int column = 0;
  int endLine = 0;
  int endColumn = 0;
};

/// The syntax tree of one parsed file, read (DS-354): what a node is, its fields and their text, where it is written.
/// What binds the model asks this; nothing here knows the model.
class TreeReader {
 public:
  explicit TreeReader(const ParsedFile& file) : file_(file) {}

  /// Whether `node` is of the grammar's `type`.
  static bool is(TSNode node, const char* type);
  /// The child of `node` carrying `fieldName`; a null node when there is none.
  static TSNode child(TSNode node, const char* fieldName);
  /// Every child carrying `fieldName` (a field may repeat: bases, modifiers).
  static std::vector<TSNode> childrenNamed(TSNode node, const char* fieldName);

  /// The text of the child carrying `fieldName`; "" when there is none.
  std::string fieldText(TSNode node, const char* fieldName) const;
  /// The text a node spans.
  std::string textOf(TSNode node) const;
  /// Where a node is written in the file.
  Span spanOf(TSNode node) const;
  /// The tree's root.
  TSNode root() const;
  const ParsedFile& file() const { return file_; }

 private:
  const ParsedFile& file_;
};

}  // namespace parse
}  // namespace ds

#endif  // DRIFTSPEC_PARSE_TREEREADER_HPP
