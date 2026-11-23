#ifndef DRIFTSPEC_PARSE_TREEREADER_HPP
#define DRIFTSPEC_PARSE_TREEREADER_HPP

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include "ds/parse/Field.hpp"
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
  /// The child of `node` in `field`; a null node when there is none. The field is read by its id (DS-360).
  static TSNode child(TSNode node, Field field) { return ts_node_child_by_field_id(node, idOf(field)); }
  /// Every child in `field` (a field may repeat: bases, modifiers).
  static std::vector<TSNode> childrenNamed(TSNode node, Field field);
  /// The name grammar.js gives `field`: "name", "else_scenario".
  static const char* nameOf(Field field);

  /// The text of the child in `field`; "" when there is none.
  std::string fieldText(TSNode node, Field field) const {
    const TSNode c = child(node, field);
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
  /// The grammar's id of `field`, from the table resolved once.
  static TSFieldId idOf(Field field) { return ids()[static_cast<std::size_t>(field)]; }
  /// The id of every field, by its place in Field, resolved from the names the first time a field is read.
  static const std::vector<TSFieldId>& ids() {
    static const std::vector<TSFieldId> table = resolve();
    return table;
  }
  /// Each field's id, looked up by its name in the grammar.
  static std::vector<TSFieldId> resolve();

  const ParsedFile& file_;
};

}  // namespace parse
}  // namespace ds

#endif  // DRIFTSPEC_PARSE_TREEREADER_HPP
