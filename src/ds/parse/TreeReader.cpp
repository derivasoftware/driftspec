#include "ds/parse/TreeReader.hpp"

#include <cstring>
#include <stdexcept>
#include <string>

namespace ds {
namespace parse {

const char* TreeReader::nameOf(Field field) {
  // in the order of Field, which is the grammar's
  static const char* const names[] = {
      "action",   "base",      "body",      "bound",    "call",       "code",    "condition", "default",  "definition", "else_scenario",
      "entity",   "event",     "fails",     "failure",  "family",     "field",   "from",      "guard",    "id",         "key",
      "kind",     "left",      "many",      "modifier", "name",       "negated", "of",        "op",       "operator",   "operators",
      "optional", "otherwise", "ownership", "package",  "parameter",  "params",  "prefix",    "realises", "relation",   "result",
      "returns",  "right",     "scenario",  "scope",    "show",       "subject", "symbol",    "target",   "text",       "title",
      "to",       "type",      "value",     "variable", "visibility",
  };
  return names[static_cast<std::size_t>(field)];
}

std::vector<TSFieldId> TreeReader::resolve() {
  const TSLanguage* language = Parser::language();
  std::vector<TSFieldId> out;
  for (std::size_t i = 0; i <= static_cast<std::size_t>(Field::visibility); ++i) {
    const char* name = nameOf(static_cast<Field>(i));
    const TSFieldId id = ts_language_field_id_for_name(language, name, static_cast<uint32_t>(std::strlen(name)));
    if (id == 0) throw std::logic_error(std::string("the grammar has no field ") + name);
    out.push_back(id);
  }
  return out;
}

std::vector<TSNode> TreeReader::childrenNamed(TSNode node, Field field) {
  std::vector<TSNode> out;
  const TSFieldId id = idOf(field);
  TSTreeCursor cursor = ts_tree_cursor_new(node);
  if (ts_tree_cursor_goto_first_child(&cursor)) {
    do {
      if (ts_tree_cursor_current_field_id(&cursor) == id) out.push_back(ts_tree_cursor_current_node(&cursor));
    } while (ts_tree_cursor_goto_next_sibling(&cursor));
  }
  ts_tree_cursor_delete(&cursor);
  return out;
}

TSNode TreeReader::root() const { return ts_tree_root_node(file_.tree.get()); }

}  // namespace parse
}  // namespace ds
