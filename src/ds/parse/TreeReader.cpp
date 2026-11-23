#include "ds/parse/TreeReader.hpp"

#include <cstring>

namespace ds {
namespace parse {

bool TreeReader::is(TSNode node, const char* type) { return std::strcmp(ts_node_type(node), type) == 0; }

TSNode TreeReader::child(TSNode node, const char* fieldName) {
  return ts_node_child_by_field_name(node, fieldName, static_cast<uint32_t>(std::strlen(fieldName)));
}

std::vector<TSNode> TreeReader::childrenNamed(TSNode node, const char* fieldName) {
  std::vector<TSNode> out;
  const uint32_t count = ts_node_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    const char* f = ts_node_field_name_for_child(node, i);
    if (f && std::strcmp(f, fieldName) == 0) out.push_back(ts_node_child(node, i));
  }
  return out;
}

std::string TreeReader::fieldText(TSNode node, const char* fieldName) const {
  TSNode c = child(node, fieldName);
  return ts_node_is_null(c) ? std::string() : file_.textOf(c);
}

std::string TreeReader::textOf(TSNode node) const { return file_.textOf(node); }

Span TreeReader::spanOf(TSNode node) const {
  const TSPoint start = ts_node_start_point(node);
  const TSPoint end = ts_node_end_point(node);
  return {file_.path, static_cast<int>(start.row) + 1, static_cast<int>(start.column) + 1, static_cast<int>(end.row) + 1,
          static_cast<int>(end.column) + 1};
}

TSNode TreeReader::root() const { return ts_tree_root_node(file_.tree.get()); }

}  // namespace parse
}  // namespace ds
