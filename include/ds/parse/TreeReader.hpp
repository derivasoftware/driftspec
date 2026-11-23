#ifndef DRIFTSPEC_PARSE_TREEREADER_HPP
#define DRIFTSPEC_PARSE_TREEREADER_HPP

#include <cstring>
#include <string>
#include <vector>

#include "ds/parse/Parser.hpp"
#include "tree_sitter/api.h"

namespace ds {
namespace parse {

/// Where a node is written in its file: lines and columns from 1, its end included. The file is the reader's.
struct Span {
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
  static bool is(TSNode node, const char* type) { return std::strcmp(ts_node_type(node), type) == 0; }
  /// The child of `node` carrying `fieldName`; a null node when there is none.
  static TSNode child(TSNode node, const char* fieldName) {
    return ts_node_child_by_field_name(node, fieldName, static_cast<uint32_t>(std::strlen(fieldName)));
  }
  /// Every child carrying `fieldName` (a field may repeat: bases, modifiers).
  static std::vector<TSNode> childrenNamed(TSNode node, const char* fieldName);

  /// The text of the child carrying `fieldName`; "" when there is none.
  std::string fieldText(TSNode node, const char* fieldName) const {
    const TSNode c = child(node, fieldName);
    return ts_node_is_null(c) ? std::string() : file_.textOf(c);
  }
  /// The text a node spans.
  std::string textOf(TSNode node) const { return file_.textOf(node); }
  /// Where a node is written in the file.
  static Span spanOf(TSNode node) {
    const TSPoint start = ts_node_start_point(node);
    const TSPoint end = ts_node_end_point(node);
    return {static_cast<int>(start.row) + 1, static_cast<int>(start.column) + 1, static_cast<int>(end.row) + 1,
            static_cast<int>(end.column) + 1};
  }
  /// The tree's root.
  TSNode root() const;
  const ParsedFile& file() const { return file_; }

 private:
  const ParsedFile& file_;
};

}  // namespace parse
}  // namespace ds

#endif  // DRIFTSPEC_PARSE_TREEREADER_HPP
